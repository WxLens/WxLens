// Tests for radar beam geometry (docs/ROADMAP.md §4.7).
//
// The thing worth testing hardest is that curvature is actually in the model. A flat-earth ray
// (range * tan(elevation)) returns a plausible-looking altitude that is wrong by kilometres at
// long range, and no "is it non-zero" test would catch that - so the expectations here are pinned
// against independently computed values and against the flat-ray figure it must exceed.

#include <wxlens/util/radar_geometry.hpp>

#include <cmath>
#include <numbers>
#include <optional>

#include <gtest/gtest.h>

namespace wxlens::util::test
{

namespace
{
/// What a straight ray over a flat earth would give - the approximation §4.7 forbids.
double FlatEarthAltitude(double rangeMeters, double elevationDegrees)
{
   return rangeMeters * std::tan(elevationDegrees * std::numbers::pi / 180.0);
}
} // namespace

TEST(RadarGeometryTest, BeamAltitudeMatchesTheFourThirdsEarthModel)
{
   // Computed independently from the 4/3-earth expression with R = 6367444 * 4/3 m. These are the
   // numbers a WSR-88D beam-height table gives for the lowest tilt, which is the check that
   // matters: 0.5° at 100 km sampling ~1.46 km is a figure a radar meteorologist knows by heart,
   // and a broken port would not land on it by accident.
   EXPECT_NEAR(BeamAltitudeMsl(100'000.0, 0.5, 0.0), 1461.461, 0.05);
   EXPECT_NEAR(BeamAltitudeMsl(200'000.0, 0.5, 0.0), 4100.050, 0.05);
   EXPECT_NEAR(BeamAltitudeMsl(50'000.0, 0.5, 0.0), 583.540, 0.05);
   EXPECT_NEAR(BeamAltitudeMsl(100'000.0, 1.5, 0.0), 3206.023, 0.05);
}

TEST(RadarGeometryTest, CurvatureIsIncludedNotApproximatedAway)
{
   // Earth curvature lifts the beam well above a straight ray, and the gap grows with range. At
   // 200 km the flat ray is ~1745 m and the real answer is ~4100 m - a flat approximation would
   // under-report the sampling height by more than 2 km.
   for (const double range : {50'000.0, 100'000.0, 200'000.0, 230'000.0})
   {
      EXPECT_GT(BeamAltitudeMsl(range, 0.5, 0.0), FlatEarthAltitude(range, 0.5)) << "range " << range;
   }

   const double gapNear = BeamAltitudeMsl(50'000.0, 0.5, 0.0) - FlatEarthAltitude(50'000.0, 0.5);
   const double gapFar = BeamAltitudeMsl(200'000.0, 0.5, 0.0) - FlatEarthAltitude(200'000.0, 0.5);
   EXPECT_GT(gapFar, gapNear * 4.0);
}

TEST(RadarGeometryTest, SiteAltitudeShiftsTheWholeProfile)
{
   // At zero range the beam is exactly at the antenna.
   EXPECT_NEAR(BeamAltitudeMsl(0.0, 0.5, 333.0), 333.0, 1e-6);
   EXPECT_NEAR(BeamAltitudeMsl(0.0, 0.5, 0.0), 0.0, 1e-6);

   // Away from the antenna a site's own height is still very nearly a constant offset, which is
   // what makes beam-centre-above-radar the honest figure to report without terrain data.
   const double atSeaLevel = BeamAltitudeMsl(100'000.0, 0.5, 0.0);
   const double onAHill    = BeamAltitudeMsl(100'000.0, 0.5, 333.0);
   EXPECT_NEAR(onAHill - atSeaLevel, 333.0, 1.0);
}

TEST(RadarGeometryTest, AltitudeRisesWithRangeAndWithTilt)
{
   EXPECT_GT(BeamAltitudeMsl(100'000.0, 0.5, 0.0), BeamAltitudeMsl(50'000.0, 0.5, 0.0));
   EXPECT_GT(BeamAltitudeMsl(100'000.0, 1.5, 0.0), BeamAltitudeMsl(100'000.0, 0.5, 0.0));

   // A tilt of exactly zero still climbs, purely from curvature - the model must not collapse to
   // "zero angle means zero height".
   EXPECT_GT(BeamAltitudeMsl(100'000.0, 0.0, 0.0), 500.0);
}

TEST(RadarGeometryTest, ProbeReportsRangeAndCompassAzimuth)
{
   // Due north of KTLX's coordinates, one degree of latitude away.
   const RadarBeamProbe north =
      ProbeRadarBeam(35.333361, -97.277761, 389.0, 0.5, 36.333361, -97.277761);

   EXPECT_GT(north.rangeMeters, 110'000.0);
   EXPECT_LT(north.rangeMeters, 112'000.0);
   EXPECT_NEAR(north.azimuthDegrees, 0.0, 0.5);

   // Due west. GeodesicInverse reports this as -90; an azimuth off a radar must read as 270.
   const RadarBeamProbe west =
      ProbeRadarBeam(35.333361, -97.277761, 389.0, 0.5, 35.333361, -98.500000);

   EXPECT_NEAR(west.azimuthDegrees, 270.0, 0.5);
   EXPECT_GE(west.azimuthDegrees, 0.0);
   EXPECT_LT(west.azimuthDegrees, 360.0);
}

TEST(RadarGeometryTest, ProbeSeparatesMslFromAboveRadarLevel)
{
   constexpr double kSiteAltitude = 389.0;

   const RadarBeamProbe probe = ProbeRadarBeam(
      35.333361, -97.277761, kSiteAltitude, 0.5, 36.333361, -97.277761);

   ASSERT_TRUE(probe.elevationAngleKnown);
   EXPECT_DOUBLE_EQ(probe.siteAltitudeMslMeters, kSiteAltitude);

   // The two figures must differ by exactly the site altitude. Conflating them is the specific
   // mistake §4.7 calls out - an above-radar-level number presented as MSL (or vice versa) is
   // wrong by the height of the tower's hill and looks entirely reasonable.
   EXPECT_NEAR(probe.beamCenterAltitudeMslMeters - probe.beamCenterAboveRadarMeters,
               kSiteAltitude,
               1e-6);
   EXPECT_LT(probe.beamCenterAboveRadarMeters, probe.beamCenterAltitudeMslMeters);
}

TEST(RadarGeometryTest, ProbeWithoutAnElevationAngleReportsNothingDownstreamOfIt)
{
   // No sweep loaded. Range and azimuth are still real - they need no radar data at all - but
   // every beam figure must stay unknown rather than defaulting to a plausible 0.5° tilt.
   const RadarBeamProbe probe = ProbeRadarBeam(
      35.333361, -97.277761, 389.0, std::nullopt, 36.333361, -97.277761);

   EXPECT_FALSE(probe.elevationAngleKnown);
   EXPECT_DOUBLE_EQ(probe.beamCenterAltitudeMslMeters, 0.0);
   EXPECT_DOUBLE_EQ(probe.beamCenterAboveRadarMeters, 0.0);

   EXPECT_GT(probe.rangeMeters, 110'000.0);
   EXPECT_NEAR(probe.azimuthDegrees, 0.0, 0.5);
}

} // namespace wxlens::util::test
