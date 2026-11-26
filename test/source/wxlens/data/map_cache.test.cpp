#include <QMapLibre/Map>
#include <QMapLibre/Settings>
#include <QCoreApplication>
#include <QEventLoop>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <gtest/gtest.h>

namespace
{
// A fresh process on each pass proves disk reuse, not a shared in-process file source.
TEST(MapCache, Child)
{
   const QString path = qEnvironmentVariable("WXLENS_TEST_MAP_CACHE");
   if (path.isEmpty()) GTEST_SKIP();
   QMapLibre::Settings settings;
   settings.setCacheDatabasePath(path);
   settings.setCacheDatabaseMaximumSize(256U * 1024U * 1024U);
   QMapLibre::Map map(nullptr, settings, QSize(64, 64));
   QEventLoop loop;
   bool loaded = false;
   QObject::connect(&map, &QMapLibre::Map::mapChanged, &loop, [&](auto change)
   {
      if (change == QMapLibre::Map::MapChangeDidFinishLoadingStyle)
      { loaded = true; loop.quit(); }
   });
   QTimer::singleShot(8000, &loop, &QEventLoop::quit);
   map.setStyleUrl(qEnvironmentVariable("WXLENS_TEST_MAP_URL"));
   loop.exec();
   EXPECT_TRUE(loaded);
   // Allow the asynchronous SQLite write to drain before destroying the process.
   QTimer::singleShot(300, &loop, &QEventLoop::quit);
   loop.exec();
}

TEST(MapCache, ReusesStyleAfterProcessRestartWithoutServer)
{
   QTemporaryDir directory;
   QTcpServer server;
   ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
   int requests = 0;
   QObject::connect(&server, &QTcpServer::newConnection, &server, [&]()
   {
      auto* socket = server.nextPendingConnection();
      QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket]()
      {
         if (!socket->readAll().contains("GET")) return;
         ++requests;
         const QByteArray body = R"({"version":8,"sources":{},"layers":[]})";
         socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nCache-Control: max-age=3600\r\nContent-Length: "
                       + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
         socket->disconnectFromHost();
      });
      QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
   });
   const QString url = QStringLiteral("http://127.0.0.1:%1/style.json").arg(server.serverPort());
   auto run = [&]()
   {
      QProcess process;
      auto environment = QProcessEnvironment::systemEnvironment();
      environment.insert("WXLENS_TEST_MAP_CACHE", directory.filePath("map.db"));
      environment.insert("WXLENS_TEST_MAP_URL", url);
      process.setProcessEnvironment(environment);
      QEventLoop loop;
      QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
      QTimer::singleShot(12000, &loop, &QEventLoop::quit);
      process.start(QCoreApplication::applicationFilePath(), {"--gtest_filter=MapCache.Child"});
      loop.exec();
      if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(); }
      EXPECT_EQ(process.exitStatus(), QProcess::NormalExit) << process.readAllStandardError().toStdString();
      EXPECT_EQ(process.exitCode(), 0) << process.readAllStandardOutput().toStdString();
   };
   run();
   EXPECT_EQ(requests, 1);
   server.close();
   run();
   EXPECT_EQ(requests, 1);
}
} // namespace
