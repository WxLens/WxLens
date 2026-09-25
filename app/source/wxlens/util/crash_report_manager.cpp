#include <wxlens/util/crash_report_manager.hpp>
#include <wxlens/util/crash_report.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>

namespace wxlens
{
namespace util
{
class CrashReportManager::Impl
{
public:
   settings::SettingsStore* store {};
   QString                  logDirectory;
   QString                  dsn;
   QString                  digest;
   QString                  status;
   QJsonObject              event;
   QNetworkAccessManager    network;
   bool                     visible {false};
   bool                     sending {false};
   bool                     sent {false};
};

CrashReportManager::CrashReportManager(settings::SettingsStore& store,
                                       const QString&           logDirectory,
                                       const QString&           dsn,
                                       QObject*                 parent) :
    QObject(parent), p(std::make_unique<Impl>())
{
   p->store        = &store;
   p->logDirectory = logDirectory;
   p->dsn          = dsn;
   scan();
   // macOS may finish writing its .ips shortly after the next launch.
   QTimer::singleShot(5000,
                      this,
                      [this]()
                      {
                         if (!hasReport())
                            scan();
                      });
}

CrashReportManager::~CrashReportManager() = default;
bool CrashReportManager::visible() const
{ return p->visible; }
bool CrashReportManager::hasReport() const
{ return !p->event.isEmpty(); }
bool CrashReportManager::canSend() const
{
   return hasReport() && !p->sending && !p->sent &&
          !SentryEnvelopeUrl(p->dsn).isEmpty();
}
bool CrashReportManager::sending() const
{ return p->sending; }
QString CrashReportManager::reportText() const
{
   return hasReport() ? QString::fromUtf8(QJsonDocument(p->event).toJson()) :
                        QString {};
}
QString CrashReportManager::status() const
{ return p->status; }
QString CrashReportManager::destination() const
{ return SentryEnvelopeUrl(p->dsn).host(); }

void CrashReportManager::scan()
{
   if (p->sending)
      return;
   QJsonObject report;
#if defined(Q_OS_MACOS)
   const QDir directory {QDir::homePath() + "/Library/Logs/DiagnosticReports"};
   const auto files =
      directory.entryInfoList({"WxLens*.ips"}, QDir::Files, QDir::Time);
   for (const auto& file : files)
   {
      if (file.lastModified().daysTo(QDateTime::currentDateTime()) > 14)
         break;
      if (file.size() > 4 * 1024 * 1024)
         continue;
      QFile input {file.absoluteFilePath()};
      if (!input.open(QIODevice::ReadOnly))
         continue;
      report = ParseMacCrashReport(input.read(4 * 1024 * 1024 + 1));
      if (!report.isEmpty())
         break;
   }
#elif defined(Q_OS_WIN)
   QFile input {QDir(p->logDirectory).filePath("wxlens-crash.log")};
   if (input.open(QIODevice::ReadOnly))
   {
      input.seek(qMax(qint64 {0}, input.size() - 512 * 1024));
      report = ParseWindowsCrashReport(input.read(512 * 1024));
   }
#endif
   if (report.isEmpty())
      return;
   const auto digest =
      QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(report).toJson(
                                                      QJsonDocument::Compact),
                                                   QCryptographicHash::Sha256)
                             .toHex());
   if (digest == p->digest)
      return;
   p->digest = digest;
   p->sent =
      p->store->GetString("crash_reporting", "sent_report", {}) == digest;
   const QString exception = report.value("exception")
                                .toObject()
                                .value("type")
                                .toString("native exception");
   p->event = {{"event_id", digest.left(32)},
               {"platform", "other"},
               {"level", "fatal"},
               {"message",
                "WxLens " + report.value("platform").toString() +
                   " crash: " + exception},
               {"extra",
                QJsonObject {
                   {"crash_report", report},
                   {"reporter_version", QCoreApplication::applicationVersion()},
                   {"reporter_os", QSysInfo::prettyProductName()},
                   {"reporter_arch", QSysInfo::currentCpuArchitecture()}}}};
   p->visible =
      !p->sent &&
      p->store->GetString("crash_reporting", "reviewed_report", {}) != digest;
   p->status.clear();
   Q_EMIT changed();
}

void CrashReportManager::open()
{
   scan();
   p->visible = true;
   Q_EMIT changed();
}

void CrashReportManager::dismiss()
{
   if (p->sending)
      return;
   if (hasReport())
   {
      p->store->SetString("crash_reporting", "reviewed_report", p->digest);
      if (!p->store->Save())
      {
         p->status =
            "Could not save your choice. This report may appear again next "
            "time.";
         Q_EMIT changed();
         return;
      }
   }
   p->visible = false;
   Q_EMIT changed();
}

void CrashReportManager::save(const QUrl& url)
{
   if (!hasReport() || !url.isLocalFile())
      return;
   QSaveFile  file {url.toLocalFile()};
   const auto bytes = reportText().toUtf8();
   const bool saved = file.open(QIODevice::WriteOnly) &&
                      file.write(bytes) == bytes.size() && file.commit();
   p->status = saved ?
                  "Crash report saved. Nothing was sent." :
                  "Could not save the crash report. Choose another location.";
   Q_EMIT changed();
}

void CrashReportManager::send()
{
   if (!canSend())
      return;
   // The only network operation in this controller is this explicit user
   // action.
   QNetworkRequest request {SentryEnvelopeUrl(p->dsn)};
   request.setHeader(QNetworkRequest::ContentTypeHeader,
                     "application/x-sentry-envelope");
   request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::ManualRedirectPolicy);
   request.setTransferTimeout(15000);
   p->sending = true;
   p->status  = "Sending crash report…";
   Q_EMIT changed();
   auto* reply = p->network.post(request, MakeCrashEnvelope(p->event, p->dsn));
   reply->setReadBufferSize(4096);
   // Also cap total time (transfer timeout measures inactivity).
   QTimer::singleShot(20000,
                      reply,
                      [reply]()
                      {
                         if (!reply->isFinished())
                            reply->abort();
                      });
   connect(
      reply,
      &QNetworkReply::finished,
      this,
      [this, reply]()
      {
         const int code =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
         p->sending = false;
         if (reply->error() == QNetworkReply::NoError && code >= 200 &&
             code < 300)
         {
            p->sent = true;
            p->store->SetString("crash_reporting", "sent_report", p->digest);
            p->store->SetString(
               "crash_reporting", "reviewed_report", p->digest);
            p->status = p->store->Save() ?
                           "Crash report sent. Thank you." :
                           "Crash report sent, but the confirmation could not "
                           "be saved locally.";
         }
         else
         {
            p->status = code == 429 ?
                           "The free reporting quota or rate limit was "
                           "reached. Save the report or try again later." :
                           "The report could not be sent. It is still "
                           "available to save or retry.";
         }
         reply->deleteLater();
         Q_EMIT changed();
      });
}

} // namespace util
} // namespace wxlens
