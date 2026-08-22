#include <nimbus/log/logger.hpp>
#include <nimbus/panes/pane_grid_model.hpp>
#include <nimbus/products/radar_product_status.hpp>

#include <scwx/util/threads.hpp>

#include <aws/core/Aws.h>
#include <boost/asio.hpp>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QSurfaceFormat>

static const std::string logPrefix_ = "main";

int main(int argc, char* argv[])
{
   QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

   // Without multisampling, RadarSweepLayer's per-gate triangles (many of them sub-pixel-sized at
   // typical zoom levels) get raw point-sampled by the rasterizer, producing a speckled/mottled
   // look instead of solid filled wedges - confirmed by a real launch against live KEAX data
   // (docs/ROADMAP.md §7 Phase 1 slice 3). Matches the legacy app's own fix for the same problem
   // (scwx-qt/source/scwx/qt/map/map_widget.cpp: `surfaceFormat.setSamples(4)`). Must be set
   // before QGuiApplication creates any window/GL context, so this has to run before the
   // QGuiApplication constructor below, not later via QQuickWindow::setFormat() on the QML window.
   QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
   surfaceFormat.setSamples(4);
   QSurfaceFormat::setDefaultFormat(surfaceFormat);

   QGuiApplication app(argc, argv);
   QGuiApplication::setApplicationName("Nimbus");
   QGuiApplication::setOrganizationName("Nimbus");
   // The base QtQuick.Window "Window" QML type has no `icon` property (that's an
   // ApplicationWindow/Controls thing) - setting it here covers every window the app creates,
   // and matters on platforms without an embedded .exe icon resource (Linux/macOS).
   QGuiApplication::setWindowIcon(
      QIcon(":/qt/qml/Nimbus/App/res/branding/nimbus-icon.png"));

   nimbus::log::Initialize();
   auto logger = nimbus::log::Create(logPrefix_);

   // Start scwx::util::io_context() actually running - wxdata only defines it and the async()
   // helper (docs/ROADMAP.md §7 Phase 1 slice 2's RadarSiteDataService uses scwx::util::async
   // indirectly via wxdata's providers), the consuming app is responsible for running it. Ported
   // from the legacy app's main.cpp; the try/catch-and-continue loop matches its handling of
   // exceptions escaping posted work.
   boost::asio::io_context& ioContext = scwx::util::io_context();
   auto                     ioContextWork = boost::asio::make_work_guard(ioContext);
   boost::asio::thread_pool ioThreadPool {4};
   boost::asio::post(ioThreadPool,
                     [&]()
                     {
                        while (true)
                        {
                           try
                           {
                              ioContext.run();
                              break;
                           }
                           catch (const std::exception& ex)
                           {
                              logger->error(ex.what());
                           }
                        }
                     });

   // Required before any AWS S3 call (RadarSiteDataService's Level 2 provider uses one) - not
   // previously needed since nothing in the app made network requests through the AWS SDK yet.
   const Aws::SDKOptions awsSdkOptions {};
   Aws::InitAPI(awsSdkOptions);

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
   // The site every new pane starts on. Per-pane site selection is pane chrome (docs/ROADMAP.md
   // §4.5), which lands with slice 5 - until then PaneGridModel hands this to each pane it
   // creates, and the grid itself is real (slice 4), not a hardcoded single view.
   static const std::string kDefaultRadarSite = "KEAX";

   nimbus::panes::PaneGridModel paneGridModel;
   paneGridModel.setDefaultSourceKey(QString::fromStdString(kDefaultRadarSite));
   engine.rootContext()->setContextProperty("paneGridModel", &paneGridModel);

   // Still a single-site status line in the top bar: a temporary bridge (see RadarProductStatus's
   // own doc comment) that predates the grid and is superseded by per-pane chrome, not extended.
   nimbus::products::RadarProductStatus radarStatus {kDefaultRadarSite};
   engine.rootContext()->setContextProperty("radarStatus", &radarStatus);

   engine.loadFromModule("Nimbus.App", "Main");

   if (engine.rootObjects().isEmpty())
   {
      logger->error("Failed to load QML application engine");
      return -1;
   }

   const int result = QGuiApplication::exec();

   // Gracefully stop the io_context main loop before shutting down the AWS SDK, so no posted
   // work tries to make an S3 call after Aws::ShutdownAPI runs.
   ioContextWork.reset();
   ioThreadPool.join();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
