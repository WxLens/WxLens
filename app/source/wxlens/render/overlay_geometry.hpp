#pragma once

#include <QVariantList>

#include <cstdint>
#include <memory>
#include <vector>

namespace wxlens
{
namespace render
{

/**
 * One vertex of the weather-overlay triangle list drawn by OverlayLayer.
 *
 * Fills and strokes share this one format so that every warning polygon and placefile item can
 * be drawn in a single glDrawArrays(GL_TRIANGLES) call in its original painter's order (fill,
 * then that item's outline, then the next item):
 *
 *  - A fill vertex has halfWidthPx == 0 and is projected as-is.
 *  - A stroke vertex is one corner of a screen-space quad around a line segment. It is anchored
 *    at one segment endpoint (latitude/longitude), knows the other endpoint (otherLatitude/
 *    otherLongitude), and is pushed sideways by side * halfWidthPx pixels in the vertex shader,
 *    plus halfWidthPx further along the segment away from the other end (a square cap, which
 *    closes the notch where two segments meet). Doing the extrusion in pixels after projection
 *    is what keeps line width constant across zoom without rebuilding geometry, and what lets
 *    lines be wider than the 1px most core-profile drivers clamp glLineWidth to.
 *
 * Positions are geographic and the camera is applied entirely in the shader, so this geometry is
 * built only when the overlay data or this pane's filter changes - never on camera movement.
 */
struct OverlayVertex
{
   float latitude {0.0f};
   float longitude {0.0f};
   float otherLatitude {0.0f};
   float otherLongitude {0.0f};
   float side {0.0f};
   float halfWidthPx {0.0f};
   std::uint8_t r {255};
   std::uint8_t g {255};
   std::uint8_t b {255};
   std::uint8_t a {255};
};

/**
 * Immutable once published - the same handoff rule as SweepData (products/radar_sweep_product.hpp):
 * a std::shared_ptr<const OverlayGeometry> passes from the GUI thread to the render thread with
 * no lock beyond the pointer swap, and OverlayLayer re-uploads only when that pointer changes.
 */
struct OverlayGeometry
{
   std::vector<OverlayVertex> vertices;
};

struct OverlayBuildOptions
{
   bool   warningsVisible {true};
   bool   placefilesVisible {true};
   bool   nearbyWarningsOnly {false};
   double homeLatitude {0.0};
   double homeLongitude {0.0};
   double nearbyRangeMeters {0.0};

   bool operator==(const OverlayBuildOptions&) const = default;
};

/**
 * Converts OverlayManager's warningPolygons/placefileItems lists into one triangle list.
 *
 * Styling matches what the QML Canvas renderer drew before this replaced it: warnings get a 12%
 * fill and a 3px outline at 95%; placefile polygons a 35% fill and a 1px outline at 90%; placefile
 * lines their own width at full opacity; placefile triangles a solid fill. Placefile polygon
 * contours after the first are treated as holes (GR placefile semantics), which the canvas did
 * not do. Text and icon items carry no vector geometry and are skipped - they stay QML items.
 */
[[nodiscard]] std::shared_ptr<const OverlayGeometry>
BuildOverlayGeometry(const QVariantList&        warningPolygons,
                     const QVariantList&        placefileItems,
                     const OverlayBuildOptions& options);

} // namespace render
} // namespace wxlens
