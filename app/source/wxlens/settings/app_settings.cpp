#include <wxlens/settings/app_settings.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/objects/map_object.hpp>
#include <wxlens/settings/settings_store.hpp>
#include <wxlens/util/unit_format.hpp>

#include <array>
#include <map>

#include <QVariantMap>

namespace wxlens
{
namespace settings
{

static const std::string logPrefix_ = "settings.app_settings";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

namespace
{

// One TOML file per category, per ADR 0003.
const QString kMeasurementCategory = QStringLiteral("measurement");
const QString kObjectsCategory     = QStringLiteral("objects");
const QString kUnitsCategory       = QStringLiteral("units");
const QString kRadarCategory       = QStringLiteral("radar");
const QString kAppearanceCategory  = QStringLiteral("appearance");
const QString kMapCategory         = QStringLiteral("map");

struct MapDetailGroup
{
   const char* id;
   const char* label;
};

constexpr std::array<MapDetailGroup, 7> kMapDetailGroups {{{"roads", "Roads"},
                                                            {"places", "City and town labels"},
                                                            {"boundaries", "Boundaries"},
                                                            {"buildings", "Buildings"},
                                                            {"poi", "Points of interest"},
                                                            {"water", "Water labels"},
                                                            {"terrain", "Terrain / hillshade"}}};

constexpr std::array<bool, 7> kOperationalMapDetails {{true, true, true, false, false, true, true}};
constexpr std::array<bool, 7> kMinimalMapDetails {{false, true, true, false, false, true, false}};
constexpr std::array<bool, 7> kDetailedMapDetails {{true, true, true, true, true, true, true}};

/**
 * The §4.7 geometry-row catalogue. `alwaysDefaultVisible` is not decoration: §4.7 requires the app
 * to state that terrain data is unavailable rather than omit the field, because a reader who
 * expects an AGL figure would otherwise take the MSL number for one. Letting a user hide those
 * rows after they have been told is fine; shipping them hidden is the silent omission that rule
 * exists to prevent, so they default on and the settings UI says why.
 */
struct GeometryRow
{
   const char* id;
   const char* label;
   const char* note; ///< shown in the settings UI; empty for ordinary rows

   /// Which key of PaneController::probeSourceAt's result this row displays. Carried here so the
   /// QML readout needs no id-to-key mapping of its own - one catalogue defines the rows, their
   /// labels, their values and their visibility, and cannot drift out of step with itself.
   const char* valueKey;

   /// When the value is a statement about what is *not* known rather than a figure, so the readout
   /// can render it differently: "never", "whenElevationUnknown", or "always".
   const char* dimWhen;
};

constexpr std::array<GeometryRow, 7> kGeometryRows {{
   {"range", "Range", "", "rangeText", "never"},
   {"azimuth", "Azimuth", "", "azimuthText", "never"},
   {"elevationAngle", "Elevation angle", "", "elevationAngleText", "whenElevationUnknown"},
   {"beamMsl", "Beam centre (MSL)", "", "beamCenterMslText", "whenElevationUnknown"},
   {"beamArl", "Above radar", "", "beamCenterArlText", "whenElevationUnknown"},
   {"terrain",
    "Terrain (MSL)",
    "Shown by default so the beam altitude is never mistaken for a height above ground.",
    "terrainText",
    "always"},
   {"beamAgl",
    "Beam height (AGL)",
    "Shown by default: hiding it silently is what makes an MSL figure look like an AGL one.",
    "beamCenterAglText",
    "always"},
}};

struct Section
{
   const char* id;
   const char* title;
   const char* summary;
};

/// Stable ids (§4.5). Changing one breaks every deep-link that points at it, so treat these as
/// part of the app's contract rather than as labels.
constexpr std::array<Section, 8> kSections {{
   {"appearance", "Appearance", "Choose the chrome theme used throughout WxLens."},
   {"toolbar", "Toolbar", "Choose optional shortcuts shown beside the complete Tools menu."},
   {"map-details", "Map details", "Choose which geographic context appears beneath data."},
   {"radar-sites", "Radar sites", "Choose how pane cameras respond when a radar site changes."},
   {"measurement", "Measurement", "How measurements are started and finished."},
   {"objects", "Map objects", "Defaults for markers, range rings and pinned measurements."},
   {"units", "Units", "How distances and altitudes are displayed."},
   {"radar-geometry", "Radar geometry", "Which rows the beam-height readout shows."},
}};

struct ToolbarAction { const char* id; const char* label; };
constexpr std::array<ToolbarAction, 4> kToolbarActions {{{"overlays", "Weather overlays"},
                                                          {"places", "Saved places"},
                                                          {"map", "Map details"},
                                                          {"palette", "Palette manager"}}};

constexpr int kMeasurementGestureMax = static_cast<int>(AppSettings::MeasurementGesture::ClickOnly);
constexpr int kPreferredMeasurementToolMax = 2;
constexpr int kSnapStrengthMax = static_cast<int>(AppSettings::SnapStrength::Strong);
constexpr int kDistanceUnitsMax      = static_cast<int>(AppSettings::DistanceUnits::Imperial);
constexpr int kMapThemeMax           = static_cast<int>(AppSettings::MapTheme::Light);
constexpr int kMapDetailsPresetMax = static_cast<int>(AppSettings::MapDetailsPreset::Custom);
constexpr int kScopeKindMax = static_cast<int>(objects::MapObjectScopeKind::AllPanes);

} // namespace

class AppSettings::Impl
{
public:
   explicit Impl(SettingsStore& store) : store_ {store} {}

