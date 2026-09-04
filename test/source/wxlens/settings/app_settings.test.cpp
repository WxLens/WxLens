// Tests for the typed settings accessors (docs/ROADMAP.md §3.2's validated-defaults pattern, and
// §4.5's addressable sections).
//
// Two things here are contract, not implementation detail, and are tested as such: section ids are
// what deep-links point at, so they must be stable; and the §4.7 terrain/AGL rows must default to
// visible, because shipping them hidden is the silent omission that section exists to prevent.

#include <wxlens/objects/map_object.hpp>
#include <wxlens/settings/app_settings.hpp>
#include <wxlens/settings/settings_store.hpp>
#include <wxlens/util/unit_format.hpp>

#include <QTemporaryDir>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace wxlens::settings::test
{

namespace
{

using Gesture = AppSettings::MeasurementGesture;
using Snap    = AppSettings::SnapStrength;
using Units   = AppSettings::DistanceUnits;

class AppSettingsTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(tempDir_.isValid());
      store_ = std::make_unique<SettingsStore>();
      store_->SetConfigDirectory(tempDir_.path());
      settings_ = std::make_unique<AppSettings>(*store_);
   }

   void TearDown() override
   {
      // Leaves the process-wide formatting preference as the next test expects to find it.
      util::SetDistanceUnitPreference(util::DistanceUnitPreference::Both);
   }

   [[nodiscard]] QVariantMap Row(const QString& id) const
   {
      for (const QVariant& entry : settings_->geometryRows())
      {
         const QVariantMap row = entry.toMap();
         if (row[QStringLiteral("id")].toString() == id)
         {
            return row;
         }
      }
      return {};
   }

   QTemporaryDir                  tempDir_;
   std::unique_ptr<SettingsStore> store_;
   std::unique_ptr<AppSettings>   settings_;
};

} // namespace

TEST_F(AppSettingsTest, ShippedDefaults)
{
   EXPECT_EQ(settings_->measurementGesture(), static_cast<int>(Gesture::Both));
   EXPECT_EQ(settings_->snapStrength(), static_cast<int>(Snap::Subtle));
   EXPECT_EQ(settings_->snapTolerancePixels(), 10.0);
   EXPECT_EQ(settings_->distanceUnits(), static_cast<int>(Units::Both));
   EXPECT_EQ(settings_->mapTheme(), static_cast<int>(AppSettings::MapTheme::FollowChrome));
   EXPECT_FALSE(settings_->controlBarDocked());
   EXPECT_EQ(settings_->radarSiteScope(),
             static_cast<int>(AppSettings::RadarSiteScope::AllPanes));
   EXPECT_TRUE(settings_->radarSitesVisible());
   EXPECT_TRUE(settings_->tdwrSitesVisible());
   EXPECT_EQ(settings_->mapDetailsPreset(),
             static_cast<int>(AppSettings::MapDetailsPreset::Operational));
   EXPECT_TRUE(settings_->mapDetailVisible(QStringLiteral("roads")));
   EXPECT_FALSE(settings_->mapDetailVisible(QStringLiteral("buildings")));
   EXPECT_FALSE(settings_->mapDetailVisible(QStringLiteral("poi")));

   // §4.3 is explicit that CurrentPaneOnly is the *shipped default*, read from config - not a
   // hardcoded constant.
   EXPECT_EQ(settings_->defaultObjectScope(),
             static_cast<int>(objects::MapObjectScopeKind::CurrentPaneOnly));
}

TEST_F(AppSettingsTest, TerrainAndAglRowsDefaultVisible)
{
   // The §4.7 constraint, pinned. If a future change makes these default off, the readout starts
   // implying its MSL figure is an AGL one - which is the exact failure that section forbids.
   EXPECT_TRUE(settings_->geometryRowVisible(QStringLiteral("terrain")));
   EXPECT_TRUE(settings_->geometryRowVisible(QStringLiteral("beamAgl")));

   // And the settings UI must be able to explain why, rather than presenting them as ordinary rows.
   EXPECT_FALSE(Row(QStringLiteral("terrain"))[QStringLiteral("note")].toString().isEmpty());
   EXPECT_FALSE(Row(QStringLiteral("beamAgl"))[QStringLiteral("note")].toString().isEmpty());
}

TEST_F(AppSettingsTest, EveryGeometryRowDefaultsVisible)
{
   for (const QVariant& entry : settings_->geometryRows())
   {
      const QVariantMap row = entry.toMap();
      EXPECT_TRUE(row[QStringLiteral("visible")].toBool())
         << row[QStringLiteral("id")].toString().toStdString();
      EXPECT_FALSE(row[QStringLiteral("label")].toString().isEmpty());
   }
}

