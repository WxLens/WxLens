#include <wxlens/render/overlay_geometry.hpp>

#include <array>
#include <cmath>
#include <numbers>

#include <QColor>
#include <QString>
#include <QVariantMap>

#include <mapbox/earcut.hpp>

namespace wxlens
{
namespace render
{

namespace
{

// The alpha multipliers and widths the QML Canvas renderer used, so moving to the GPU path does
// not change how anything looks.
constexpr float kWarningFillAlpha     = 0.12f;
constexpr float kWarningStrokeAlpha   = 0.95f;
constexpr float kWarningStrokeWidthPx = 3.0f;
constexpr float kPolygonFillAlpha     = 0.35f;
constexpr float kPolygonStrokeAlpha   = 0.9f;
constexpr float kPolygonStrokeWidthPx = 1.0f;

struct Rgba
{
   std::uint8_t r {255};
   std::uint8_t g {255};
   std::uint8_t b {255};
   std::uint8_t a {255};
};

struct LatLon
{
   double latitude {0.0};
   double longitude {0.0};

   bool operator==(const LatLon&) const = default;
};

Rgba ColorFor(const QVariant& value, float alphaMultiplier)
{
   // OverlayManager publishes "#AARRGGBB", which QColor parses directly.
   const QColor color(value.toString());
   if (!color.isValid())
   {
      return {255, 255, 255, static_cast<std::uint8_t>(std::lround(255.0f * alphaMultiplier))};
   }
   return {static_cast<std::uint8_t>(color.red()),
           static_cast<std::uint8_t>(color.green()),
           static_cast<std::uint8_t>(color.blue()),
           static_cast<std::uint8_t>(
              std::lround(static_cast<float>(color.alpha()) * alphaMultiplier))};
}

std::vector<LatLon> Points(const QVariant& flatCoordinates)
{
   const QVariantList list = flatCoordinates.toList();
   std::vector<LatLon> points;
   points.reserve(static_cast<std::size_t>(list.size() / 2));
   for (qsizetype i = 0; i + 1 < list.size(); i += 2)
   {
      points.push_back({list[i].toDouble(), list[i + 1].toDouble()});
   }
   return points;
}

// A ring's explicit closing point (last == first) is dropped: outlines close themselves and
// earcut treats a repeated point as a degenerate edge.
std::vector<LatLon> Ring(const QVariant& flatCoordinates)
{
   std::vector<LatLon> ring = Points(flatCoordinates);
   if (ring.size() > 1 && ring.front() == ring.back())
   {
      ring.pop_back();
   }
   return ring;
}

double DistanceMeters(double lat1, double lon1, double lat2, double lon2)
{
   // Haversine, identical to the formula the QML layer used for the same filter.
   constexpr double toRad             = std::numbers::pi / 180.0;
   constexpr double earthRadiusMeters = 6371000.0;
   const double     dLat              = (lat2 - lat1) * toRad;
   const double     dLon              = (lon2 - lon1) * toRad;
   const double     a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                    std::cos(lat1 * toRad) * std::cos(lat2 * toRad) * std::sin(dLon / 2) *
                       std::sin(dLon / 2);
   return earthRadiusMeters * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

class Builder
{
public:
   explicit Builder(OverlayGeometry& geometry) : vertices_ {geometry.vertices} {}

   void Fill(const LatLon& point, Rgba color)
   {
      OverlayVertex v;
      v.latitude       = static_cast<float>(point.latitude);
      v.longitude      = static_cast<float>(point.longitude);
      v.otherLatitude  = v.latitude;
      v.otherLongitude = v.longitude;
      SetColor(v, color);
      vertices_.push_back(v);
   }

   // Triangulates in (longitude, Mercator y) rather than raw degrees, so the triangulation is
   // valid in the same plane the map draws it in. Rings after the first are holes.
   void FillPolygon(const std::vector<std::vector<LatLon>>& rings, Rgba color)
   {
      using Point = std::array<double, 2>;
      std::vector<std::vector<Point>> projected;
      std::vector<LatLon>             flat;
      for (const auto& ring : rings)
      {
         if (ring.size() < 3)
         {
            continue;
         }
         auto& out = projected.emplace_back();
         for (const auto& point : ring)
         {
            out.push_back({point.longitude, MercatorY(point.latitude)});
            flat.push_back(point);
         }
      }
      if (projected.empty())
      {
         return;
      }
      for (const auto index : mapbox::earcut<std::uint32_t>(projected))
      {
         Fill(flat[index], color);
      }
   }

   void Stroke(const std::vector<LatLon>& points, bool closed, float widthPx, Rgba color)
   {
      if (points.size() < 2 || widthPx <= 0.0f)
      {
         return;
      }
      for (std::size_t i = 0; i + 1 < points.size(); ++i)
      {
         Segment(points[i], points[i + 1], widthPx * 0.5f, color);
      }
      if (closed && points.size() > 2)
      {
         Segment(points.back(), points.front(), widthPx * 0.5f, color);
      }
   }

private:
   static double MercatorY(double latitude)
   {
      return std::log(std::tan(std::numbers::pi / 4.0 + latitude * std::numbers::pi / 360.0));
   }

   static void SetColor(OverlayVertex& v, Rgba color)
   {
      v.r = color.r;
      v.g = color.g;
      v.b = color.b;
      v.a = color.a;
   }

   OverlayVertex Corner(const LatLon& anchor, const LatLon& other, float side, float halfWidth,
                        Rgba color) const
   {
      OverlayVertex v;
      v.latitude       = static_cast<float>(anchor.latitude);
      v.longitude      = static_cast<float>(anchor.longitude);
      v.otherLatitude  = static_cast<float>(other.latitude);
      v.otherLongitude = static_cast<float>(other.longitude);
      v.side           = side;
      v.halfWidthPx    = halfWidth;
      SetColor(v, color);
      return v;
   }

   // Two triangles covering segment a-b. The shader's normal is perpendicular to the direction
   // from a vertex's anchor toward its other end, which is reversed at b - so b's corners take the
   // opposite side sign to land on the same edge of the quad as a's.
   void Segment(const LatLon& a, const LatLon& b, float halfWidth, Rgba color)
   {
      if (a == b)
      {
         return;
      }
      const OverlayVertex aLeft  = Corner(a, b, +1.0f, halfWidth, color);
      const OverlayVertex aRight = Corner(a, b, -1.0f, halfWidth, color);
      const OverlayVertex bLeft  = Corner(b, a, -1.0f, halfWidth, color);
      const OverlayVertex bRight = Corner(b, a, +1.0f, halfWidth, color);
      vertices_.insert(vertices_.end(), {aLeft, aRight, bLeft, aRight, bRight, bLeft});
   }

   std::vector<OverlayVertex>& vertices_;
};

} // namespace

std::shared_ptr<const OverlayGeometry>
BuildOverlayGeometry(const QVariantList&        warningPolygons,
                     const QVariantList&        placefileItems,
                     const OverlayBuildOptions& options)
{
   auto    geometry = std::make_shared<OverlayGeometry>();
   Builder builder {*geometry};

   if (options.warningsVisible)
   {
      for (const auto& value : warningPolygons)
      {
         const QVariantMap warning = value.toMap();
         if (options.nearbyWarningsOnly &&
             DistanceMeters(options.homeLatitude,
                            options.homeLongitude,
                            warning.value("latitude").toDouble(),
                            warning.value("longitude").toDouble()) > options.nearbyRangeMeters)
         {
            continue;
         }
         const auto ring = Ring(warning.value("coordinates"));
         if (ring.size() < 3)
         {
            continue;
         }
         const QVariant color = warning.value("color");
         builder.FillPolygon({ring}, ColorFor(color, kWarningFillAlpha));
         builder.Stroke(ring, true, kWarningStrokeWidthPx, ColorFor(color, kWarningStrokeAlpha));
      }
   }

   if (options.placefilesVisible)
   {
      for (const auto& value : placefileItems)
      {
         const QVariantMap item  = value.toMap();
         const QString     kind  = item.value("kind").toString();
         const QVariant    color = item.value("color");
         if (kind == QLatin1String("line"))
         {
            builder.Stroke(Points(item.value("coordinates")),
                           false,
                           static_cast<float>(item.value("width").toDouble()),
                           ColorFor(color, 1.0f));
         }
         else if (kind == QLatin1String("triangles"))
         {
            const auto points = Points(item.value("coordinates"));
            const Rgba fill   = ColorFor(color, 1.0f);
            for (std::size_t t = 0; t + 2 < points.size(); t += 3)
            {
               builder.Fill(points[t], fill);
               builder.Fill(points[t + 1], fill);
               builder.Fill(points[t + 2], fill);
            }
         }
         else if (kind == QLatin1String("polygon"))
         {
            std::vector<std::vector<LatLon>> rings;
            for (const auto& contour : item.value("contours").toList())
            {
               auto ring = Ring(contour);
               if (ring.size() >= 3)
               {
                  rings.push_back(std::move(ring));
               }
            }
            if (rings.empty())
            {
               continue;
            }
            builder.FillPolygon(rings, ColorFor(color, kPolygonFillAlpha));
            const Rgba stroke = ColorFor(color, kPolygonStrokeAlpha);
            for (const auto& ring : rings)
            {
               builder.Stroke(ring, true, kPolygonStrokeWidthPx, stroke);
            }
         }
      }
   }

   return geometry;
}

bool ResolveNearbyWarningsOnly(int warningsFilterOverride, bool globalDefault)
{
   switch (warningsFilterOverride)
   {
   case 1:
      return false;
   case 2:
      return true;
   default:
      return globalDefault;
   }
}

} // namespace render
} // namespace wxlens
