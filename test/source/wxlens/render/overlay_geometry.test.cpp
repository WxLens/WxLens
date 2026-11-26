#include <wxlens/render/overlay_geometry.hpp>

#include <algorithm>
#include <cmath>
#include <span>

#include <QVariantMap>
#include <gtest/gtest.h>

namespace wxlens
{
namespace render
{
namespace
{

constexpr std::size_t kVerticesPerSegment = 6;

QVariantList Flat(std::initializer_list<std::pair<double, double>> points)
{
   QVariantList list;
   for (const auto& [lat, lon] : points)
   {
      list.append(lat);
      list.append(lon);
   }
   return list;
}

QVariantMap Warning(QVariantList coordinates,
                    double       latitude,
                    double       longitude,
                    QString      color = QStringLiteral("#ffff3b30"))
{
   return {{"coordinates", coordinates},
           {"latitude", latitude},
           {"longitude", longitude},
           {"color", color}};
}

// A square warning centred on (35, -97), roughly 11 km across.
QVariantMap SquareWarning(double lat = 35.0, double lon = -97.0)
{
   return Warning(Flat({{lat - 0.05, lon - 0.05},
                        {lat - 0.05, lon + 0.05},
                        {lat + 0.05, lon + 0.05},
                        {lat + 0.05, lon - 0.05}}),
                  lat,
                  lon);
}

std::size_t CountFills(const OverlayGeometry& geometry)
{
   return static_cast<std::size_t>(std::ranges::count_if(
      geometry.vertices, [](const OverlayVertex& v) { return v.halfWidthPx == 0.0f; }));
}

std::size_t CountStrokes(const OverlayGeometry& geometry)
{
   return geometry.vertices.size() - CountFills(geometry);
}

} // namespace

TEST(OverlayGeometry, EmptyInputBuildsNothing)
{
   const auto geometry = BuildOverlayGeometry({}, {}, {});
   ASSERT_NE(geometry, nullptr);
   EXPECT_TRUE(geometry->vertices.empty());
}

TEST(OverlayGeometry, WarningIsFilledAndOutlinedWithCanvasStyling)
{
   const auto geometry = BuildOverlayGeometry({SquareWarning()}, {}, {});

   // A quad triangulates to two triangles; its closed outline is four segments.
   EXPECT_EQ(CountFills(*geometry), 6u);
   EXPECT_EQ(CountStrokes(*geometry), 4 * kVerticesPerSegment);

   // Fill first, then outline, as the Canvas painted them.
   EXPECT_EQ(geometry->vertices.front().halfWidthPx, 0.0f);
   EXPECT_GT(geometry->vertices.back().halfWidthPx, 0.0f);

   for (const auto& v : geometry->vertices)
   {
      EXPECT_EQ(v.r, 0xff);
      EXPECT_EQ(v.g, 0x3b);
      EXPECT_EQ(v.b, 0x30);
      if (v.halfWidthPx == 0.0f)
      {
         EXPECT_EQ(v.a, std::lround(255 * 0.12)); // 12% fill
      }
      else
      {
         EXPECT_EQ(v.a, std::lround(255 * 0.95)); // 95% outline
         EXPECT_FLOAT_EQ(v.halfWidthPx, 1.5f);    // 3px outline
      }
   }
}

TEST(OverlayGeometry, ExplicitlyClosedRingIsNotDoubleClosed)
{
   auto warning = SquareWarning();
   auto coordinates = warning.value("coordinates").toList();
   coordinates.append(coordinates[0]);
   coordinates.append(coordinates[1]);
   warning.insert("coordinates", coordinates);

   const auto geometry = BuildOverlayGeometry({warning}, {}, {});
   EXPECT_EQ(CountFills(*geometry), 6u);
   EXPECT_EQ(CountStrokes(*geometry), 4 * kVerticesPerSegment);
}

TEST(OverlayGeometry, StrokeCornersStraddleTheirSegment)
{
   const auto geometry = BuildOverlayGeometry({SquareWarning()}, {}, {});
   std::size_t corners = 0;
   for (const auto& v : geometry->vertices)
   {
      if (v.halfWidthPx == 0.0f) continue;
      ++corners;
      EXPECT_TRUE(v.side == 1.0f || v.side == -1.0f);
      EXPECT_FALSE(v.latitude == v.otherLatitude && v.longitude == v.otherLongitude);
   }
   EXPECT_GT(corners, 0u);

   // Each quad's two triangles use both sides at both ends: (a,+1) (a,-1) (b,-1) / (a,-1) (b,+1) (b,-1).
   const auto first = std::ranges::find_if(geometry->vertices,
                                           [](const auto& v) { return v.halfWidthPx > 0.0f; });
   ASSERT_NE(first, geometry->vertices.end());
   const auto quad = std::span(first, kVerticesPerSegment);
   EXPECT_EQ(quad[0].side, +1.0f);
   EXPECT_EQ(quad[1].side, -1.0f);
   EXPECT_EQ(quad[2].side, -1.0f);
   EXPECT_EQ(quad[4].side, +1.0f);
   EXPECT_EQ(quad[0].latitude, quad[2].otherLatitude);
   EXPECT_EQ(quad[2].latitude, quad[0].otherLatitude);
}

TEST(OverlayGeometry, ConcaveWarningTriangulatesWithoutOverfilling)
{
   // An L shape: six vertices, so four triangles if triangulated correctly.
   const auto warning = Warning(Flat({{35.0, -97.0},
                                      {35.0, -96.8},
                                      {35.1, -96.8},
                                      {35.1, -96.9},
                                      {35.2, -96.9},
                                      {35.2, -97.0}}),
                                35.1,
                                -96.9);
   const auto geometry = BuildOverlayGeometry({warning}, {}, {});
   EXPECT_EQ(CountFills(*geometry), 4 * 3u);
   EXPECT_EQ(CountStrokes(*geometry), 6 * kVerticesPerSegment);
}

TEST(OverlayGeometry, DegenerateWarningsAreSkipped)
{
   const auto twoPoints = Warning(Flat({{35.0, -97.0}, {35.1, -97.0}}), 35.0, -97.0);
   const auto geometry  = BuildOverlayGeometry({twoPoints, QVariant()}, {}, {});
   EXPECT_TRUE(geometry->vertices.empty());
}

TEST(OverlayGeometry, NearbyFilterKeepsOnlyWarningsWithinRange)
{
   const QVariantList warnings {SquareWarning(35.0, -97.0),  // at the site
                                SquareWarning(40.0, -105.0)}; // ~800 km away

   OverlayBuildOptions options;
   options.nearbyWarningsOnly = true;
   options.homeLatitude       = 35.2;
   options.homeLongitude      = -97.4;
   options.nearbyRangeMeters  = 230000.0;

   const auto nearby = BuildOverlayGeometry(warnings, {}, options);
   const auto all    = BuildOverlayGeometry(warnings, {}, {});
   EXPECT_EQ(all->vertices.size(), 2 * nearby->vertices.size());

   options.homeLatitude  = 0.0;
   options.homeLongitude = 0.0;
   EXPECT_TRUE(BuildOverlayGeometry(warnings, {}, options)->vertices.empty());
}

TEST(OverlayGeometry, VisibilityTogglesDropTheirLayer)
{
   const QVariantList placefile {
      QVariantMap {{"kind", "line"},
                   {"coordinates", Flat({{35.0, -97.0}, {35.1, -97.1}})},
                   {"color", "#ff00ff00"},
                   {"width", 2.0}}};

   OverlayBuildOptions options;
   options.warningsVisible = false;
   const auto onlyPlacefile = BuildOverlayGeometry({SquareWarning()}, placefile, options);
   EXPECT_EQ(onlyPlacefile->vertices.size(), kVerticesPerSegment);

   options.warningsVisible   = true;
   options.placefilesVisible = false;
   const auto onlyWarning = BuildOverlayGeometry({SquareWarning()}, placefile, options);
   EXPECT_EQ(CountStrokes(*onlyWarning), 4 * kVerticesPerSegment);
   EXPECT_EQ(CountFills(*onlyWarning), 6u);
}

TEST(OverlayGeometry, PlacefileLineIsOpenAndUsesItsOwnWidth)
{
   const QVariantList items {
      QVariantMap {{"kind", "line"},
                   {"coordinates", Flat({{35.0, -97.0}, {35.1, -97.1}, {35.2, -97.0}})},
                   {"color", "#8000ff00"},
                   {"width", 4.0}}};
   const auto geometry = BuildOverlayGeometry({}, items, {});

   // Three points, open: two segments, no closing segment.
   ASSERT_EQ(geometry->vertices.size(), 2 * kVerticesPerSegment);
   for (const auto& v : geometry->vertices)
   {
      EXPECT_FLOAT_EQ(v.halfWidthPx, 2.0f);
      EXPECT_EQ(v.g, 0xff);
      EXPECT_EQ(v.a, 0x80); // the item's own alpha, at full opacity
   }
}

TEST(OverlayGeometry, PlacefileTrianglesPassThroughAsSolidFill)
{
   const QVariantList items {QVariantMap {
      {"kind", "triangles"},
      {"coordinates",
       Flat({{35.0, -97.0}, {35.1, -97.0}, {35.0, -96.9}, {36.0, -97.0}, {36.1, -97.0},
             {36.0, -96.9}, {37.0, -97.0}})}, // trailing partial triangle is ignored
      {"color", "#ff0000ff"}}};
   const auto geometry = BuildOverlayGeometry({}, items, {});
   ASSERT_EQ(geometry->vertices.size(), 6u);
   for (const auto& v : geometry->vertices)
   {
      EXPECT_EQ(v.halfWidthPx, 0.0f);
      EXPECT_EQ(v.b, 0xff);
      EXPECT_EQ(v.a, 0xff);
   }
}

TEST(OverlayGeometry, PlacefilePolygonContoursAfterTheFirstAreHoles)
{
   const QVariantList outer = Flat({{35.0, -97.0}, {35.0, -96.0}, {36.0, -96.0}, {36.0, -97.0}});
   const QVariantList hole  = Flat({{35.4, -96.6}, {35.4, -96.4}, {35.6, -96.4}, {35.6, -96.6}});
   const QVariantList items {QVariantMap {
      {"kind", "polygon"}, {"contours", QVariantList {outer, hole}}, {"color", "#ffffffff"}}};
   const auto geometry = BuildOverlayGeometry({}, items, {});

   // A square with a square hole triangulates to eight triangles. Filling each contour on its own,
   // as the Canvas did, would have been four and would have painted over the hole.
   EXPECT_EQ(CountFills(*geometry), 8 * 3u);
   EXPECT_EQ(CountStrokes(*geometry), 8 * kVerticesPerSegment); // both rings outlined

   for (const auto& v : geometry->vertices)
   {
      EXPECT_EQ(v.a, v.halfWidthPx == 0.0f ? std::lround(255 * 0.35) : std::lround(255 * 0.9));
      if (v.halfWidthPx > 0.0f) EXPECT_FLOAT_EQ(v.halfWidthPx, 0.5f);
   }
}

TEST(OverlayGeometry, TextAndIconItemsHaveNoVectorGeometry)
{
   const QVariantList items {
      QVariantMap {{"kind", "text"}, {"latitude", 35.0}, {"longitude", -97.0}, {"text", "x"}},
      QVariantMap {{"kind", "icon"}, {"latitude", 35.0}, {"longitude", -97.0}}};
   EXPECT_TRUE(BuildOverlayGeometry({}, items, {})->vertices.empty());
}

// Ported from the QML test that covered this predicate while it lived in WeatherOverlaysLayer.
TEST(OverlayGeometry, PaneOverrideResolvesAgainstTheGlobalDefault)
{
   EXPECT_FALSE(ResolveNearbyWarningsOnly(0, false)) << "0 follows default off";
   EXPECT_TRUE(ResolveNearbyWarningsOnly(0, true)) << "0 follows default on";
   EXPECT_FALSE(ResolveNearbyWarningsOnly(1, true)) << "1 forces all against the default";
   EXPECT_TRUE(ResolveNearbyWarningsOnly(2, false)) << "2 forces nearby against the default";
   EXPECT_TRUE(ResolveNearbyWarningsOnly(7, true)) << "unknown values follow the default";
}

// The QML test's realistic case: KEAX, ~111 km (inside 230 km) vs ~333 km (outside).
TEST(OverlayGeometry, NearbyFilterUsesTheRangeAroundTheSite)
{
   const double siteLat = 38.810;
   const double siteLon = -94.264;
   const QVariantList inside {SquareWarning(siteLat + 1.0, siteLon)};
   const QVariantList outside {SquareWarning(siteLat + 3.0, siteLon)};

   OverlayBuildOptions options;
   options.nearbyWarningsOnly = true;
   options.homeLatitude       = siteLat;
   options.homeLongitude      = siteLon;
   options.nearbyRangeMeters  = 230000.0;

   EXPECT_FALSE(BuildOverlayGeometry(inside, {}, options)->vertices.empty());
   EXPECT_TRUE(BuildOverlayGeometry(outside, {}, options)->vertices.empty());
}

} // namespace render
} // namespace wxlens
