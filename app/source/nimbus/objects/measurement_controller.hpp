#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QVariantList>

namespace nimbus
{
namespace panes
{
class PaneController;
}

namespace objects
{

/**
 * The measurement framework (docs/ROADMAP.md §4.4) - a reusable interaction, not a radar-only
 * utility. All three modes share one geometry model: an ordered list of coordinates with
 * per-segment distance and bearing, plus a running total.
 *
 * **All distances and bearings use real WGS84 geodesics** (util::GeodesicInverse), never flat
 * screen-pixel math, so results stay correct at any zoom, pan or projection - that is an explicit
 * §4.4 requirement, and the reason a "close enough" pixel approximation is not acceptable here.
 *
 * An in-progress measurement is **tier 1 (Temporary)** per §4.3: it lives entirely in this
 * controller, updates live as the cursor moves, and never reaches MapObjectStore. Only Commit()
 * creates a stored, Pinned object. This is what keeps interrogating the map from leaving debris
 * behind.
 */
class MeasurementController : public QObject
{
   Q_OBJECT

   Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
   Q_PROPERTY(bool active READ active NOTIFY measurementChanged)

   /// Vertices of the in-progress measurement, including the live cursor point, as a flat list of
   /// {lat, lon, lat, lon, ...} ready for the overlay to project.
   Q_PROPERTY(QVariantList points READ points NOTIFY measurementChanged)

   /// Per-segment readouts: a list of maps with distanceMeters and bearingDegrees.
   Q_PROPERTY(QVariantList segments READ segments NOTIFY measurementChanged)

   Q_PROPERTY(double totalMeters READ totalMeters NOTIFY measurementChanged)

   /**
    * The pane the in-progress measurement belongs to, or -1 when there is none.
    *
    * The controller is a singleton shared by every pane, so without this each pane's overlay
    * drew the same in-progress geometry - a measurement started in one pane appeared in all of
    * them, then collapsed to a single pane the instant it was committed. Worse, a measurement is
    * anchored to coordinates the user picked in *one* pane; drawing it through another pane's
    * camera says something the user never measured.
    */
   Q_PROPERTY(int activePaneId READ activePaneId NOTIFY measurementChanged)

   /// One-line primary readout for the common case, per §4.4/§5.3's progressive disclosure: the
   /// detail lives in `segments`, this is what the pane shows without being asked.
   Q_PROPERTY(QString readout READ readout NOTIFY measurementChanged)

public:
   enum class Mode
   {
      None = 0,
      PointToPoint, ///< click A, click B
      RadarToPoint, ///< A is the pane's radar site; the user clicks only B
      Path          ///< multi-segment path with a running total
   };
   Q_ENUM(Mode)

   explicit MeasurementController(QObject* parent = nullptr);
   ~MeasurementController() override;

   MeasurementController(const MeasurementController&)            = delete;
   MeasurementController& operator=(const MeasurementController&) = delete;
   MeasurementController(MeasurementController&&)                 = delete;
   MeasurementController& operator=(MeasurementController&&)      = delete;

   [[nodiscard]] int          mode() const;
   [[nodiscard]] bool         active() const;
   [[nodiscard]] QVariantList points() const;
   [[nodiscard]] QVariantList segments() const;
   [[nodiscard]] double       totalMeters() const;
   [[nodiscard]] int          activePaneId() const;
   [[nodiscard]] QString      readout() const;

   void setMode(int mode);

   /**
    * Adds a committed vertex. In RadarToPoint the first vertex is supplied automatically from the
    * pane's own source location, so the user only ever clicks the far end.
    *
    * PointToPoint completes at two vertices and stops accepting more until reset; Path keeps
    * accumulating.
    */
   Q_INVOKABLE void addPoint(double latitude, double longitude, nimbus::panes::PaneController* pane);

   /**
    * Starts a fresh measurement whose origin is seeded but whose far end is not - the press half
    * of a press-drag-release measurement.
    *
    * Distinct from addPoint() because the origin is not always where the user pressed:
    * RadarToPoint anchors to the pane's own source location and ignores the press position
    * entirely, which is the whole point of that mode. Anything already in progress is discarded,
    * since a new press means a new measurement.
    */
   Q_INVOKABLE void beginDrag(double latitude, double longitude, nimbus::panes::PaneController* pane);

   /** Live cursor position - drives the rubber-band segment and readout. Never stored. */
   Q_INVOKABLE void updateCursor(double latitude, double longitude);

   /** Removes the last committed vertex. */
   Q_INVOKABLE void undoPoint();

   /** Discards the in-progress measurement entirely. */
   Q_INVOKABLE void cancel();

   /**
    * Promotes the in-progress measurement to a Pinned MapObject (tier 2), returning its id or -1.
    * The measurement is cleared afterwards so the tool is ready for the next one.
    */
   Q_INVOKABLE int commit(nimbus::panes::PaneController* pane, int scopeKind);

   /** Formats a distance for display. Kilometres and statute miles until unit settings exist. */
   Q_INVOKABLE static QString formatDistance(double meters);

   /** Re-emits formatted properties after the global unit preference changes. */
   void refreshFormatting();

private:
   /// Total over the committed vertices only, excluding the live cursor - used for the label
   /// baked into a pinned measurement.
   [[nodiscard]] QString readoutForPoints() const;

signals:
   void modeChanged();
   void measurementChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace objects
} // namespace nimbus
