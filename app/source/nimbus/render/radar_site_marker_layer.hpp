#pragma once

#include <memory>

#include <QMapLibre/Types>

namespace nimbus
{
namespace render
{

/**
 * Phase 1 slice 3: a deliberately minimal MapLibre custom layer - draws one fixed-size colored
 * point at a hardcoded geo coordinate, nothing else. Proves the custom-layer rendering seam
 * (docs/ROADMAP.md §0.2 "prove the rendering seam early") end-to-end - registration via
 * QMapLibre::Map::addCustomLayer, a real GL context available inside render(), and the
 * lat/lon-to-screen projection (ported from the legacy app's gl/radar.vert) - before porting the
 * actual radar sweep vertex/shader pipeline (view::RadarProductView + gl/draw/radar.*), which
 * reuses this same registration mechanism and coordinate transform. Superseded by that renderer,
 * not extended in place.
 */
class RadarSiteMarkerLayer : public QMapLibre::CustomLayerHostInterface
{
public:
   RadarSiteMarkerLayer(double latitude, double longitude);
   ~RadarSiteMarkerLayer() override;

   RadarSiteMarkerLayer(const RadarSiteMarkerLayer&)            = delete;
   RadarSiteMarkerLayer& operator=(const RadarSiteMarkerLayer&) = delete;
   RadarSiteMarkerLayer(RadarSiteMarkerLayer&&)                 = delete;
   RadarSiteMarkerLayer& operator=(RadarSiteMarkerLayer&&)      = delete;

   void initialize() override;
   void render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace render
} // namespace nimbus
