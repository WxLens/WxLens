// Tests for the measurement framework (docs/ROADMAP.md §4.4).
//
// Two things matter most here and are tested hardest: that the geometry uses real geodesics (a
// flat-earth approximation would pass a "does it return a number" test and be quietly wrong), and
// that an in-progress measurement never reaches the store until it is committed (§4.3 tier 1).

#include <nimbus/objects/map_object_store.hpp>
#include <nimbus/objects/measurement_controller.hpp>
#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/panes/pane_grid_model.hpp>
#include <nimbus/util/geodesic.hpp>
#include <nimbus/util/unit_format.hpp>

#include <cmath>

#include <gtest/gtest.h>

namespace nimbus::objects::test
{

namespace
{

using Mode = MeasurementController::Mode;

class MeasurementTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      model_.setDefaultSourceKey(QString {});
      model_.setGridSize(2, 1);
      store_.Clear();
   }

   void TearDown() override { store_.Clear(); }

   [[nodiscard]] panes::PaneController* Pane(int row) const
   {
      const QModelIndex index = model_.index(row, 0);
      return qvariant_cast<panes::PaneController*>(
         model_.data(index, panes::PaneGridModel::PaneRole));
   }

   panes::PaneGridModel  model_;
   MapObjectStore&       store_ {MapObjectStore::Instance()};
   MeasurementController measurement_;
};

} // namespace

TEST_F(MeasurementTest, NoModeMeansNoMeasurement)
{
   measurement_.addPoint(35.0, -97.0, Pane(0));

   EXPECT_FALSE(measurement_.active());
   EXPECT_TRUE(measurement_.points().isEmpty());
}

TEST_F(MeasurementTest, PointToPointProducesDistanceAndBearing)
{
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));

   const QVariantList segments = measurement_.segments();
   ASSERT_EQ(segments.size(), 1);

   const QVariantMap segment = segments.first().toMap();

   // One degree of latitude is ~111 km. A flat-earth or degree-based approximation would not land
   // in this window, so this pins that real geodesics are in use.
   const double distance = segment[QStringLiteral("distanceMeters")].toDouble();
   EXPECT_GT(distance, 110'000.0);
   EXPECT_LT(distance, 112'000.0);

   // Due north.
   EXPECT_NEAR(segment[QStringLiteral("bearingDegrees")].toDouble(), 0.0, 0.5);
}

TEST_F(MeasurementTest, BearingIsReportedAsACompassAngle)
{
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(35.0, -98.0, Pane(0));

   // Due west is 270 degrees on a compass; the raw geodesic azimuth is -90.
   const QVariantMap segment = measurement_.segments().first().toMap();
   EXPECT_NEAR(segment[QStringLiteral("bearingDegrees")].toDouble(), 270.0, 0.5);
}

TEST_F(MeasurementTest, PointToPointStopsAtTwoVertices)
{
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   measurement_.addPoint(37.0, -97.0, Pane(0));

   EXPECT_EQ(measurement_.points().size(), 4) << "two vertices, flat lat/lon pairs";
   EXPECT_EQ(measurement_.segments().size(), 1);
}

TEST_F(MeasurementTest, PathAccumulatesSegmentsAndTotal)
{
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   measurement_.addPoint(37.0, -97.0, Pane(0));

   ASSERT_EQ(measurement_.segments().size(), 2);

   const double first  = measurement_.segments()[0].toMap()["distanceMeters"].toDouble();
   const double second = measurement_.segments()[1].toMap()["distanceMeters"].toDouble();
   EXPECT_NEAR(measurement_.totalMeters(), first + second, 1e-6);
}

TEST_F(MeasurementTest, RadarToPointAnchorsAtThePanesOwnSite)
{
   measurement_.setMode(static_cast<int>(Mode::RadarToPoint));

   // A single click supplies only the far end; the origin comes from the pane.
   measurement_.addPoint(35.0, -97.0, Pane(0));

   const QVariantList points = measurement_.points();
   ASSERT_EQ(points.size(), 4) << "origin plus clicked point";
   EXPECT_NEAR(points[0].toDouble(), Pane(0)->homeLatitude(), 1e-9);
   EXPECT_NEAR(points[1].toDouble(), Pane(0)->homeLongitude(), 1e-9);
   EXPECT_NEAR(points[2].toDouble(), 35.0, 1e-9);

   ASSERT_EQ(measurement_.segments().size(), 1);
   EXPECT_FALSE(measurement_.readout().isEmpty());
}

TEST_F(MeasurementTest, LiveCursorIsIncludedButNotCommitted)
{
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.updateCursor(36.0, -97.0);

   EXPECT_EQ(measurement_.points().size(), 4) << "committed vertex plus live cursor";
   EXPECT_EQ(measurement_.segments().size(), 1) << "rubber-band segment is measured live";

   // ...but the cursor is not a chosen vertex, so it must not be enough to pin anything.
   EXPECT_EQ(measurement_.commit(Pane(0), 0), -1);
   EXPECT_EQ(store_.rowCount(), 0);
}

TEST_F(MeasurementTest, InProgressMeasurementNeverReachesTheStore)
{
   // Tier 1 (§4.3): interrogating the map must leave nothing behind.
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   measurement_.updateCursor(37.0, -97.0);

   EXPECT_EQ(store_.rowCount(), 0);

   measurement_.cancel();
   EXPECT_EQ(store_.rowCount(), 0);
   EXPECT_FALSE(measurement_.active());
}

TEST_F(MeasurementTest, CommitPinsAMeasurementObject)
{
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));

   const int id = measurement_.commit(Pane(0), static_cast<int>(MapObjectScopeKind::AllPanes));
   ASSERT_GT(id, 0);

   const MapObject* object = store_.Find(id);
   ASSERT_NE(object, nullptr);
   EXPECT_EQ(object->type, MapObjectType::Measurement);
   EXPECT_EQ(object->lifecycle, MapObjectLifecycle::Pinned);
   EXPECT_EQ(object->latitudes.size(), 2);
   EXPECT_FALSE(object->label.isEmpty()) << "distance is baked in at commit time";

   // The tool resets so the next measurement starts clean.
   EXPECT_FALSE(measurement_.active());
}

