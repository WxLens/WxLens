#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scwx { namespace wsr88d { class Level3File; } }

namespace wxlens
{
namespace products
{

enum class GraphicOverlayKind
{
   Polyline,
   StormId,
   Hail,
   Mesocyclone,
   PointFeature,
   PointSymbol,
   StiCircle
};

struct GeographicPoint
{
   double latitude {};
   double longitude {};
};

/** Presentation-neutral meteorological geometry. It is intentionally not a MapObject. */
struct GraphicOverlayPrimitive
{
   GraphicOverlayKind            kind {GraphicOverlayKind::PointSymbol};
   std::vector<GeographicPoint> geometry;
   std::string                   stormId;
   std::string                   label;
   std::optional<std::uint16_t> value;
   bool                          forecast {false};
   bool                          past {false};
   double                        radiusMeters {};
};

struct Level3GraphicOverlaySnapshot
{
   std::vector<GraphicOverlayPrimitive> primitives;
   std::chrono::system_clock::time_point productTime {};
   std::vector<std::uint16_t> unsupportedPacketCodes;
};

/** Converts every graphic packet whose decoded geometry wxdata publicly exposes. */
[[nodiscard]] std::optional<Level3GraphicOverlaySnapshot>
BuildLevel3GraphicOverlaySnapshot(const scwx::wsr88d::Level3File& file);

} // namespace products
} // namespace wxlens
