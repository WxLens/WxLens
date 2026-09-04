#include <wxlens/data/radar_site_marker_source.hpp>

#include <gtest/gtest.h>

namespace wxlens::data::test
{
namespace
{
RadarSiteInfo Site(const char* id, double x, double y, RadarSiteType type = RadarSiteType::Wsr88d)
{
   RadarSiteInfo site {};
   site.id = id; site.latitude = y; site.longitude = x; site.type = type;
   return site;
}
const auto kProject = [](double latitude, double longitude) { return QPointF{longitude, latitude}; };
} // namespace

TEST(RadarSiteMarkerSourceTest, CullsBeyondMarginButKeepsBoundary)
{
   const std::vector<RadarSiteInfo> sites {Site("IN", 50, 50), Site("EDGE", -10, 50),
                                           Site("OUT", -10.01, 50)};
   const auto result = RadarSiteMarkerSource::Cull(sites, kProject, 100, 100, 10, true);
   ASSERT_EQ(result.size(), 2u);
   EXPECT_EQ(result[0].site.id, "IN");
   EXPECT_EQ(result[1].site.id, "EDGE");
}

TEST(RadarSiteMarkerSourceTest, TdwrFilterOnlyAffectsMarkers)
{
   const std::vector<RadarSiteInfo> sites {Site("KAAA", 10, 10),
                                           Site("TAAA", 20, 20, RadarSiteType::Tdwr)};
   EXPECT_EQ(RadarSiteMarkerSource::Cull(sites, kProject, 100, 100, 0, true).size(), 2u);
   EXPECT_EQ(RadarSiteMarkerSource::Cull(sites, kProject, 100, 100, 0, false).size(), 1u);
}

TEST(RadarSiteMarkerSourceTest, NearestWinsAndEqualDistanceUsesLexicalId)
{
   std::vector<ProjectedRadarSite> sites {{Site("KBBB", 0, 0), QPointF{9, 10}},
                                          {Site("KAAA", 0, 0), QPointF{11, 10}}};
   auto result = RadarSiteMarkerSource::Nearest(sites, QPointF{10, 10}, 4);
   ASSERT_TRUE(result);
   EXPECT_EQ(result->site.id, "KAAA");
   sites[0].pixel = QPointF{10.5, 10};
   result = RadarSiteMarkerSource::Nearest(sites, QPointF{10, 10}, 4);
   ASSERT_TRUE(result);
   EXPECT_EQ(result->site.id, "KBBB");
}

TEST(RadarSiteMarkerSourceTest, OutsideToleranceSelectsNothing)
{
   // A click that hits no site must fall through to the map (ROADMAP.md 4.10), including when
   // the only candidate is the first entry in the culled list.
   const std::vector<ProjectedRadarSite> sites {{Site("KAAA", 0, 0), QPointF{80, 80}},
                                                {Site("KBBB", 0, 0), QPointF{40, 40}}};
   EXPECT_FALSE(RadarSiteMarkerSource::Nearest(sites, QPointF{10, 10}, 12));
   const auto hit = RadarSiteMarkerSource::Nearest(sites, QPointF{44, 44}, 12);
   ASSERT_TRUE(hit);
   EXPECT_EQ(hit->site.id, "KBBB");
}

} // namespace wxlens::data::test
