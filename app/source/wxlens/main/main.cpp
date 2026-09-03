#include <wxlens/log/logger.hpp>
#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/objects/measurement_controller.hpp>
#include <wxlens/objects/snap_target_registry.hpp>
#include <wxlens/objects/saved_place_manager.hpp>
#include <wxlens/objects/object_tool_controller.hpp>
#include <wxlens/overlays/overlay_manager.hpp>
#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/products/radar_product_status.hpp>
#include <wxlens/settings/app_settings.hpp>
#include <wxlens/settings/settings_store.hpp>
#include <wxlens/theme/theme_manager.hpp>
#include <wxlens/util/crash_handler.hpp>

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
   QGuiApplication::setApplicationName("WxLens");
   QGuiApplication::setOrganizationName("WxLens");
   // The base QtQuick.Window "Window" QML type has no `icon` property (that's an
   // ApplicationWindow/Controls thing) - setting it here covers every window the app creates,
   // and matters on platforms without an embedded .exe icon resource (Linux/macOS).
   QGuiApplication::setWindowIcon(
      QIcon(":/qt/qml/WxLens/App/res/branding/wxlens-icon.png"));

   wxlens::log::Initialize();
   auto logger = wxlens::log::Create(logPrefix_);

   // Installed immediately after logging so it covers the whole process lifetime - including
   // static destruction after main() returns, which is where the known exit-path fault lives
   // (docs/ROADMAP.md, Phase 1 slice 4). Writes its own crash log rather than going through
   // spdlog, whose sinks may already be gone by then.
   wxlens::util::InstallCrashHandler(wxlens::log::LogDirectory());

   // A shutdown that never finishes produces no crash and no output at all, so it has to be
   // caught deliberately: if the process is still alive well after it started quitting, dump
   // every thread's stack so the deadlock is visible instead of just looking frozen.
   QObject::connect(&app,
                    &QGuiApplication::aboutToQuit,
                    &app,
                    []() { wxlens::util::ArmShutdownWatchdog(10); });

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

   wxlens::panes::PaneGridModel paneGridModel;
   paneGridModel.setDefaultSourceKey(QString::fromStdString(kDefaultRadarSite));
   engine.rootContext()->setContextProperty("paneGridModel", &paneGridModel);
   // Family defaults (which palette velocity/reflectivity/... panes use) persist like any other
   // preference; the editor's own drafts deliberately do not (factory palettes are never
   // overwritten - users save a .pal copy instead).
   wxlens::palettes::PaletteManager::Instance().bindSettings(
      wxlens::settings::SettingsStore::Instance());
   engine.rootContext()->setContextProperty(
      "paletteManager", &wxlens::palettes::PaletteManager::Instance());

   // Still a single-site status line in the top bar: a temporary bridge (see RadarProductStatus's
   // own doc comment) that predates the grid and is superseded by per-pane chrome, not extended.
   wxlens::products::RadarProductStatus radarStatus {kDefaultRadarSite};
   engine.rootContext()->setContextProperty("radarStatus", &radarStatus);

   // The unified map-object store and its placement tools (docs/ROADMAP.md §4.3). The store is a
   // process-wide singleton because objects are scoped across panes, not owned by one.
   engine.rootContext()->setContextProperty("mapObjectStore",
                                            &wxlens::objects::MapObjectStore::Instance());

   // The structured config store and its typed accessors (docs/ROADMAP.md §3.2, ADR 0003,
   // slice 17). Constructed before the controllers that read defaults from it.
   wxlens::settings::AppSettings appSettings {wxlens::settings::SettingsStore::Instance()};
   engine.rootContext()->setContextProperty("appSettings", &appSettings);
   wxlens::theme::ThemeManager themeManager {wxlens::settings::SettingsStore::Instance()};
   engine.rootContext()->setContextProperty("themeManager", &themeManager);

   // "Reset to defaults" has to mean every preference, including the palette a product family
   // renders with. Wired here so `settings` stays independent of `palettes` - same direction of
   // coupling as the default object scope below.
   QObject::connect(&appSettings,
                    &wxlens::settings::AppSettings::defaultsReset,
                    &wxlens::palettes::PaletteManager::Instance(),
                    &wxlens::palettes::PaletteManager::resetFamilyDefaults);

   wxlens::objects::SavedPlaceManager savedPlaces {
      wxlens::objects::MapObjectStore::Instance(), wxlens::settings::SettingsStore::Instance()};
   engine.rootContext()->setContextProperty("savedPlaces", &savedPlaces);

   // Warnings and placefiles are shared meteorological overlays. Each pane projects the same
   // geographic model independently, keeping them out of both radar-product state and the user
   // analysis MapObjectStore.
   wxlens::overlays::OverlayManager overlayManager {wxlens::settings::SettingsStore::Instance()};
   engine.rootContext()->setContextProperty("overlayManager", &overlayManager);
   overlayManager.refreshWarnings();

   wxlens::objects::ObjectToolController objectTools;

   // §4.3 forbids baking the default object scope in as a constant - the competing apps genuinely
   // disagree about it and both are right for their users. Wired here rather than by giving
   // ObjectToolController a settings dependency, so `objects` stays independent of `settings` and
   // the direction of the coupling is visible in one place.
   objectTools.setScopeKind(appSettings.defaultObjectScope());
   QObject::connect(&appSettings,
                    &wxlens::settings::AppSettings::defaultObjectScopeChanged,
                    &objectTools,
                    [&appSettings, &objectTools]()
                    { objectTools.setScopeKind(appSettings.defaultObjectScope()); });

   engine.rootContext()->setContextProperty("objectTools", &objectTools);

   wxlens::objects::MeasurementController measurementTool;
   wxlens::objects::SnapTargetRegistry snapTargets;
   engine.rootContext()->setContextProperty("snapTargets", &snapTargets);
   QObject::connect(&appSettings,
                    &wxlens::settings::AppSettings::distanceUnitsChanged,
                    &measurementTool,
                    &wxlens::objects::MeasurementController::refreshFormatting);
   QObject::connect(&appSettings,
                    &wxlens::settings::AppSettings::distanceUnitsChanged,
                    &wxlens::objects::MapObjectStore::Instance(),
                    &wxlens::objects::MapObjectStore::refreshFormatting);
   engine.rootContext()->setContextProperty("measurementTool", &measurementTool);

   engine.loadFromModule("WxLens.App", "Main");

   if (engine.rootObjects().isEmpty())
   {
      logger->error("Failed to load QML application engine");
      return -1;
   }

   const int result = QGuiApplication::exec();

   // NOTE: the process still faults during teardown, after this point, inside MapLibre. It is
   // fully diagnosed rather than mysterious - the crash handler installed above writes the stack
   // to logs/wxlens-crash.log on every occurrence, and docs/ROADMAP.md records the analysis. It
   // is an exit-path fault only: the window is already gone and nothing is left to lose.
   //
   // Gracefully stop the io_context main loop before shutting down the AWS SDK, so no posted
   // work tries to make an S3 call after Aws::ShutdownAPI runs.
   ioContextWork.reset();
   ioThreadPool.join();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
