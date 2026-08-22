#pragma once

#include <nimbus/products/product_descriptor.hpp>

#include <memory>

#include <QMapLibre/Map>
#include <QObject>
#include <QString>

namespace nimbus
{
namespace panes
{

/**
 * One pane in the grid: a View (docs/ROADMAP.md §0.1 principle #4, §4.6). Owns what it displays
 * (a products::ProductDescriptor - never a `radarSite` field, per §4.6's audit note) and its own
 * camera state, both fully independent of every other pane.
 *
 * **No synchronization here yet.** Per-channel sync (§4.1-4.2) is slice 5; this slice
 * deliberately builds the multi-pane structure with every pane independent, so sync layers on top
 * of a working grid instead of being designed against a single-pane special case. The camera
 * properties below exist now precisely because slice 5 and the persistence schema (§4.6) both
 * need the pane - not the QML item - to be where camera state lives.
 */
class PaneController : public QObject
{
   Q_OBJECT

   Q_PROPERTY(int paneId READ paneId CONSTANT)

   Q_PROPERTY(QString productKind READ productKind NOTIFY productChanged)
   Q_PROPERTY(QString sourceKey READ sourceKey WRITE setSourceKey NOTIFY productChanged)
   Q_PROPERTY(QString productName READ productName NOTIFY productChanged)

   // The pane's data source's own coordinates (a radar site's location today) - used to centre a
   // freshly created pane on something meaningful. Named for the role, not the domain, so a
   // non-radar source can answer it too.
   Q_PROPERTY(double homeLatitude READ homeLatitude NOTIFY productChanged)
   Q_PROPERTY(double homeLongitude READ homeLongitude NOTIFY productChanged)

   Q_PROPERTY(double centerLatitude READ centerLatitude WRITE setCenterLatitude NOTIFY cameraChanged)
   Q_PROPERTY(
      double centerLongitude READ centerLongitude WRITE setCenterLongitude NOTIFY cameraChanged)
   Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY cameraChanged)
   Q_PROPERTY(double bearing READ bearing WRITE setBearing NOTIFY cameraChanged)
   Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY cameraChanged)

public:
   explicit PaneController(int                                 paneId,
                           const products::ProductDescriptor& descriptor,
                           QObject*                            parent = nullptr);
   ~PaneController() override;

   PaneController(const PaneController&)            = delete;
   PaneController& operator=(const PaneController&) = delete;
   PaneController(PaneController&&)                 = delete;
   PaneController& operator=(PaneController&&)      = delete;

   [[nodiscard]] int paneId() const;

   [[nodiscard]] QString productKind() const;
   [[nodiscard]] QString sourceKey() const;
   [[nodiscard]] QString productName() const;
   void                  setSourceKey(const QString& sourceKey);

   [[nodiscard]] double homeLatitude() const;
   [[nodiscard]] double homeLongitude() const;

   [[nodiscard]] double centerLatitude() const;
   [[nodiscard]] double centerLongitude() const;
   [[nodiscard]] double zoom() const;
   [[nodiscard]] double bearing() const;
   [[nodiscard]] double pitch() const;

   void setCenterLatitude(double value);
   void setCenterLongitude(double value);
   void setZoom(double value);
   void setBearing(double value);
   void setPitch(double value);

   /**
    * Registers this pane's Visualization Layer(s) on its map. Called from QML once the map's
    * style has loaded - custom layers added before that are silently dropped (see
    * docs/adr/0004-maplibre-qml-integration.md's slice 3 findings).
    */
   Q_INVOKABLE void attachLayers(QMapLibre::Map* map);

signals:
   void productChanged();
   void cameraChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace panes
} // namespace nimbus
