#pragma once

#include <wxlens/panes/sync_types.hpp>
#include <wxlens/products/product_descriptor.hpp>

#include <memory>

#include <QMapLibre/Map>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>

namespace wxlens
{
namespace panes
{

/**
 * One pane in the grid: a View (docs/ROADMAP.md §0.1 principle #4, §4.6). Owns what it displays
 * (a products::ProductDescriptor - never a `radarSite` field, per §4.6's audit note) and its own
 * camera state, both fully independent of every other pane.
 *
 * Holds its own per-channel sync group membership (§4.1). A pane knows only which groups it
 * belongs to, never which other panes exist - fanning a change out to grouped panes is
 * PaneGridModel's job, so panes stay independent of one another by construction.
 */
class PaneController : public QObject
{
   Q_OBJECT

   Q_PROPERTY(int paneId READ paneId CONSTANT)

   Q_PROPERTY(QString productKind READ productKind NOTIFY productChanged)
   Q_PROPERTY(QString sourceKey READ sourceKey WRITE setSourceKey NOTIFY productChanged)
   Q_PROPERTY(QString productName READ productName WRITE setProductName NOTIFY productChanged)
   Q_PROPERTY(QStringList availableProducts READ availableProducts CONSTANT)
   Q_PROPERTY(QVariantList productCatalog READ productCatalog NOTIFY productCatalogChanged)
   Q_PROPERTY(bool productCatalogLoading READ productCatalogLoading NOTIFY productCatalogChanged)
   Q_PROPERTY(QString productCatalogError READ productCatalogError NOTIFY productCatalogChanged)
   Q_PROPERTY(QString productIdentity READ productIdentity NOTIFY productChanged)
   Q_PROPERTY(bool level3Product READ level3Product NOTIFY productChanged)
   Q_PROPERTY(QString paletteName READ paletteName WRITE setPaletteName NOTIFY paletteChanged)
   Q_PROPERTY(QVariantList elevationCuts READ elevationCuts NOTIFY sourceDataChanged)
   Q_PROPERTY(double selectedElevation READ selectedElevation WRITE setSelectedElevation NOTIFY productChanged)

   // The pane's data source's own coordinates (a radar site's location today) - used to centre a
   // freshly created pane on something meaningful. Named for the role, not the domain, so a
   // non-radar source can answer it too.
   Q_PROPERTY(double homeLatitude READ homeLatitude NOTIFY productChanged)
   Q_PROPERTY(double homeLongitude READ homeLongitude NOTIFY productChanged)

   // Location is read-only as a property and written through setCenter(): latitude and longitude
   // are one channel, and setting them one at a time would fan a half-updated coordinate out to
   // grouped panes.
   Q_PROPERTY(double centerLatitude READ centerLatitude NOTIFY cameraChanged)
   Q_PROPERTY(double centerLongitude READ centerLongitude NOTIFY cameraChanged)
   Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY cameraChanged)
   Q_PROPERTY(double bearing READ bearing WRITE setBearing NOTIFY cameraChanged)
   Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY cameraChanged)
   Q_PROPERTY(bool liveMode READ liveMode NOTIFY timeChanged)
   Q_PROPERTY(QString selectedTimeText READ selectedTimeText NOTIFY timeChanged)
   Q_PROPERTY(bool timeLoading READ timeLoading NOTIFY timeChanged)
   Q_PROPERTY(QString timeError READ timeError NOTIFY timeChanged)
   Q_PROPERTY(QString selectedStorm READ selectedStorm NOTIFY selectedStormChanged)
   Q_PROPERTY(QVariantList productOverlays READ productOverlays NOTIFY productDetailsChanged)
   Q_PROPERTY(QString productDetailsText READ productDetailsText NOTIFY productDetailsChanged)

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
   [[nodiscard]] QStringList availableProducts() const;
   [[nodiscard]] QVariantList productCatalog() const;
   [[nodiscard]] bool productCatalogLoading() const;
   [[nodiscard]] QString productCatalogError() const;
   [[nodiscard]] QString productIdentity() const;
   [[nodiscard]] bool level3Product() const;
   [[nodiscard]] QString paletteName() const;
   void setPaletteName(const QString& paletteName);
   Q_INVOKABLE void refreshProductCatalog();
   Q_INVOKABLE bool selectProduct(const QString& identityKind,
                                  const QString& identity,
                                  const QString& name);
   [[nodiscard]] QVariantList elevationCuts() const;
   [[nodiscard]] double selectedElevation() const;
   void setProductName(const QString& productName);
   void setSelectedElevation(double elevation);
   void                  setSourceKey(const QString& sourceKey);

