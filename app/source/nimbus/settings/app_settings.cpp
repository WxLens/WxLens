#include <nimbus/settings/app_settings.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/objects/map_object.hpp>
#include <nimbus/settings/settings_store.hpp>
#include <nimbus/util/unit_format.hpp>

#include <array>
#include <map>

#include <QVariantMap>

namespace nimbus
{
namespace settings
{

static const std::string logPrefix_ = "settings.app_settings";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{

// One TOML file per category, per ADR 0003.
const QString kMeasurementCategory = QStringLiteral("measurement");
const QString kObjectsCategory     = QStringLiteral("objects");
const QString kUnitsCategory       = QStringLiteral("units");
const QString kRadarCategory       = QStringLiteral("radar");

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
constexpr std::array<Section, 4> kSections {{
   {"measurement", "Measurement", "How measurements are started and finished."},
   {"objects", "Map objects", "Defaults for markers, range rings and pinned measurements."},
   {"units", "Units", "How distances and altitudes are displayed."},
   {"radar-geometry", "Radar geometry", "Which rows the beam-height readout shows."},
}};

constexpr int kMeasurementGestureMax = static_cast<int>(AppSettings::MeasurementGesture::ClickOnly);
constexpr int kDistanceUnitsMax      = static_cast<int>(AppSettings::DistanceUnits::Imperial);
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
   int defaultObjectScope_ {static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly)};
   int distanceUnits_ {static_cast<int>(DistanceUnits::Both)};

   std::map<QString, bool> geometryRowVisible_ {};
};

void AppSettings::Impl::Load()
{
   measurementGesture_ = store_.GetInt(kMeasurementCategory,
                                       QStringLiteral("gesture"),
                                       static_cast<int>(MeasurementGesture::Both),
                                       0,
                                       kMeasurementGestureMax);

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

int AppSettings::defaultObjectScope() const
{
   return p->defaultObjectScope_;
}

int AppSettings::distanceUnits() const
{
   return p->distanceUnits_;
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
   p->store_.SetInt(kObjectsCategory,
                    QStringLiteral("default_scope"),
                    static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly));
   p->store_.SetInt(kUnitsCategory, QStringLiteral("distance"), static_cast<int>(DistanceUnits::Both));

   for (const GeometryRow& row : kGeometryRows)
   {
      p->store_.SetBool(
         kRadarCategory, QStringLiteral("show_") + QString::fromLatin1(row.id), true);
   }

   p->store_.Save();
   p->Load();

   Q_EMIT measurementGestureChanged();
   Q_EMIT defaultObjectScopeChanged();
   Q_EMIT distanceUnitsChanged();
   Q_EMIT geometryRowsChanged();
}

QString AppSettings::configDirectory() const
{
   return p->store_.ConfigDirectory();
}

} // namespace settings
} // namespace nimbus
