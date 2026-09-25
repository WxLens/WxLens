#include <wxlens/util/crash_report.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace wxlens
{
namespace util
{
namespace
{
QJsonObject Select(const QJsonObject& input, const QStringList& keys)
{
   QJsonObject result;
   for (const auto& key : keys)
   {
      const auto value = input.value(key);
      if (value.isDouble() || value.isBool())
      {
         result.insert(key, value);
      }
      else if (value.isString())
      {
         // No arbitrary paths, URLs, email addresses or multiline strings even
         // inside an otherwise allowed field. Native symbol names are retained.
         QString text = value.toString().left(512);
         if (text.contains('/') || text.contains('\\') || text.contains('@') ||
             text.contains('\n') || text.contains('\r'))
         {
            text = QStringLiteral("[omitted]");
         }
         result.insert(key, text);
      }
   }
   return result;
}

QJsonArray Frames(const QJsonArray& input)
{
   QJsonArray result;
   for (const auto& value : input)
   {
      if (result.size() == 128)
         break;
      result.append(
         Select(value.toObject(),
                {"symbol", "symbolLocation", "imageIndex", "imageOffset"}));
   }
   return result;
}
} // namespace

QJsonObject ParseMacCrashReport(const QByteArray& data)
{
   if (data.size() > 4 * 1024 * 1024)
      return {};
   // .ips has a one-line metadata object followed by the crash data object.
   auto document = QJsonDocument::fromJson(data);
   if (!document.isObject())
      document = QJsonDocument::fromJson(data.mid(data.indexOf('\n') + 1));
   const auto input = document.object();
   if (input.value("procName").toString() != "WxLens" ||
       input.value("bundleInfo")
             .toObject()
             .value("CFBundleIdentifier")
             .toString() != "org.wxlens.WxLens" ||
       !input.value("exception").isObject())
      return {};

   auto result =
      Select(input, {"captureTime", "cpuType", "translated", "faultingThread"});
   result.insert("platform", "macOS");
   result.insert("bundleInfo",
                 Select(input.value("bundleInfo").toObject(),
                        {"CFBundleShortVersionString", "CFBundleVersion"}));
   result.insert(
      "osVersion",
      Select(input.value("osVersion").toObject(), {"train", "build"}));
   result.insert(
      "exception",
      Select(input.value("exception").toObject(), {"type", "signal", "codes"}));
   result.insert("termination",
                 Select(input.value("termination").toObject(),
                        {"namespace", "code", "indicator"}));
   QJsonArray threads;
   for (const auto& value : input.value("threads").toArray())
   {
      if (threads.size() == 128)
         break;
      const auto thread   = value.toObject();
      auto       selected = Select(thread, {"triggered"});
      selected.insert("frames", Frames(thread.value("frames").toArray()));
      threads.append(selected);
   }
   result.insert("threads", threads);
   result.insert("lastExceptionBacktrace",
                 Frames(input.value("lastExceptionBacktrace").toArray()));
   QJsonArray images;
   for (const auto& value : input.value("usedImages").toArray())
   {
      if (images.size() == 512)
         break;
      images.append(
         Select(value.toObject(), {"name", "uuid", "arch", "base", "size"}));
   }
   result.insert("usedImages", images);
   return result;
}

QJsonObject ParseWindowsCrashReport(const QByteArray& data)
{
   const QString text  = QString::fromUtf8(data.right(512 * 1024));
   const auto    end   = text.lastIndexOf("=== end crash ===");
   const auto    start = text.lastIndexOf("=== wxlens-app crash ", end);
   if (start < 0 || end < start)
      return {};
   QStringList selected;
   // Only the exception header and symbolized frames, never source-path lines
   // or the shutdown-watchdog thread dumps in the same file.
   const QRegularExpression header {
      "^(=== wxlens-app crash [0-9 :\\-]+ ===|exception : 0x[0-9a-fA-F]+ "
      "\\([A-Za-z_ ]+\\)|version   : [0-9A-Za-z.+-]+)$"};
   const QRegularExpression frame {"^  \\[[0-9]+\\] [^/\\\\@\\r\\n]+$"};
   for (const auto& line : text.mid(start, end - start).split('\n'))
   {
      QString clean = line;
      if (clean.endsWith('\r'))
         clean.chop(1);
      if (header.match(clean).hasMatch() || frame.match(clean).hasMatch())
         selected.append(clean.left(1024));
   }
   if (selected.size() < 2)
      return {};
   const QRegularExpression exception {
      "exception : (0x[0-9a-fA-F]+) \\(([A-Za-z_ ]+)\\)"};
   const auto match = exception.match(selected.join('\n'));
   if (!match.hasMatch())
      return {};
   return {{"platform", "Windows"},
           {"stack", selected.join('\n')},
           {"exception",
            QJsonObject {{"type", match.captured(2)},
                         {"codes", match.captured(1)}}}};
}

QUrl SentryEnvelopeUrl(const QString& dsn)
{
   const QUrl               source {dsn, QUrl::StrictMode};
   const QRegularExpression host {
      "^(o[0-9]+\\.ingest(\\.[a-z]+)?\\.)?sentry\\.io$"};
   const QRegularExpression key {"^[0-9a-fA-F]{32}$"};
   const QRegularExpression project {"^/[0-9]+$"};
   if (!source.isValid() || source.scheme() != "https" ||
       !host.match(source.host()).hasMatch() ||
       !key.match(source.userName()).hasMatch() ||
       !project.match(source.path()).hasMatch() ||
       !source.password().isEmpty() || source.hasQuery() ||
       source.hasFragment() || source.port() != -1)
      return {};
   QUrl endpoint;
   endpoint.setScheme("https");
   endpoint.setHost(source.host());
   endpoint.setPath("/api" + source.path() + "/envelope/");
   return endpoint;
}

QByteArray MakeCrashEnvelope(const QJsonObject& event, const QString& dsn)
{
   if (SentryEnvelopeUrl(dsn).isEmpty())
      return {};
   const auto payload = QJsonDocument(event).toJson(QJsonDocument::Compact);
   const QJsonObject header {{"event_id", event.value("event_id")},
                             {"dsn", dsn}};
   const QJsonObject item {{"type", "event"}, {"length", payload.size()}};
   return QJsonDocument(header).toJson(QJsonDocument::Compact) + '\n' +
          QJsonDocument(item).toJson(QJsonDocument::Compact) + '\n' + payload +
          '\n';
}

} // namespace util
} // namespace wxlens
