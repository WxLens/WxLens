#pragma once

#include <wxlens/render/overlay_geometry.hpp>

#include <memory>
#include <mutex>

#include <QMapLibre/Types>

namespace wxlens
{
namespace render
{

/**
 * Push-style handoff from the GUI thread (PaneController rebuilds geometry when overlay data or
 * the pane's warning filter changes) to the render thread (OverlayLayer reads whatever is
 * current each frame).
 */
class OverlayLayerBinding
{
public:
   void setData(std::shared_ptr<const OverlayGeometry> data);
   [[nodiscard]] std::shared_ptr<const OverlayGeometry> data() const;

private:
   mutable std::mutex                     mutex_;
   std::shared_ptr<const OverlayGeometry> data_;
};

/**
 * The Visualization Layer (docs/ROADMAP.md §4.6) for warning polygons and placefile vector items,
 * registered directly above the radar sweep so it keeps its §4.3 place in the stack (above radar,
 * below every QML analysis layer).
 *
 * Replaces the QML Canvas that repainted and re-projected every vertex in JavaScript on every
 * camera tick. Geometry is uploaded once per data change (pointer comparison, as in
 * RadarSweepLayer) and each frame is one draw call: the camera reaches the GPU only as uniforms,
 * the same MVP construction RadarSweepLayer uses, so panning and zooming cost no CPU work here and
 * the overlay moves in lockstep with the radar it sits on.
 */
class OverlayLayer : public QMapLibre::CustomLayerHostInterface
{
public:
   explicit OverlayLayer(std::shared_ptr<OverlayLayerBinding> binding);
   ~OverlayLayer() override;

   OverlayLayer(const OverlayLayer&)            = delete;
   OverlayLayer& operator=(const OverlayLayer&) = delete;
   OverlayLayer(OverlayLayer&&)                 = delete;
   OverlayLayer& operator=(OverlayLayer&&)      = delete;

   void initialize() override;
   void render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace render
} // namespace wxlens