   void Load();

   /// Keeps util::unit_format in step with the stored preference. unit_format deliberately does
   /// not depend on settings (it is a Qt-only formatting util used by tests without a config
   /// store), so the dependency runs this way round: settings pushes into it.
   void ApplyDistanceUnits() const
   {
      util::SetDistanceUnitPreference(
         static_cast<util::DistanceUnitPreference>(distanceUnits_));
   }

   SettingsStore& store_;

   int measurementGesture_ {static_cast<int>(MeasurementGesture::Both)};
   int preferredMeasurementTool_ {1};
   int snapStrength_ {static_cast<int>(SnapStrength::Subtle)};
   int defaultObjectScope_ {static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly)};
   int distanceUnits_ {static_cast<int>(DistanceUnits::Both)};
   int mapTheme_ {static_cast<int>(MapTheme::FollowChrome)};
   bool controlBarDocked_ {false};
   bool centerMapOnSiteChange_ {true};
   int mapDetailsPreset_ {static_cast<int>(MapDetailsPreset::Operational)};
   std::map<QString, bool> mapDetailVisible_ {};

   std::map<QString, bool> geometryRowVisible_ {};
   std::map<QString, bool> toolbarActionVisible_ {};
};

void AppSettings::Impl::Load()
{
   measurementGesture_ = store_.GetInt(kMeasurementCategory,
                                       QStringLiteral("gesture"),
                                       static_cast<int>(MeasurementGesture::Both),
                                       0,
                                       kMeasurementGestureMax);
   preferredMeasurementTool_ = store_.GetInt(
      kMeasurementCategory, QStringLiteral("preferred_tool"), 1, 1, kPreferredMeasurementToolMax);
   snapStrength_ = store_.GetInt(kMeasurementCategory,
                                 QStringLiteral("snap_strength"),
                                 static_cast<int>(SnapStrength::Subtle),
                                 0,
                                 kSnapStrengthMax);

   defaultObjectScope_ =
      store_.GetInt(kObjectsCategory,
                    QStringLiteral("default_scope"),
                    static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly),
                    0,
                    kScopeKindMax);

   distanceUnits_ = store_.GetInt(kUnitsCategory,
                                  QStringLiteral("distance"),
                                  static_cast<int>(DistanceUnits::Both),
                                  0,
                                  kDistanceUnitsMax);

   mapTheme_ = store_.GetInt(kAppearanceCategory,
                             QStringLiteral("map_theme"),
                             static_cast<int>(MapTheme::FollowChrome),
                             0,
                             kMapThemeMax);
   controlBarDocked_ = store_.GetBool(
      kAppearanceCategory, QStringLiteral("control_bar_docked"), false);
   centerMapOnSiteChange_ =
      store_.GetBool(kRadarCategory, QStringLiteral("center_map_on_site_change"), true);
   toolbarActionVisible_.clear();
   for (const ToolbarAction& action : kToolbarActions)
   {
      const QString id = QString::fromLatin1(action.id);
      toolbarActionVisible_[id] =
         store_.GetBool(kAppearanceCategory, QStringLiteral("show_toolbar_") + id, false);
   }

   mapDetailsPreset_ = store_.GetInt(kMapCategory,
                                     QStringLiteral("details_preset"),
                                     static_cast<int>(MapDetailsPreset::Operational),
                                     0,
                                     kMapDetailsPresetMax);
   mapDetailVisible_.clear();
   for (std::size_t i = 0; i < kMapDetailGroups.size(); ++i)
   {
      const QString id = QString::fromLatin1(kMapDetailGroups[i].id);
      mapDetailVisible_[id] =
         store_.GetBool(kMapCategory, QStringLiteral("show_") + id, kOperationalMapDetails[i]);
   }

