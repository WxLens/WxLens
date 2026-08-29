#pragma once

#include <memory>

#include <QObject>

namespace wxlens
{
namespace panes
{
class PaneController;
}

namespace objects
{

/**
 * Drives object placement from the pane chrome (docs/ROADMAP.md §4.3/§4.5). Holds which tool is
 * active, what scope newly created objects get, and the range-ring radius - so QML stays
 * presentation-only and the placement rules live in C++.
 *
 * This is the seam where the three-tier lifecycle starts: a tool being active is tier 1
 * (Temporary) state and lives here, not in MapObjectStore. Only committing a placement creates a
 * stored, Pinned object.
 */
class ObjectToolController : public QObject
{
   Q_OBJECT

   Q_PROPERTY(int activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
   Q_PROPERTY(int scopeKind READ scopeKind WRITE setScopeKind NOTIFY scopeKindChanged)
   Q_PROPERTY(double ringRadiusMeters READ ringRadiusMeters WRITE setRingRadiusMeters NOTIFY
                 ringRadiusMetersChanged)
   Q_PROPERTY(bool drawingActive READ drawingActive NOTIFY drawingChanged)
   Q_PROPERTY(QVariantList drawingLatitudes READ drawingLatitudes NOTIFY drawingChanged)
   Q_PROPERTY(QVariantList drawingLongitudes READ drawingLongitudes NOTIFY drawingChanged)

public:
   /// Deliberately not MapObjectType: "no tool" is a first-class state, and the tool set is a UI
   /// concept that will not stay one-to-one with object types (slice 7 adds measurement modes
   /// that all produce the same object type).
   enum class Tool
   {
      None = 0,
      Marker,
      RangeRing,
      Drawing
   };
   Q_ENUM(Tool)

   explicit ObjectToolController(QObject* parent = nullptr);
   ~ObjectToolController() override;

   ObjectToolController(const ObjectToolController&)            = delete;
   ObjectToolController& operator=(const ObjectToolController&) = delete;
   ObjectToolController(ObjectToolController&&)                 = delete;
   ObjectToolController& operator=(ObjectToolController&&)      = delete;

   [[nodiscard]] int    activeTool() const;
   [[nodiscard]] int    scopeKind() const;
   [[nodiscard]] double ringRadiusMeters() const;
   [[nodiscard]] bool drawingActive() const;
   [[nodiscard]] QVariantList drawingLatitudes() const;
   [[nodiscard]] QVariantList drawingLongitudes() const;

   void setActiveTool(int tool);
   void setScopeKind(int scopeKind);
   void setRingRadiusMeters(double radiusMeters);

   /**
    * Commits a placement at the given coordinate, creating a Pinned object in the store. Returns
    * the new object's id, or -1 when no tool is active or the placement was rejected.
    */
   Q_INVOKABLE int placeAt(double                         latitude,
                           double                         longitude,
                           wxlens::panes::PaneController* pane);
   Q_INVOKABLE void beginDrawing(double latitude, double longitude);
   Q_INVOKABLE void appendDrawingPoint(double latitude, double longitude);
   Q_INVOKABLE int commitDrawing(wxlens::panes::PaneController* pane);
   Q_INVOKABLE void cancelDrawing();

signals:
   void activeToolChanged();
   void scopeKindChanged();
   void ringRadiusMetersChanged();
   void objectPlaced(int objectId);
   void drawingChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace objects
} // namespace wxlens
