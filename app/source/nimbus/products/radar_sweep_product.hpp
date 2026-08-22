#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>
#include <QObject>

namespace nimbus
{
namespace products
{

/**
 * One computed radar sweep's worth of GPU-ready geometry: triangle-fan/quad vertices (lat/lon
 * pairs, one per gate corner - docs/ROADMAP.md §7 Phase 1 slice 3, ported from the legacy app's
 * view::Level2ProductView::ComputeSweep), the raw data moment value per vertex, and the color
 * table lookup texture data. Immutable once published by RadarSweepProduct, so a
 * std::shared_ptr<const SweepData> can be handed across threads (GUI thread computes it, the
 * render thread reads it) without locking beyond the pointer handoff itself.
 */
struct SweepData
{
   std::vector<float>         vertices; ///< (latitude, longitude) pairs, 2 floats/vertex
   std::vector<std::uint8_t>  dataMoments8;  ///< populated when the moment word size is 8 bits
   std::vector<std::uint16_t> dataMoments16; ///< populated when the moment word size is 16 bits

   std::vector<boost::gil::rgba8_pixel_t> colorTableLut;
   std::uint16_t                          colorTableMin {0};
   std::uint16_t                          colorTableMax {0};
};

/**
 * The Data Product layer (docs/ROADMAP.md §0.1 principle #4, §4.6) for one radar site's
 * reflectivity sweep. Listens for LevelTwoDataLoaded on that site's RadarSiteDataService (the
 * Data Source), and turns the raw Ar2vFile into renderable geometry a Visualization Layer
 * (RadarSweepLayer) can upload to the GPU as-is.
 *
 * Deliberately narrow for this slice, matching RadarSiteDataService's own "minimal first version"
 * precedent: hardcoded to Level 2 Reflectivity (wsr88d::rda::DataBlockType::MomentRef), the
 * lowest elevation cut, and no smoothing/CFP/other-units handling (all present in the legacy
 * Level2ProductView but real settings-driven features, not yet ported). Full product/elevation
 * selection is exactly what the real Data Product layer gains once PaneController exists
 * (slice 4+) to drive it - this is a bridge proving the geometry pipeline, not that layer.
 */
class RadarSweepProduct : public QObject
{
   Q_OBJECT

public:
   explicit RadarSweepProduct(const std::string& radarSite,
                              double             siteLatitude,
                              double             siteLongitude,
                              QObject*           parent = nullptr);
   ~RadarSweepProduct() override;

   RadarSweepProduct(const RadarSweepProduct&)            = delete;
   RadarSweepProduct& operator=(const RadarSweepProduct&) = delete;
   RadarSweepProduct(RadarSweepProduct&&)                 = delete;
   RadarSweepProduct& operator=(RadarSweepProduct&&)      = delete;

   /**
    * A thread-safe snapshot of the latest computed sweep, or nullptr if none has been computed
    * yet. Safe to call from any thread (the render thread included) - see SweepData's comment.
    */
   [[nodiscard]] std::shared_ptr<const SweepData> sweep_data() const;

signals:
   /// Emitted after a new SweepData snapshot is published; sweep_data() will return it.
   void SweepUpdated();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace products
} // namespace nimbus
