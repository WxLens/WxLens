#include <wxlens/data/radar_site_marker_source.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/objects/measurement_controller.hpp>
#include <wxlens/objects/snap_target_registry.hpp>
#include <wxlens/objects/saved_place_manager.hpp>
#include <wxlens/objects/object_tool_controller.hpp>
#include <wxlens/overlays/overlay_manager.hpp>
#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/palettes/palette_manager.hpp>
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
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSurfaceFormat>

#include <algorithm>

static const std::string logPrefix_ = "main";

namespace
{
/**
 * Shrinks and repositions the main window so its whole frame fits the screen it is on.
 *
 * Main.qml asks for 1280x800, which is a device-independent size: at increased text scaling the
 * window becomes far larger than the desktop. At 150 % it opened 2418x1427 on a 2066x1238 screen
 * and put the entire floating control bar under the taskbar - LIVE/Archive, the time field, the
 * site, product and tilt controls and the layout selector were all unreachable, with no way to
 * scroll or drag them back.
 *
 * Doing this from QML did not work. `Screen.desktopAvailableWidth/Height` there did not compare
 * against the window's own size in the units the size is expressed in, so the clamp silently did
 * nothing while its reposition branch still ran. QWindow::geometry() and QScreen::availableGeometry()
 * are both device-independent pixels, so the arithmetic here is unambiguous - and the sizes are
 * logged, so a future scaling report can be checked against real numbers instead of inference.
 *
 * Qt clamps the resize to the window's minimum size, so a screen smaller than that minimum still
 * yields the smallest window the layout supports rather than a broken one.
 */
void FitWindowToScreen(QWindow* window, const std::shared_ptr<spdlog::logger>& logger)
{
   const QScreen* screen = window->screen();
   if (screen == nullptr) return;

   const QRect available = screen->availableGeometry();
   const QMargins frame  = window->frameMargins();
   const int maxWidth    = available.width() - frame.left() - frame.right();
   const int maxHeight   = available.height() - frame.top() - frame.bottom();
   const int width       = std::min(window->width(), maxWidth);
   const int height      = std::min(window->height(), maxHeight);

   if (width != window->width() || height != window->height())
   {
      logger->info("Window {}x{} exceeds the {}x{} available on \"{}\"; fitting to {}x{}",
                   window->width(), window->height(), maxWidth, maxHeight,
                   screen->name().toStdString(), width, height);
      window->resize(width, height);
   }

   // Shrinking keeps the top-left corner, which can still leave the frame hanging off an edge.
   const QRect frameGeometry = window->frameGeometry();
   const int   x = std::clamp(frameGeometry.x(), available.left(),
                              std::max(available.left(), available.right() - frameGeometry.width() + 1));
   const int   y = std::clamp(frameGeometry.y(), available.top(),
                              std::max(available.top(), available.bottom() - frameGeometry.height() + 1));
   if (x != frameGeometry.x() || y != frameGeometry.y()) window->setFramePosition(QPoint(x, y));
}
} // namespace

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

   // Qt Quick Controls defaults to the platform's native style on Windows, and a native style
   // *silently refuses* every contentItem/background/indicator customization - the controls in
   // OverlaysDialog were being drawn as native Windows widgets no matter what the theme said, and
   // Qt only says so through a qWarning nothing was capturing. Basic is the neutral, fully
   // customizable style, which is what this app wants: every other control it draws is a themed
   // Rectangle, so there is no native look to preserve.
   QQuickStyle::setStyle(QStringLiteral("Basic"));

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

   // The unified map-object store and its placement tools (docs/ROADMAP.md §4.3). The store is a
   // process-wide singleton because objects are scoped across panes, not owned by one.
   engine.rootContext()->setContextProperty("mapObjectStore",
                                            &wxlens::objects::MapObjectStore::Instance());

   // The structured config store and its typed accessors (docs/ROADMAP.md §3.2, ADR 0003,
   // slice 17). Constructed before the controllers that read defaults from it.
   wxlens::settings::AppSettings appSettings {wxlens::settings::SettingsStore::Instance()};
   engine.rootContext()->setContextProperty("appSettings", &appSettings);
   paneGridModel.setCenterMapOnSiteChange(appSettings.centerMapOnSiteChange());
   paneGridModel.setRadarSiteScope(appSettings.radarSiteScope());
   QObject::connect(&appSettings, &wxlens::settings::AppSettings::centerMapOnSiteChangeChanged,
                    &paneGridModel, [&]() { paneGridModel.setCenterMapOnSiteChange(
                                             appSettings.centerMapOnSiteChange()); });
   QObject::connect(&appSettings, &wxlens::settings::AppSettings::radarSiteScopeChanged,
                    &paneGridModel, [&]() { paneGridModel.setRadarSiteScope(
                                             appSettings.radarSiteScope()); });
   wxlens::data::RadarSiteMarkerSource radarSiteMarkers;
   engine.rootContext()->setContextProperty("radarSiteMarkers", &radarSiteMarkers);
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

   if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()))
   {
      FitWindowToScreen(window, logger);
      // A window dragged to a smaller screen has the same problem, so re-fit on every move.
      QObject::connect(window, &QWindow::screenChanged, window,
                       [window, logger](QScreen*) { FitWindowToScreen(window, logger); });
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