   geometryRowVisible_.clear();
   for (const GeometryRow& row : kGeometryRows)
   {
      const QString id = QString::fromLatin1(row.id);
      geometryRowVisible_[id] =
         store_.GetBool(kRadarCategory, QStringLiteral("show_") + id, true);
   }

   ApplyDistanceUnits();
}

AppSettings::AppSettings(SettingsStore& store, QObject* parent) :
    QObject(parent), p {std::make_unique<Impl>(store)}
{
   p->Load();
   logger_->info("Settings loaded from {}", store.ConfigDirectory().toStdString());
}

AppSettings::~AppSettings() = default;

int AppSettings::measurementGesture() const
{
   return p->measurementGesture_;
}
int AppSettings::preferredMeasurementTool() const { return p->preferredMeasurementTool_; }

int AppSettings::snapStrength() const { return p->snapStrength_; }

double AppSettings::snapTolerancePixels() const
{
   switch (static_cast<SnapStrength>(p->snapStrength_))
   {
   case SnapStrength::Off: return 0.0;
   case SnapStrength::Strong: return 18.0;
   case SnapStrength::Subtle:
   default: return 10.0;
   }
}

int AppSettings::defaultObjectScope() const
{
   return p->defaultObjectScope_;
}

int AppSettings::distanceUnits() const
{
   return p->distanceUnits_;
}

int AppSettings::mapTheme() const
{
   return p->mapTheme_;
}

bool AppSettings::controlBarDocked() const { return p->controlBarDocked_; }
bool AppSettings::centerMapOnSiteChange() const { return p->centerMapOnSiteChange_; }

int AppSettings::mapDetailsPreset() const
{
   return p->mapDetailsPreset_;
}

void AppSettings::setMeasurementGesture(int gesture)
{
   if (gesture < 0 || gesture > kMeasurementGestureMax || gesture == p->measurementGesture_)
   {
      return;
   }

   p->measurementGesture_ = gesture;
   p->store_.SetInt(kMeasurementCategory, QStringLiteral("gesture"), gesture);
   p->store_.Save();

   Q_EMIT measurementGestureChanged();
}

void AppSettings::setPreferredMeasurementTool(int tool)
{
   if (tool < 1 || tool > kPreferredMeasurementToolMax || tool == p->preferredMeasurementTool_)
      return;
   p->preferredMeasurementTool_ = tool;
   p->store_.SetInt(kMeasurementCategory, QStringLiteral("preferred_tool"), tool);
   p->store_.Save();
   Q_EMIT preferredMeasurementToolChanged();
}

void AppSettings::setSnapStrength(int strength)
{
   if (strength < 0 || strength > kSnapStrengthMax || strength == p->snapStrength_)
   {
      return;
   }
   p->snapStrength_ = strength;
   p->store_.SetInt(kMeasurementCategory, QStringLiteral("snap_strength"), strength);
   p->store_.Save();
   Q_EMIT snapStrengthChanged();
}

void AppSettings::setDefaultObjectScope(int scopeKind)
{
   if (scopeKind < 0 || scopeKind > kScopeKindMax || scopeKind == p->defaultObjectScope_)
   {
      return;
   }

   p->defaultObjectScope_ = scopeKind;
   p->store_.SetInt(kObjectsCategory, QStringLiteral("default_scope"), scopeKind);
   p->store_.Save();

   Q_EMIT defaultObjectScopeChanged();
}

void AppSettings::setDistanceUnits(int units)
{
   if (units < 0 || units > kDistanceUnitsMax || units == p->distanceUnits_)
   {
      return;
   }

   p->distanceUnits_ = units;
   p->ApplyDistanceUnits();
   p->store_.SetInt(kUnitsCategory, QStringLiteral("distance"), units);
   p->store_.Save();

   Q_EMIT distanceUnitsChanged();
}

void AppSettings::setMapTheme(int theme)
{
   if (theme < 0 || theme > kMapThemeMax || theme == p->mapTheme_)
   {
      return;
   }
   p->mapTheme_ = theme;
   p->store_.SetInt(kAppearanceCategory, QStringLiteral("map_theme"), theme);
   p->store_.Save();
   Q_EMIT mapThemeChanged();
}

void AppSettings::setControlBarDocked(bool docked)
{
   if (docked == p->controlBarDocked_)
   {
      return;
   }
   p->controlBarDocked_ = docked;
   p->store_.SetBool(kAppearanceCategory, QStringLiteral("control_bar_docked"), docked);
   p->store_.Save();
   Q_EMIT controlBarDockedChanged();
}

