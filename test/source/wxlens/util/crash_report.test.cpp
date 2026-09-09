#include <wxlens/util/crash_report.hpp>
#include <wxlens/util/crash_report_manager.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <gtest/gtest.h>

namespace wxlens
{
namespace util
{
namespace
{
const QString kDsn =
   "https://0123456789abcdef0123456789abcdef@o123.ingest.us.sentry.io/456";

TEST(CrashReport, MacReportPreservesFramesWithoutPersonalFields)
{
   const QByteArray raw = R"({"bug_type":"309","crashReporterKey":"private-id"}
{"procName":"WxLens","bundleInfo":{"CFBundleIdentifier":"org.wxlens.WxLens","CFBundleVersion":"1.2.3"},
"procPath":"/Users/Alice/WxLens.app", "userID":501,"crashReporterKey":"private-id",
"exception":{"type":"EXC_BAD_ACCESS","signal":"SIGSEGV","message":"secret password"},
"termination":{"namespace":"DYLD","code":1,"details":"/Users/Alice/private"},
"osVersion":{"train":"macOS 15.5","build":"24F74"},
"threads":[{"name":"Alice location 35,-97","triggered":true,"frames":[{"symbol":"mbgl::render", "imageOffset":42,"imageIndex":0,"private":"secret password"}]}],
"usedImages":[{"name":"WxLens","uuid":"image-uuid","base":4096,"path":"/Users/Alice/WxLens.app"}],
"asi":{"message":"https://host/?token=secret password"}})";
   const auto       report = ParseMacCrashReport(raw);
   ASSERT_FALSE(report.isEmpty());
   EXPECT_EQ(report.value("threads")
                .toArray()[0]
                .toObject()
                .value("frames")
                .toArray()[0]
                .toObject()
                .value("imageOffset")
                .toInt(),
             42);
   const auto output = QJsonDocument(report).toJson();
   EXPECT_FALSE(output.contains("Alice"));
   EXPECT_FALSE(output.contains("private-id"));
   EXPECT_FALSE(output.contains("secret password"));
   EXPECT_TRUE(output.contains("image-uuid"));
   EXPECT_TRUE(output.contains("mbgl::render"));
}

TEST(CrashReport, MacRejectsOtherAppsMalformedAndOversizedReports)
{
   EXPECT_TRUE(ParseMacCrashReport("not JSON").isEmpty());
   EXPECT_TRUE(ParseMacCrashReport(R"({"procName":"OtherApp","exception":{}})")
                  .isEmpty());
   EXPECT_TRUE(
      ParseMacCrashReport(QByteArray(4 * 1024 * 1024 + 1, 'x')).isEmpty());
}

TEST(CrashReport, MacRejectsPathsEvenInsideAllowedFields)
{
   const auto report = ParseMacCrashReport(
      R"({"procName":"WxLens", "bundleInfo":{"CFBundleIdentifier":"org.wxlens.WxLens"},"exception":{"type":"EXC_CRASH"},"termination":{"indicator":"/Users/Alice/private"},"threads":[{"frames":[{"symbol":"https://private/secret"}]}]})");
   ASSERT_FALSE(report.isEmpty());
   EXPECT_FALSE(QJsonDocument(report).toJson().contains("private"));
}

TEST(CrashReport, WindowsUsesLatestCrashAndOmitsSourcePathsAndWatchdog)
{
   const QByteArray raw =
      "=== wxlens-app crash 2026-09-08 12:00:00 ===\n"
      "exception : 0xc0000005 (EXCEPTION_ACCESS_VIOLATION)\n"
      "  [00] old.dll + 0x123\n=== end crash ===\n"
      "=== wxlens-app crash 2026-09-09 12:00:00 ===\r\n"
      "exception : 0xc0000005 (EXCEPTION_ACCESS_VIOLATION)\r\n"
      "  [00] Qt6Gui.dll!render + 0x42\r\n"
      "       C:\\Users\\Alice\\secret.cpp:12\r\n"
      "=== end crash ===\n"
      "=== wxlens-app thread dump 2026-09-09 ===\nreason: watchdog secret\n";
   const auto report = ParseWindowsCrashReport(raw);
   ASSERT_FALSE(report.isEmpty());
   const auto output = QJsonDocument(report).toJson();
   EXPECT_TRUE(output.contains("Qt6Gui.dll"));
   EXPECT_FALSE(output.contains("Alice"));
   EXPECT_FALSE(output.contains("old.dll"));
   EXPECT_FALSE(output.contains("watchdog"));
   EXPECT_EQ(ParseWindowsCrashReport(
                raw + "=== wxlens-app crash 2026-09-10 12:00:00 ===\n"),
             report);
   EXPECT_TRUE(
      ParseWindowsCrashReport("=== wxlens-app thread dump ===").isEmpty());
   EXPECT_TRUE(
      ParseWindowsCrashReport("=== wxlens-app crash 2026-09-09 ===").isEmpty());
}

TEST(CrashReport, OnlyPublicHostedSentryHttpsDsnsAreAccepted)
{
   EXPECT_EQ(SentryEnvelopeUrl(kDsn).toString(),
             "https://o123.ingest.us.sentry.io/api/456/envelope/");
   for (const auto& dsn :
        {"http://key@host/1",
         "https://secret@evil.invalid/123",
         "https://0123456789abcdef0123456789abcdef:secret@sentry.io/1",
         "https://0123456789abcdef0123456789abcdef@sentry.io/1?token=secret",
         "https://0123456789abcdef0123456789abcdef@sentry.io.evil.invalid/1",
         "https://0123456789abcdef0123456789abcdef@sentry.io/1#secret"})
      EXPECT_TRUE(SentryEnvelopeUrl(dsn).isEmpty()) << dsn;
}

TEST(CrashReport, EnvelopeLengthIsUtf8BytesAndPayloadMatchesPreview)
{
   const QJsonObject event {{"event_id", "0123456789abcdef0123456789abcdef"},
                            {"message", QString::fromUtf8("Crash — reviewed")}};
   const auto        envelope = MakeCrashEnvelope(event, kDsn);
   const auto        lines    = envelope.split('\n');
   ASSERT_EQ(lines.size(), 4);
   EXPECT_EQ(QJsonDocument::fromJson(lines[0]).object().value("dsn").toString(),
             kDsn);
   EXPECT_EQ(
      QJsonDocument::fromJson(lines[1]).object().value("length").toInteger(),
      lines[2].size());
   EXPECT_EQ(QJsonDocument::fromJson(lines[2]).object(), event);
   EXPECT_TRUE(MakeCrashEnvelope(event, "").isEmpty());
}

#if defined(Q_OS_WIN)
TEST(CrashReport, ReviewAndSaveDoNotSendAndDismissalPersists)
{
   QTemporaryDir directory;
   ASSERT_TRUE(directory.isValid());
   settings::SettingsStore store;
   store.SetConfigDirectory(directory.path() + "/settings");
   QFile source {directory.filePath("wxlens-crash.log")};
   ASSERT_TRUE(source.open(QIODevice::WriteOnly));
   source.write(
      "=== wxlens-app crash 2026-09-09 12:00:00 ===\n"
      "exception : 0xc0000005 (EXCEPTION_ACCESS_VIOLATION)\n"
      "  [00] WxLens!render + 0x42\n=== end crash ===\n");
   source.close();
   {
      CrashReportManager manager {store, directory.path(), kDsn};
      ASSERT_TRUE(manager.visible());
      ASSERT_TRUE(manager.hasReport());
      EXPECT_TRUE(manager.canSend());
      EXPECT_FALSE(manager.sending());
      EXPECT_TRUE(manager.status().isEmpty());
      const auto output = directory.filePath("reviewed.json");
      manager.save(QUrl::fromLocalFile(output));
      QFile saved {output};
      ASSERT_TRUE(saved.open(QIODevice::ReadOnly));
      EXPECT_EQ(saved.readAll(), manager.reportText().toUtf8());
      EXPECT_FALSE(manager.sending());
      EXPECT_TRUE(manager.canSend());
      manager.dismiss();
      EXPECT_FALSE(manager.visible());
   }
   store.Reload();
   CrashReportManager reopened {store, directory.path(), kDsn};
   EXPECT_FALSE(reopened.visible());
   EXPECT_TRUE(reopened.hasReport());
   reopened.open();
   EXPECT_TRUE(reopened.visible());
}

TEST(CrashReport, NoReportOrDisabledDestinationCannotSend)
{
   QTemporaryDir           directory;
   settings::SettingsStore store;
   store.SetConfigDirectory(directory.path());
   CrashReportManager manager {store, directory.path(), {}};
   EXPECT_FALSE(manager.hasReport());
   EXPECT_FALSE(manager.canSend());
   manager.send();
   EXPECT_FALSE(manager.sending());
}
#endif
} // namespace
} // namespace util
} // namespace wxlens
