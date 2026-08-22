#pragma once

#include <nimbus/products/radar_sweep_product.hpp>

#include <map>
#include <memory>
#include <string>

#include <QMapLibre/Map>
#include <QObject>

namespace nimbus
{
namespace render
{

/**
 * QML-facing bridge that registers the RadarSweepLayer MapLibre custom layer for a radar site,
 * once the map's underlying QMapLibre::Map object exists and its style has finished loading
 * (called from PaneHost.qml's onStyleLoaded, via MapLibre.mapLibreMap() - see
 * external/patches/0005-mln-qt-expose-map-object.patch, and docs/ROADMAP.md §7 Phase 1 slice 3's
 * status notes on why styleLoaded and not mapReady). Owns the RadarSweepProduct(s) it creates, so
 * they outlive the custom layer that reads from them. Superseded once real pane-bound rendering
 * exists (slice 4+, once PaneController owns this instead), not extended in place.
 */
class RadarLayerController : public QObject
{
   Q_OBJECT

public:
   explicit RadarLayerController(QObject* parent = nullptr);

   Q_INVOKABLE void
   attachRadarSweep(QMapLibre::Map* map, const QString& siteId, double latitude, double longitude);

private:
   std::map<std::string, std::shared_ptr<products::RadarSweepProduct>> products_;
};

} // namespace render
} // namespace nimbus
