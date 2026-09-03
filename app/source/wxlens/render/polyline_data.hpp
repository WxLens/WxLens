#pragma once

#include <vector>

namespace wxlens
{
namespace render
{

/**
 * One endpoint of a line segment: a map coordinate plus its own RGBA color, so a single vertex
 * buffer can hold many independently-colored polylines (e.g. warning polygons colored by
 * phenomenon, placefile lines/polygon outlines colored per item, county boundaries).
 */
struct PolylineVertex
{
   float latitude {0.0f};
   float longitude {0.0f};
   float r {1.0f};
   float g {1.0f};
   float b {1.0f};
   float a {1.0f};
};

/**
 * GPU-ready line geometry: consecutive pairs of vertices are independent segments (GL_LINES), not
 * a connected strip - so unrelated rings/polylines can share one vertex buffer and one draw call
 * without needing primitive-restart indices. A closed ring is its own list of (vN, vN+1) pairs
 * ending back at v0, computed by whichever product builds this.
 *
 * Immutable once published, matching SweepData's rationale (products/radar_sweep_product.hpp): a
 * std::shared_ptr<const PolylineData> is safe to hand from the product's update thread to the
 * render thread with no lock beyond the pointer handoff.
 */
struct PolylineData
{
   std::vector<PolylineVertex> vertices;
};

} // namespace render
} // namespace wxlens