TEST_F(AppSettingsTest, ChangesPersistAcrossAReload)
{
   settings_->setMeasurementGesture(static_cast<int>(Gesture::DragOnly));
   settings_->setSnapStrength(static_cast<int>(Snap::Strong));
   settings_->setDefaultObjectScope(static_cast<int>(objects::MapObjectScopeKind::AllPanes));
   settings_->setDistanceUnits(static_cast<int>(Units::Imperial));
   settings_->setMapTheme(static_cast<int>(AppSettings::MapTheme::Dark));
   settings_->setControlBarDocked(true);
   settings_->setRadarSiteScope(static_cast<int>(AppSettings::RadarSiteScope::ActivePaneOnly));
   settings_->setRadarSitesVisible(false);
   settings_->setTdwrSitesVisible(false);
   settings_->setMapDetailsPreset(static_cast<int>(AppSettings::MapDetailsPreset::Detailed));
   settings_->setMapDetailVisible(QStringLiteral("roads"), false);
   settings_->setGeometryRowVisible(QStringLiteral("terrain"), false);

   // A second store over the same directory stands in for the next launch. A setting that only
   // lived in memory would pass an in-process check and still be gone tomorrow.
   SettingsStore reopened;
   reopened.SetConfigDirectory(tempDir_.path());
   AppSettings reloaded {reopened};

   EXPECT_EQ(reloaded.measurementGesture(), static_cast<int>(Gesture::DragOnly));
   EXPECT_EQ(reloaded.snapStrength(), static_cast<int>(Snap::Strong));
   EXPECT_EQ(reloaded.snapTolerancePixels(), 18.0);
   EXPECT_EQ(reloaded.defaultObjectScope(),
             static_cast<int>(objects::MapObjectScopeKind::AllPanes));
   EXPECT_EQ(reloaded.distanceUnits(), static_cast<int>(Units::Imperial));
   EXPECT_EQ(reloaded.mapTheme(), static_cast<int>(AppSettings::MapTheme::Dark));
   EXPECT_TRUE(reloaded.controlBarDocked());
   EXPECT_EQ(reloaded.radarSiteScope(),
             static_cast<int>(AppSettings::RadarSiteScope::ActivePaneOnly));
   EXPECT_FALSE(reloaded.radarSitesVisible());
   EXPECT_FALSE(reloaded.tdwrSitesVisible());
   EXPECT_EQ(reloaded.mapDetailsPreset(), static_cast<int>(AppSettings::MapDetailsPreset::Custom));
   EXPECT_FALSE(reloaded.mapDetailVisible(QStringLiteral("roads")));
   EXPECT_TRUE(reloaded.mapDetailVisible(QStringLiteral("buildings")));
   EXPECT_FALSE(reloaded.geometryRowVisible(QStringLiteral("terrain")));
}

TEST_F(AppSettingsTest, OutOfRangeValuesAreRefused)
{
   const int before = settings_->measurementGesture();
   settings_->setMeasurementGesture(99);
   settings_->setMeasurementGesture(-1);
   EXPECT_EQ(settings_->measurementGesture(), before);

   settings_->setDistanceUnits(42);
   EXPECT_EQ(settings_->distanceUnits(), static_cast<int>(Units::Both));

   settings_->setSnapStrength(42);
   EXPECT_EQ(settings_->snapStrength(), static_cast<int>(Snap::Subtle));
}

TEST_F(AppSettingsTest, SnapPresetsMapToScreenSpaceTolerances)
{
   settings_->setSnapStrength(static_cast<int>(Snap::Off));
   EXPECT_EQ(settings_->snapTolerancePixels(), 0.0);
   settings_->setSnapStrength(static_cast<int>(Snap::Subtle));
   EXPECT_EQ(settings_->snapTolerancePixels(), 10.0);
   settings_->setSnapStrength(static_cast<int>(Snap::Strong));
   EXPECT_EQ(settings_->snapTolerancePixels(), 18.0);
}

TEST_F(AppSettingsTest, ChangesNotifyExactlyOnce)
{
   // Counted with plain connections rather than QSignalSpy, so the suite does not need Qt6::Test
   // linked for one assertion.
   int gestureCount = 0;
   int rowCount     = 0;
   QObject::connect(settings_.get(),
                    &AppSettings::measurementGestureChanged,
                    settings_.get(),
                    [&gestureCount]() { ++gestureCount; });
   QObject::connect(settings_.get(),
                    &AppSettings::geometryRowsChanged,
                    settings_.get(),
                    [&rowCount]() { ++rowCount; });

   settings_->setMeasurementGesture(static_cast<int>(Gesture::ClickOnly));
   EXPECT_EQ(gestureCount, 1);

   // Setting the same value again is not a change; a redundant signal would re-run every binding
   // watching it.
   settings_->setMeasurementGesture(static_cast<int>(Gesture::ClickOnly));
   EXPECT_EQ(gestureCount, 1);

   settings_->setGeometryRowVisible(QStringLiteral("azimuth"), false);
   EXPECT_EQ(rowCount, 1);
   settings_->setGeometryRowVisible(QStringLiteral("azimuth"), false);
   EXPECT_EQ(rowCount, 1);
}

