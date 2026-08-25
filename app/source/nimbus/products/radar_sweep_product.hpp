#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>

#include <cstdint>
#include <memory>
#include <optional>
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

   float dataMomentOffset {0.0f};
   float dataMomentScale {1.0f};
};

/** Small, independently replaceable palette snapshot; changing it never rebuilds geometry. */
struct ColorTableLut
{
   std::vector<boost::gil::rgba8_pixel_t> colors;
   std::uint16_t                          minimum {0};
   std::uint16_t                          maximum {0};
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
                              double             siteAltitudeMslMeters,
                              QObject*           parent = nullptr);
   ~RadarSweepProduct() override;

   /**
    * Per-site singleton, mirroring RadarSiteDataService::Instance's pattern one level up the
    * pipeline (docs/ROADMAP.md §4.6): every pane showing the same site shares one computed sweep
    * rather than each recomputing identical geometry. Looks the site's coordinates up via
    * data::FindRadarSite; returns nullptr for an unknown site id.
    *
    * Requesting the underlying data load is this product's own job (it is the Data Product for
    * that site), so simply obtaining an instance is enough to start a fetch - callers must not
    * depend on some other object having triggered one first.
    */
   static std::shared_ptr<RadarSweepProduct> Instance(const std::string& radarSite);

   [[nodiscard]] const std::string& radar_site() const;
   [[nodiscard]] double             site_latitude() const;
   [[nodiscard]] double             site_longitude() const;

   /// Antenna altitude above MSL, metres - the `height` input to the beam model (§4.7). Comes
   /// from data::FindRadarSite, which converts the bundled list's feet; see that header for what
   /// is and is not known about this figure.
   [[nodiscard]] double site_altitude_msl_meters() const;

   /**
    * The tilt of the cut the current sweep was built from, or nullopt when no sweep has been
    * computed yet.
    *
    * Optional on purpose. "No data loaded" and "0.5°" are different answers, and §4.7's
    * beam-geometry readout has to be able to say the first one - a default would make the UI
    * report an angle the radar never used, at a moment when it has no data at all.
    */
   [[nodiscard]] std::optional<double> elevation_angle_degrees() const;

   RadarSweepProduct(const RadarSweepProduct&)            = delete;
   RadarSweepProduct& operator=(const RadarSweepProduct&) = delete;
   RadarSweepProduct(RadarSweepProduct&&)                 = delete;
   RadarSweepProduct& operator=(RadarSweepProduct&&)      = delete;

   /**
    * A thread-safe snapshot of the latest computed sweep, or nullptr if none has been computed
    * yet. Safe to call from any thread (the render thread included) - see SweepData's comment.
    */
   [[nodiscard]] std::shared_ptr<const SweepData> sweep_data() const;
   [[nodiscard]] std::shared_ptr<const ColorTableLut> color_table_lut() const;

signals:
   /// Emitted after a new SweepData snapshot is published; sweep_data() will return it.
   void SweepUpdated();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace products
} // namespace nimbus