   [[nodiscard]] double homeLatitude() const;
   [[nodiscard]] double homeLongitude() const;

   [[nodiscard]] double centerLatitude() const;
   [[nodiscard]] double centerLongitude() const;
   [[nodiscard]] double zoom() const;
   [[nodiscard]] double bearing() const;
   [[nodiscard]] double pitch() const;
   [[nodiscard]] bool liveMode() const;
   [[nodiscard]] QString selectedTimeText() const;
   [[nodiscard]] bool timeLoading() const;
   [[nodiscard]] QString timeError() const;
   [[nodiscard]] QString selectedStorm() const;
   [[nodiscard]] QVariantList productOverlays() const;
   [[nodiscard]] QString productDetailsText() const;
   Q_INVOKABLE void selectStorm(const QString& stormId);

   Q_INVOKABLE void selectLive();
   Q_INVOKABLE bool selectArchiveTime(const QString& isoUtc);

   /**
    * Sets the Location channel atomically. Called from QML as the user pans, so it is treated as
    * UserInput and is the one path that fans out to grouped panes.
    */
   Q_INVOKABLE void setCenter(double latitude, double longitude);

   void setZoom(double value);
   void setBearing(double value);
   void setPitch(double value);

   /** This pane's group on `channel`, or kNoSyncGroup when independent on it. */
   Q_INVOKABLE int syncGroup(wxlens::panes::SyncChannel channel) const;

   /**
    * Joins (or, with kNoSyncGroup, leaves) a group on one channel. Joining does not itself copy
    * any value across - panes converge on the next change to that channel. Use
    * PaneGridModel::copyChannel for the distinct "match this pane to that one now" action
    * (§4.1's persistent-link vs. one-shot-apply distinction).
    */
   Q_INVOKABLE void setSyncGroup(wxlens::panes::SyncChannel channel, int groupId);

   /** Current value of `channel`, or an invalid QVariant for channels with no state yet. */
   [[nodiscard]] QVariant channelValue(wxlens::panes::SyncChannel channel) const;

   /**
    * Applies an incoming value from the sync coordinator. Origin is recorded so the resulting
    * change does not fan out again (§4.2).
    */
   void applyChannelValue(wxlens::panes::SyncChannel channel,
                          const QVariant&            value,
                          wxlens::panes::ChangeOrigin origin);

   /**
    * Registers this pane's Visualization Layer(s) on its map. Called from QML once the map's
    * style has loaded - custom layers added before that are silently dropped (see
    * docs/adr/0004-maplibre-qml-integration.md's slice 3 findings).
    */
   Q_INVOKABLE void attachLayers(QMapLibre::Map* map);

   /** Applies the shared Map-details visibility policy to this pane's active style. */
   Q_INVOKABLE void applyMapDetails(const QVariantMap& visibility);

   /**
    * Geographic -> screen projection for this pane's map, so the QML object overlay can position
    * geo-anchored objects (docs/ROADMAP.md §4.3 requires objects be stored in coordinates, never
    * screen space - this is where that gets turned into pixels, per pane, at draw time).
    *
    * Returns (-1, -1) before the map exists. Wrapping QMapLibre::Map's own projection rather than
    * exposing the Map to QML keeps the ownership hazard documented in attachLayers contained to
    * one place.
    */
   Q_INVOKABLE QPointF pixelForCoordinate(double latitude, double longitude) const;