TEST_F(AppSettingsTest, UnitPreferenceDrivesFormatting)
{
   settings_->setDistanceUnits(static_cast<int>(Units::Metric));
   settings_->setMapTheme(static_cast<int>(AppSettings::MapTheme::Light));
   EXPECT_EQ(util::FormatGroundDistance(50000.0), QStringLiteral("50.0 km"));
   EXPECT_EQ(util::FormatAltitude(1000.0), QStringLiteral("1000 m"));

   settings_->setDistanceUnits(static_cast<int>(Units::Imperial));
   EXPECT_TRUE(util::FormatGroundDistance(50000.0).endsWith(QStringLiteral("mi")));
   EXPECT_TRUE(util::FormatAltitude(1000.0).endsWith(QStringLiteral("ft")));

   // Both is what slice 7 shipped and remains the default, so it must keep working.
   settings_->setDistanceUnits(static_cast<int>(Units::Both));
   EXPECT_EQ(util::FormatGroundDistance(50000.0), QStringLiteral("50.0 km / 31.1 mi"));
}

TEST_F(AppSettingsTest, SectionIdsAreStableAndAddressable)
{
   // §4.5: a quick control deep-links by id. These ids are a contract - renaming one silently
   // breaks every control that points at it, which is why they are asserted literally here.
   for (const QString& id : {QStringLiteral("appearance"),
                             QStringLiteral("map-details"),
                             QStringLiteral("measurement"),
                             QStringLiteral("objects"),
                             QStringLiteral("units"),
                             QStringLiteral("radar-geometry")})
   {
      EXPECT_TRUE(settings_->hasSection(id)) << id.toStdString();
   }

   EXPECT_FALSE(settings_->hasSection(QStringLiteral("not-a-section")));

   // Every section must be presentable, or the dialog renders a blank row.
   for (const QVariant& entry : settings_->sections())
   {
      const QVariantMap section = entry.toMap();
      EXPECT_FALSE(section[QStringLiteral("id")].toString().isEmpty());
      EXPECT_FALSE(section[QStringLiteral("title")].toString().isEmpty());
      EXPECT_FALSE(section[QStringLiteral("summary")].toString().isEmpty());
   }
}

TEST_F(AppSettingsTest, UnknownGeometryRowIsVisibleRatherThanHidden)
{
   // Failing toward showing data: an id this build has never heard of is far more likely a newer
   // row than a mistake, and silently hiding a beam figure is the worse error for this readout.
   EXPECT_TRUE(settings_->geometryRowVisible(QStringLiteral("somethingNewer")));
}

TEST_F(AppSettingsTest, ResetRestoresEveryDefault)
{
   settings_->setMeasurementGesture(static_cast<int>(Gesture::ClickOnly));
   settings_->setDistanceUnits(static_cast<int>(Units::Metric));
   settings_->setGeometryRowVisible(QStringLiteral("beamAgl"), false);
   settings_->setMapDetailsPreset(static_cast<int>(AppSettings::MapDetailsPreset::Detailed));

   settings_->resetToDefaults();

   EXPECT_EQ(settings_->measurementGesture(), static_cast<int>(Gesture::Both));
   EXPECT_EQ(settings_->distanceUnits(), static_cast<int>(Units::Both));
   EXPECT_EQ(settings_->mapTheme(), static_cast<int>(AppSettings::MapTheme::FollowChrome));
   EXPECT_FALSE(settings_->controlBarDocked());
   EXPECT_TRUE(settings_->geometryRowVisible(QStringLiteral("beamAgl")));
   EXPECT_EQ(settings_->mapDetailsPreset(),
             static_cast<int>(AppSettings::MapDetailsPreset::Operational));
   EXPECT_FALSE(settings_->mapDetailVisible(QStringLiteral("buildings")));

   // And the reset is itself persisted, not just applied in memory.
   SettingsStore reopened;
   reopened.SetConfigDirectory(tempDir_.path());
   AppSettings reloaded {reopened};
   EXPECT_TRUE(reloaded.geometryRowVisible(QStringLiteral("beamAgl")));
}

TEST_F(AppSettingsTest, MapDetailPresetsAreCompleteAndManualChangesBecomeCustom)
{
   settings_->setMapDetailsPreset(static_cast<int>(AppSettings::MapDetailsPreset::Minimal));
   EXPECT_FALSE(settings_->mapDetailVisible(QStringLiteral("roads")));
   EXPECT_TRUE(settings_->mapDetailVisible(QStringLiteral("places")));

   settings_->setMapDetailsPreset(static_cast<int>(AppSettings::MapDetailsPreset::Detailed));
   for (const QVariant& entry : settings_->mapDetailGroups())
   {
      EXPECT_TRUE(entry.toMap()[QStringLiteral("visible")].toBool());
   }

   settings_->setMapDetailVisible(QStringLiteral("poi"), false);
   EXPECT_EQ(settings_->mapDetailsPreset(), static_cast<int>(AppSettings::MapDetailsPreset::Custom));
}

} // namespace wxlens::settings::test
