#pragma once

#include <nimbus/objects/map_object.hpp>

#include <memory>

#include <QAbstractListModel>
#include <QVariantMap>

namespace nimbus
{
namespace panes
{
class PaneController;
}

namespace objects
{

/**
 * The single store for every map object (docs/ROADMAP.md §4.3) - one object family and one store,
 * replacing the legacy app's parallel marker and annotation systems.
 *
 * Scope resolution lives here rather than in the view: whether an object belongs in a given pane
 * is a property of the object and that pane's state, not of how it happens to be drawn.
 */
class MapObjectStore : public QAbstractListModel
{
   Q_OBJECT

   Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
   enum Roles
   {
      IdRole = Qt::UserRole + 1,
      TypeRole,
      LabelRole,
      ColorRole,
      RadiusRole,
      LatitudesRole,
      LongitudesRole,
      ScopeKindRole,
      OriginPaneRole,
      LifecycleRole
   };

   explicit MapObjectStore(QObject* parent = nullptr);
   ~MapObjectStore() override;

   MapObjectStore(const MapObjectStore&)            = delete;
   MapObjectStore& operator=(const MapObjectStore&) = delete;
   MapObjectStore(MapObjectStore&&)                 = delete;
   MapObjectStore& operator=(MapObjectStore&&)      = delete;

   /** Process-wide store, matching the roadmap's "one store" requirement. */
   static MapObjectStore& Instance();

   [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
   [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

   /**
    * Bumped on every change. QML bindings that call the scope-resolution methods below reference
    * it so they know when to re-evaluate, since a method result cannot be tracked for staleness.
    */
   [[nodiscard]] int revision() const;

   /**
    * Adds an object and returns its id, or -1 if it was rejected.
    *
    * Temporary objects are deliberately rejected: tier 1 of the lifecycle is tool-local UI state
    * and must never reach the store (§4.3), which is what keeps the map from filling with clutter
    * every time someone probes a point.
    */
   int Add(const MapObject& object);

   bool Remove(int id);
   void Clear();

   [[nodiscard]] const MapObject* Find(int id) const;
   [[nodiscard]] std::vector<MapObject> Objects() const;

   /**
    * Whether `object` should be drawn in `pane`, per its scope (§4.3). This is the resolution
    * step that makes an object visible across "linked" panes without being global.
    */
   [[nodiscard]] static bool IsVisibleInPane(const MapObject&            object,
                                             const panes::PaneController* pane);

   /** QML-facing: the objects visible in one pane, as maps ready to drive a Repeater. */
   Q_INVOKABLE QVariantList objectsForPane(nimbus::panes::PaneController* pane) const;

   /**
    * QML-facing convenience for the placement tools. Returns the new object's id, or -1.
    *
    * Takes the originating pane rather than just its id so the object's sync group can be
    * captured at creation - see MapObjectScope::originGroupId.
    */
   Q_INVOKABLE int addMarker(double                         latitude,
                             double                         longitude,
                             const QString&                 label,
                             nimbus::panes::PaneController* originPane,
                             int                            scopeKind);

   Q_INVOKABLE int addRangeRing(double                         latitude,
                                double                         longitude,
                                double                         radiusMeters,
                                const QString&                 label,
                                nimbus::panes::PaneController* originPane,
                                int                            scopeKind);

   Q_INVOKABLE bool removeObject(int id);
   Q_INVOKABLE void clearObjects();

   /** Changes an existing object's scope, so "show this everywhere" is available after creation. */
   Q_INVOKABLE bool setObjectScope(int id, int scopeKind);

signals:
   void revisionChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace objects
} // namespace nimbus
