#pragma once

// NOT YET WIRED INTO THE BUILD - not in app/CMakeLists.txt, no consumer.
//
// Salvaged 2026-09-02 from the abandoned "worktree-slice-12-warnings-placefiles" branch (commit
// 531c947, namespace renamed nimbus -> wxlens) before that branch was deleted as superseded by
// overlays::OverlayManager. That replacement renders placefile/warning geometry on a QML Canvas,
// which is fine at today's scale but will not hold up for a dense national dataset (e.g. county
// boundaries, per docs/ROADMAP.md's Phase 3 overlay backlog) - this GL custom-layer renderer is
// the intended starting point for that follow-up work. Carried over as source, not build-wired,
// because it has no consumer yet; wire it into app/CMakeLists.txt and app/res/gl's resource list
// when the first consumer (e.g. a CountyLayer data service) is built against it. Verify it still
// compiles against the current RadarSweepLayer/QMapLibre APIs before relying on it - it has not
// been rebuilt since the branch diverged.

#include <wxlens/render/polyline_data.hpp>

#include <memory>
#include <mutex>

#include <QMapLibre/Types>

namespace wxlens
{
namespace render
{

/**
 * Push-style binding for PolylineLayer: whoever computes new geometry calls setData() whenever it
 * republishes, and the render thread reads whatever's current via data(). No product pointer to
 * swap here (unlike RadarSweepLayerBinding) - a shared vector overlay (alerts, placefiles,
 * counties) is global, one instance answers every pane, so there is nothing pane-specific to
 * rebind.
 */
class PolylineLayerBinding
{
public:
   void setData(std::shared_ptr<const PolylineData> data);
   [[nodiscard]] std::shared_ptr<const PolylineData> data() const;

private:
   mutable std::mutex                  mutex_;
   std::shared_ptr<const PolylineData> data_;
};

/**
 * A generic Visualization Layer (docs/ROADMAP.md §4.6) for colored line geometry, meant to be
 * shared by every vector-overlay data source (warning polygon outlines, placefile Line/Polygon
 * items, county boundaries) rather than one near-identical GL layer per source. Same MVP
 * construction and lat/lon-delta screen transform as RadarSweepLayer (see that class's comment
 * and res/gl/radar.vert), reused verbatim in res/gl/polyline.vert since the projection math does
 * not depend on what is being drawn.
 *
 * Each vertex carries its own RGBA color (see PolylineVertex), so one draw call can render many
 * independently-styled rings/lines - e.g. every currently-active warning polygon, colored by
 * phenomenon, in a single glDrawArrays(GL_LINES, ...).
 */
class PolylineLayer : public QMapLibre::CustomLayerHostInterface
{
public:
   explicit PolylineLayer(std::shared_ptr<PolylineLayerBinding> binding);
   ~PolylineLayer() override;

   PolylineLayer(const PolylineLayer&)            = delete;
   PolylineLayer& operator=(const PolylineLayer&) = delete;
   PolylineLayer(PolylineLayer&&)                 = delete;
   PolylineLayer& operator=(PolylineLayer&&)      = delete;

   void initialize() override;
   void render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace render
} // namespace wxlens
