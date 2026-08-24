// Tests for PaneController::probeSourceAt (docs/ROADMAP.md §4.4's point-info probe, §4.7's radar
// -geometry interrogation).
//
// Only the unbound-source paths are exercised here: binding a pane to a real site makes
// RadarSweepProduct fetch Level 2 data from S3, which does not belong in a unit test. The beam
// math itself is covered without any of that in util/radar_geometry.test.cpp, and the bound path
// is verified in the running app. What these pin is the contract the UI depends on - that an
// unavailable probe says so explicitly instead of returning zeros that render as real numbers.

#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/panes/pane_grid_model.hpp>

#include <QVariantMap>

#include <gtest/gtest.h>

namespace nimbus::panes::test
{

namespace
{

class SourceProbeTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // Empty source key: no site binding, and therefore no network access from these tests.
      model_.setDefaultSourceKey(QString {});
      model_.setGridSize(1, 1);
   }

   [[nodiscard]] PaneController* Pane() const
   {
      const QModelIndex index = model_.index(0, 0);
      return qvariant_cast<PaneController*>(model_.data(index, PaneGridModel::PaneRole));
   }

   PaneGridModel model_;
};

} // namespace

TEST_F(SourceProbeTest, ProbeAlwaysReportsWhatWasAsked)
{
   const QVariantMap probe = Pane()->probeSourceAt(35.5, -97.5);

   EXPECT_DOUBLE_EQ(probe[QStringLiteral("latitude")].toDouble(), 35.5);
   EXPECT_DOUBLE_EQ(probe[QStringLiteral("longitude")].toDouble(), -97.5);
   EXPECT_FALSE(probe[QStringLiteral("kind")].toString().isEmpty());
}

TEST_F(SourceProbeTest, UnboundSourceIsUnavailableWithAReason)
{
   const QVariantMap probe = Pane()->probeSourceAt(35.5, -97.5);

   EXPECT_FALSE(probe[QStringLiteral("available")].toBool());

   // A reason the UI can show. Without this the readout would have to invent its own wording for
   // a state it cannot distinguish from "not loaded yet".
   EXPECT_FALSE(probe[QStringLiteral("unavailableReason")].toString().isEmpty());
}

TEST_F(SourceProbeTest, UnavailableProbeCarriesNoBeamFiguresAtAll)
{
   const QVariantMap probe = Pane()->probeSourceAt(35.5, -97.5);

   // Absent, not zero. A zero would format as "0 m / 0 ft" and read as a measured beam altitude
   // at ground level - the exact class of confidently-wrong output §4.7 exists to prevent.
   EXPECT_FALSE(probe.contains(QStringLiteral("beamCenterAltitudeMslMeters")));
   EXPECT_FALSE(probe.contains(QStringLiteral("beamCenterMslText")));
   EXPECT_FALSE(probe.contains(QStringLiteral("rangeMeters")));
   EXPECT_FALSE(probe.contains(QStringLiteral("elevationAngleDegrees")));
}

} // namespace nimbus::panes::test
