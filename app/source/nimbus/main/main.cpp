#include <nimbus/log/logger.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickWindow>

static const std::string logPrefix_ = "main";

int main(int argc, char* argv[])
{
   QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

   QGuiApplication app(argc, argv);
   QGuiApplication::setApplicationName("Nimbus");
   QGuiApplication::setOrganizationName("Nimbus");

   nimbus::log::Initialize();
   auto logger = nimbus::log::Create(logPrefix_);

   QQmlApplicationEngine engine;
   QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreationFailed,
      &app,
      []() { QCoreApplication::exit(-1); },
      Qt::QueuedConnection);
   QObject::connect(&engine,
                     &QQmlApplicationEngine::warnings,
                     &app,
                     [logger](const QList<QQmlError>& warnings)
                     {
                        for (const auto& w : warnings)
                        {
                           logger->error("QML warning: {}", w.toString().toStdString());
                        }
                     });
   engine.loadFromModule("Nimbus.App", "Main");

   if (engine.rootObjects().isEmpty())
   {
      logger->error("Failed to load QML application engine");
      return -1;
   }

   return QGuiApplication::exec();
}