void AppSettings::setCenterMapOnSiteChange(bool enabled)
{
   if (enabled == p->centerMapOnSiteChange_) return;
   p->centerMapOnSiteChange_ = enabled;
   p->store_.SetBool(kRadarCategory, QStringLiteral("center_map_on_site_change"), enabled);
   p->store_.Save();
   Q_EMIT centerMapOnSiteChangeChanged();
}

void AppSettings::setMapDetailsPreset(int preset)
{
   if (preset < 0 || preset > kMapDetailsPresetMax || preset == p->mapDetailsPreset_)
   {
      return;
   }

   p->mapDetailsPreset_ = preset;
   if (preset != static_cast<int>(MapDetailsPreset::Custom))
   {
      const auto& values = preset == static_cast<int>(MapDetailsPreset::Minimal)
                              ? kMinimalMapDetails
                              : (preset == static_cast<int>(MapDetailsPreset::Detailed)
                                    ? kDetailedMapDetails
                                    : kOperationalMapDetails);
      for (std::size_t i = 0; i < kMapDetailGroups.size(); ++i)
      {
         const QString id = QString::fromLatin1(kMapDetailGroups[i].id);
         p->mapDetailVisible_[id] = values[i];
         p->store_.SetBool(kMapCategory, QStringLiteral("show_") + id, values[i]);
      }
   }
   p->store_.SetInt(kMapCategory, QStringLiteral("details_preset"), preset);
   p->store_.Save();
   Q_EMIT mapDetailsChanged();
}

QVariantList AppSettings::mapDetailGroups() const
{
   QVariantList groups;
   for (const MapDetailGroup& group : kMapDetailGroups)
   {
      QVariantMap entry;
      const QString id = QString::fromLatin1(group.id);
      entry[QStringLiteral("id")] = id;
      entry[QStringLiteral("label")] = QString::fromLatin1(group.label);
      entry[QStringLiteral("visible")] = mapDetailVisible(id);
      groups.append(entry);
   }
   return groups;
}

QVariantMap AppSettings::mapDetailVisibility() const
{
   QVariantMap visibility;
   for (const auto& [id, visible] : p->mapDetailVisible_)
   {
      visibility[id] = visible;
   }
   return visibility;
}

bool AppSettings::mapDetailVisible(const QString& groupId) const
{
   const auto it = p->mapDetailVisible_.find(groupId);
   return it != p->mapDetailVisible_.end() ? it->second : true;
}

void AppSettings::setMapDetailVisible(const QString& groupId, bool visible)
{
   const auto it = p->mapDetailVisible_.find(groupId);
   if (it == p->mapDetailVisible_.end() || it->second == visible)
   {
      return;
   }
   it->second = visible;
   p->mapDetailsPreset_ = static_cast<int>(MapDetailsPreset::Custom);
   p->store_.SetBool(kMapCategory, QStringLiteral("show_") + groupId, visible);
   p->store_.SetInt(kMapCategory,
                    QStringLiteral("details_preset"),
                    static_cast<int>(MapDetailsPreset::Custom));
   p->store_.Save();
   Q_EMIT mapDetailsChanged();
}

QVariantList AppSettings::toolbarActions() const
{
   QVariantList actions;
   for (const ToolbarAction& action : kToolbarActions)
   {
      QVariantMap item;
      const QString id = QString::fromLatin1(action.id);
      item[QStringLiteral("id")] = id;
      item[QStringLiteral("label")] = QString::fromLatin1(action.label);
      item[QStringLiteral("visible")] = p->toolbarActionVisible_.at(id);
      actions.append(item);
   }
   return actions;
}

void AppSettings::setToolbarActionVisible(const QString& actionId, bool visible)
{
   const auto it = p->toolbarActionVisible_.find(actionId);
   if (it == p->toolbarActionVisible_.end() || it->second == visible) return;
   it->second = visible;
   p->store_.SetBool(kAppearanceCategory, QStringLiteral("show_toolbar_") + actionId, visible);
   p->store_.Save();
   Q_EMIT toolbarActionsChanged();
}

void AppSettings::resetToolbarActions()
{
   for (const ToolbarAction& action : kToolbarActions)
   {
      const QString id = QString::fromLatin1(action.id);
      p->toolbarActionVisible_[id] = false;
      p->store_.SetBool(kAppearanceCategory, QStringLiteral("show_toolbar_") + id, false);
   }
   p->store_.Save();
   Q_EMIT toolbarActionsChanged();
}

