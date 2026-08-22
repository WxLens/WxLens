#pragma once

#include <QMapLibre/Map>
#include <QObject>

namespace nimbus
{
namespace render
{

/**
 * Phase 1 slice 3: minimal QML-facing bridge that registers the trivial
 * RadarSiteMarkerLayer proof-of-concept as a MapLibre custom layer, once the map's underlying
 * QMapLibre::Map object exists (called from PaneHost.qml's onMapReady, via
 * MapLibre.mapLibreMap() - see external/patches/0005-mln-qt-expose-map-object.patch). Superseded
 * once real product/pane-bound rendering exists (slice 4+), not extended in place.
 */
class RadarLayerController : public QObject
{
   Q_OBJECT

public:
   explicit RadarLayerController(QObject* parent = nullptr);

   Q_INVOKABLE void attachSiteMarker(QMapLibre::Map* map, double latitude, double longitude);
};

} // namespace render
} // namespace nimbus