TEST_F(MeasurementTest, CommitExcludesTheLiveCursor)
{
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   measurement_.updateCursor(45.0, -80.0);

   const int id = measurement_.commit(Pane(0), 0);
   ASSERT_GT(id, 0);

   const MapObject* object = store_.Find(id);
   ASSERT_NE(object, nullptr);
   EXPECT_EQ(object->latitudes.size(), 2)
      << "the cursor is where the pointer happens to be, not a chosen vertex";
}

TEST_F(MeasurementTest, CommittedMeasurementRespectsScope)
{
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));

   const int id =
      measurement_.commit(Pane(0), static_cast<int>(MapObjectScopeKind::CurrentPaneOnly));
   ASSERT_GT(id, 0);

   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(*store_.Find(id), Pane(0)));
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(*store_.Find(id), Pane(1)));
}

TEST_F(MeasurementTest, UndoRemovesTheLastVertex)
{
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   measurement_.addPoint(37.0, -97.0, Pane(0));
   ASSERT_EQ(measurement_.segments().size(), 2);

   measurement_.undoPoint();
   EXPECT_EQ(measurement_.segments().size(), 1);
}

TEST_F(MeasurementTest, UndoOnRadarToPointDropsTheAutomaticOrigin)
{
   measurement_.setMode(static_cast<int>(Mode::RadarToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   ASSERT_TRUE(measurement_.active());

   // Leaving a lone origin behind would be confusing, since the user never placed it.
   measurement_.undoPoint();
   EXPECT_FALSE(measurement_.active());
   EXPECT_TRUE(measurement_.points().isEmpty());
}

TEST_F(MeasurementTest, SwitchingModeAbandonsWorkInProgress)
{
   measurement_.setMode(static_cast<int>(Mode::Path));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   ASSERT_TRUE(measurement_.active());

   measurement_.setMode(static_cast<int>(Mode::PointToPoint));

   EXPECT_FALSE(measurement_.active())
      << "half a path would mean something different in another mode";
   EXPECT_EQ(store_.rowCount(), 0) << "abandoning is not the same as pinning";
}

TEST_F(MeasurementTest, GeodesicMatchesTheUnderlyingHelper)
{
   // The controller must not reimplement the math - it should agree exactly with util::Geodesic.
   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.2, -97.4, Pane(0));
   measurement_.addPoint(38.9, -77.0, Pane(0));

   const auto expected = util::GeodesicInverse(35.2, -97.4, 38.9, -77.0);
   EXPECT_NEAR(measurement_.totalMeters(), expected.distanceMeters, 1e-6);
}

TEST_F(MeasurementTest, DistanceFormattingShowsBothUnits)
{
   const QString text = MeasurementController::formatDistance(1609.344);
   EXPECT_TRUE(text.contains(QStringLiteral("km")));
   EXPECT_TRUE(text.contains(QStringLiteral("mi")));
   EXPECT_TRUE(text.contains(QStringLiteral("1.00")));
}

TEST_F(MeasurementTest, UnitChangeRefreshesLiveAndPinnedMeasurementLabels)
{
   util::SetDistanceUnitPreference(util::DistanceUnitPreference::Metric);

   measurement_.setMode(static_cast<int>(Mode::PointToPoint));
   measurement_.addPoint(35.0, -97.0, Pane(0));
   measurement_.addPoint(36.0, -97.0, Pane(0));
   ASSERT_TRUE(measurement_.readout().contains(QStringLiteral("km")));
   ASSERT_FALSE(measurement_.readout().contains(QStringLiteral("mi")));

   const int id = measurement_.commit(Pane(0), static_cast<int>(MapObjectScopeKind::AllPanes));
   ASSERT_GT(id, 0);

   util::SetDistanceUnitPreference(util::DistanceUnitPreference::Imperial);
   measurement_.refreshFormatting();
   store_.refreshFormatting();

   const QVariantList objects = store_.objectsForPane(Pane(0));
   ASSERT_EQ(objects.size(), 1);
   const QString label = objects.first().toMap()[QStringLiteral("label")].toString();
   EXPECT_TRUE(label.contains(QStringLiteral("mi")));
   EXPECT_FALSE(label.contains(QStringLiteral("km")));

   util::SetDistanceUnitPreference(util::DistanceUnitPreference::Both);
}

} // namespace nimbus::objects::test