QVariantList AppSettings::geometryRows() const
{
   QVariantList rows;

   for (const GeometryRow& row : kGeometryRows)
   {
      const QString id = QString::fromLatin1(row.id);

      QVariantMap entry;
      entry[QStringLiteral("id")]       = id;
      entry[QStringLiteral("label")]    = QString::fromLatin1(row.label);
      entry[QStringLiteral("note")]     = QString::fromLatin1(row.note);
      entry[QStringLiteral("valueKey")] = QString::fromLatin1(row.valueKey);
      entry[QStringLiteral("dimWhen")]  = QString::fromLatin1(row.dimWhen);
      entry[QStringLiteral("visible")]  = geometryRowVisible(id);
      rows.append(entry);
   }

   return rows;
}

bool AppSettings::geometryRowVisible(const QString& rowId) const
{
   const auto it = p->geometryRowVisible_.find(rowId);
   return (it != p->geometryRowVisible_.end()) ? it->second : true;
}

void AppSettings::setGeometryRowVisible(const QString& rowId, bool visible)
{
   const auto it = p->geometryRowVisible_.find(rowId);
   if (it == p->geometryRowVisible_.end())
   {
      logger_->warn("Unknown radar geometry row \"{}\"", rowId.toStdString());
      return;
   }
   if (it->second == visible)
   {
      return;
   }

   it->second = visible;
   p->store_.SetBool(kRadarCategory, QStringLiteral("show_") + rowId, visible);
   p->store_.Save();

   Q_EMIT geometryRowsChanged();
}

QVariantList AppSettings::sections() const
{
   QVariantList list;

   for (const Section& section : kSections)
   {
      QVariantMap entry;
      entry[QStringLiteral("id")]      = QString::fromLatin1(section.id);
      entry[QStringLiteral("title")]   = QString::fromLatin1(section.title);
      entry[QStringLiteral("summary")] = QString::fromLatin1(section.summary);
      list.append(entry);
   }

   return list;
}

bool AppSettings::hasSection(const QString& sectionId) const
{
   for (const Section& section : kSections)
   {
      if (sectionId == QLatin1String(section.id))
      {
         return true;
      }
   }
   return false;
}

void AppSettings::resetToDefaults()
{
   p->store_.SetInt(
      kMeasurementCategory, QStringLiteral("gesture"), static_cast<int>(MeasurementGesture::Both));
   p->store_.SetInt(kMeasurementCategory, QStringLiteral("preferred_tool"), 1);
   p->store_.SetInt(kMeasurementCategory,
                    QStringLiteral("snap_strength"),
                    static_cast<int>(SnapStrength::Subtle));
   p->store_.SetInt(kObjectsCategory,
                    QStringLiteral("default_scope"),
                    static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly));
   p->store_.SetInt(kAppearanceCategory,
                    QStringLiteral("map_theme"),
                    static_cast<int>(MapTheme::FollowChrome));
   p->store_.SetBool(kAppearanceCategory, QStringLiteral("control_bar_docked"), false);
   p->store_.SetBool(kRadarCategory, QStringLiteral("center_map_on_site_change"), true);
   for (const ToolbarAction& action : kToolbarActions)
      p->store_.SetBool(kAppearanceCategory,
                        QStringLiteral("show_toolbar_") + QString::fromLatin1(action.id), false);
   p->store_.SetInt(kMapCategory,
                    QStringLiteral("details_preset"),
                    static_cast<int>(MapDetailsPreset::Operational));
   for (std::size_t i = 0; i < kMapDetailGroups.size(); ++i)
   {
      p->store_.SetBool(kMapCategory,
                        QStringLiteral("show_") +
                           QString::fromLatin1(kMapDetailGroups[i].id),
                        kOperationalMapDetails[i]);
   }
   p->store_.SetInt(kUnitsCategory, QStringLiteral("distance"), static_cast<int>(DistanceUnits::Both));

   for (const GeometryRow& row : kGeometryRows)
   {
      p->store_.SetBool(
         kRadarCategory, QStringLiteral("show_") + QString::fromLatin1(row.id), true);
   }

   p->store_.Save();
   p->Load();

   Q_EMIT measurementGestureChanged();
   Q_EMIT preferredMeasurementToolChanged();
   Q_EMIT snapStrengthChanged();
   Q_EMIT defaultObjectScopeChanged();
   Q_EMIT distanceUnitsChanged();
   Q_EMIT mapThemeChanged();
   Q_EMIT controlBarDockedChanged();
   Q_EMIT centerMapOnSiteChangeChanged();
   Q_EMIT mapDetailsChanged();
   Q_EMIT toolbarActionsChanged();
   Q_EMIT geometryRowsChanged();
}

QString AppSettings::configDirectory() const
{
   return p->store_.ConfigDirectory();
}

} // namespace settings
} // namespace wxlens
