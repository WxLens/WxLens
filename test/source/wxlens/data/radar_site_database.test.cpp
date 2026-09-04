// Tests for the bundled radar site metadata lookup.
//
// These exist because of one specific defect: the loader named its altitude field "meters" and
// stored the JSON's raw value, which is feet. Nothing downstream noticed until §4.7's beam
// geometry made site altitude load-bearing, and nothing about the failure is visible - a site
// reads 3.28x too high and the beam altitudes look entirely plausible. So the conversion is
// pinned here against sites whose real elevation is not in dispute.

#include <wxlens/data/radar_site_database.hpp>

#include <gtest/gtest.h>

namespace wxlens::data::test
{

namespace
{
constexpr double kMetersPerFoot = 0.3048;
} // namespace

TEST(RadarSiteDatabaseTest, KnownSiteResolves)
{
   const auto site = FindRadarSite("KTLX");
   ASSERT_TRUE(site.has_value());

   EXPECT_EQ(site->id, "KTLX");
   EXPECT_EQ(site->state, "OK");
   EXPECT_NEAR(site->latitude, 35.3334, 0.001);
   EXPECT_NEAR(site->longitude, -97.2778, 0.001);
   EXPECT_EQ(site->timeZoneId, "America/Chicago");
   EXPECT_EQ(site->type, RadarSiteType::Wsr88d);
}

TEST(RadarSiteDatabaseTest, TdwrTypeIsPreserved)
{
   const auto site = FindRadarSite("TATL");
   ASSERT_TRUE(site.has_value());
   EXPECT_EQ(site->type, RadarSiteType::Tdwr);
   EXPECT_STREQ(RadarSiteTypeName(site->type), "tdwr");
}

TEST(RadarSiteDatabaseTest, UnknownSiteResolvesToNothing)
{
   EXPECT_FALSE(FindRadarSite("ZZZZ").has_value());
   EXPECT_FALSE(FindRadarSite("").has_value());
}

TEST(RadarSiteDatabaseTest, AltitudeIsConvertedFromTheListsFeet)
{
   const auto ktlx = FindRadarSite("KTLX");
   ASSERT_TRUE(ktlx.has_value());

   // radar_sites.json says 1278 for KTLX. As feet that is ~390 m, which matches Oklahoma City's
   // terrain; as metres it would put the radar higher than anything in the state.
   EXPECT_NEAR(ktlx->altitudeMslMeters, 1278.0 * kMetersPerFoot, 0.01);
   EXPECT_NEAR(ktlx->altitudeMslMeters, 389.5, 1.0);
}

TEST(RadarSiteDatabaseTest, AltitudesAreSaneAsMetresAcrossTheExtremes)
{
   // The two ends of the list are the clearest evidence for the unit, so they are the ones tested:
   // Nome sits essentially at sea level and Missoula's radar is on a mountain. Read as metres the
   // raw values (90 and 7978) would make Nome a hilltop site and Missoula higher than Denali.
   const auto nome = FindRadarSite("PAEC");
   ASSERT_TRUE(nome.has_value());
   EXPECT_GT(nome->altitudeMslMeters, 10.0);
   EXPECT_LT(nome->altitudeMslMeters, 60.0);

   const auto missoula = FindRadarSite("KMSX");
   ASSERT_TRUE(missoula.has_value());
   EXPECT_GT(missoula->altitudeMslMeters, 2000.0);
   EXPECT_LT(missoula->altitudeMslMeters, 2600.0);
}

} // namespace wxlens::data::test