   /**
    * Whether this pane has a map yet, i.e. whether the projection above returns anything real.
    * Callers that project several coordinates and compare the results need this: without a map
    * every coordinate collapses to the same (-1, -1), which silently turns "these are all at the
    * same place" into a true statement.
    */
   [[nodiscard]] Q_INVOKABLE bool hasMap() const;

   /** Screen -> geographic, for placing objects where the user clicked. */
   Q_INVOKABLE QVariantList coordinateForPixel(double x, double y) const;

   /** Ground distance in metres between two coordinates, using real geodesic math. */
   Q_INVOKABLE double distanceMeters(double latitude1,
                                     double longitude1,
                                     double latitude2,
                                     double longitude2) const;

   /**
    * A coordinate at `bearingDegrees` and `distanceMeters` from a point. Used to build range-ring
    * geometry as a true geodesic circle rather than a screen-space ellipse.
    */
   Q_INVOKABLE QVariantList coordinateAtOffset(double latitude,
                                               double longitude,
                                               double bearingDegrees,
                                               double distanceMeters) const;

   /**
    * Asks this pane's data source what it knows about one coordinate (docs/ROADMAP.md §4.4's
    * point-info probe, §4.7's radar-geometry interrogation).
    *
    * Deliberately *not* a `radarGeometryAt()`. §4.6's audit note forbids radar-specific fields on
    * PaneController, and §4.4 asks for a probe designed so additional data sources plug in later
    * without a rework - a satellite or model pane answering the same question with its own fields
    * is the whole point. So this dispatches on product kind exactly as attachLayers does, and the
    * returned map's `kind` says which shape the caller is looking at.
    *
    * Always present: `kind`, `available`, `latitude`, `longitude`. When `available` is false,
    * `unavailableReason` says why in words a user can read, and nothing else should be displayed.
    *
    * For `kind == "radar"` the map carries range/azimuth, the selected elevation angle, and beam
    * -centre altitude MSL and above-radar-level, each with raw numbers *and* a preformatted
    * display string so the UI stays presentation-only. Beam-centre height above *ground* is
    * absent by construction, and `terrainAvailable` is false, until a real terrain source exists
    * (§4.7): a missing field cannot be rendered as though it were authoritative, whereas a
    * plausible placeholder can.
    *
    * Cheap and side-effect free - safe to call on every cursor move, which is what §4.7's "live,
    * not static" readout does.
    */
   Q_INVOKABLE QVariantMap probeSourceAt(double latitude, double longitude) const;

signals:
   void productChanged();
   void productCatalogChanged();
   void paletteChanged();
   void cameraChanged();
   void syncGroupsChanged();

   /**
    * The pane's data source published new data. Distinct from productChanged, which means "this
    * pane is now bound to something else" - this means "the thing it is bound to has new content".
    *
    * probeSourceAt's answer depends on that content (the selected elevation angle comes from the
    * loaded sweep), and a method call is invisible to a QML binding, so the readout needs this to
    * know it has gone stale.
    */
   void sourceDataChanged();
   void timeChanged();
   void selectedStormChanged();
   void productDetailsChanged();

   /**
    * Emitted only when a camera channel was changed by something other than this pane's own user
    * input - i.e. by the sync coordinator. The view must follow the map for its own gestures, not
    * the other way round: re-applying the controller's camera during a local gesture fights it.
    * Zoom-about-cursor moves the centre as part of zooming, so a handler that snaps the centre
    * back mid-gesture turns a zoom into a slide. (Observed as exactly that, and this signal
    * exists to keep the two directions separable.)
    */
   void cameraSynced();

   /**
    * Emitted whenever a channel's value changes, tagged with where the change came from.
    * PaneGridModel listens and fans out only UserInput changes.
    */
   void channelChanged(wxlens::panes::SyncChannel channel, wxlens::panes::ChangeOrigin origin);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace panes
} // namespace wxlens
