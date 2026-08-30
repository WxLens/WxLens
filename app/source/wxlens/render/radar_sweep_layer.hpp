#pragma once

#include <wxlens/products/radar_sweep_product.hpp>

#include <memory>
#include <mutex>

#include <QMapLibre/Types>

namespace wxlens
{
namespace render
{

class RadarSweepLayerBinding
{
public:
   explicit RadarSweepLayerBinding(std::shared_ptr<products::RadarSweepProduct> product);

   void setProduct(std::shared_ptr<products::RadarSweepProduct> product);
   [[nodiscard]] std::shared_ptr<products::RadarSweepProduct> product() const;
   void setSnapshot(products::SweepSnapshot snapshot);
   [[nodiscard]] products::SweepSnapshot snapshot() const;

private:
   mutable std::mutex                            mutex_;
   std::shared_ptr<products::RadarSweepProduct> product_;
   products::SweepSnapshot                     snapshot_ {};
};

/**
 * The Visualization Layer (docs/ROADMAP.md §0.1 principle #4, §4.6) for one radar site's
 * reflectivity sweep - a real MapLibre custom layer, superseding the proof-of-concept
 * RadarSiteMarkerLayer now that the registration/GL-context/coordinate-transform seam it proved
 * is no longer in question. Ported from the legacy app's map::RadarProductLayer (radar_product_
 * layer.cpp): same MVP construction, same gl/radar.vert+.frag shaders (see app/res/gl/), same
 * vertex/data-moment/color-table-texture upload split - see RadarSweepProduct for what replaces
 * view::RadarProductView as the data source those uploads read from.
 *
 * Each render() call cheaply checks whether the product has published a newer SweepData snapshot
 * (pointer comparison - see SweepData's comment on why that's safe/cheap) and re-uploads GPU
 * buffers only when it has, rather than every frame.
 */
class RadarSweepLayer : public QMapLibre::CustomLayerHostInterface
{
public:
   explicit RadarSweepLayer(std::shared_ptr<RadarSweepLayerBinding> binding);
   ~RadarSweepLayer() override;

   RadarSweepLayer(const RadarSweepLayer&)            = delete;
   RadarSweepLayer& operator=(const RadarSweepLayer&) = delete;
   RadarSweepLayer(RadarSweepLayer&&)                 = delete;
   RadarSweepLayer& operator=(RadarSweepLayer&&)      = delete;

   void initialize() override;
   void render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void deinitialize() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace render
} // namespace wxlens
