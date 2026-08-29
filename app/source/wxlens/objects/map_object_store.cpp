#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/util/geodesic.hpp>
#include <wxlens/util/unit_format.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <QPointF>

namespace wxlens
{
namespace objects
{

static const std::string logPrefix_ = "objects.map_object_store";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

class MapObjectStore::Impl
{
public:
   std::vector<MapObject> objects_ {};
   int                    nextId_ {1};
   int                    revision_ {0};
};

MapObjectStore::MapObjectStore(QObject* parent) :
    QAbstractListModel(parent), p {std::make_unique<Impl>()}
{
}

MapObjectStore::~MapObjectStore() = default;

MapObjectStore& MapObjectStore::Instance()
{
   static MapObjectStore instance;
   return instance;
}

int MapObjectStore::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid())
   {
      return 0;
   }
   return static_cast<int>(p->objects_.size());
}

QVariant MapObjectStore::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       index.row() >= static_cast<int>(p->objects_.size()))
   {
      return {};
   }

   const MapObject& object = p->objects_[static_cast<std::size_t>(index.row())];

   switch (role)
   {
   case IdRole:
      return object.id;
   case TypeRole:
      return static_cast<int>(object.type);
   case LabelRole:
      return object.label;
   case ColorRole:
      return object.color;
   case RadiusRole:
      return object.radiusMeters;
   case LatitudesRole:
   {
      QVariantList values;
      for (const double value : object.latitudes)
      {
         values.append(value);
      }
      return values;
   }
   case LongitudesRole:
   {
      QVariantList values;
      for (const double value : object.longitudes)
      {
         values.append(value);
      }
      return values;
   }
   case ScopeKindRole:
      return static_cast<int>(object.scope.kind);
   case OriginPaneRole:
      return object.scope.originPaneId;
   case LifecycleRole:
      return static_cast<int>(object.lifecycle);
   default:
      return {};
   }
}

QHash<int, QByteArray> MapObjectStore::roleNames() const
{
   return {{IdRole, QByteArrayLiteral("objectId")},
           {TypeRole, QByteArrayLiteral("objectType")},
           {LabelRole, QByteArrayLiteral("label")},
           {ColorRole, QByteArrayLiteral("color")},
           {RadiusRole, QByteArrayLiteral("radiusMeters")},
           {LatitudesRole, QByteArrayLiteral("latitudes")},
           {LongitudesRole, QByteArrayLiteral("longitudes")},
           {ScopeKindRole, QByteArrayLiteral("scopeKind")},
           {OriginPaneRole, QByteArrayLiteral("originPaneId")},
           {LifecycleRole, QByteArrayLiteral("lifecycle")}};
}

int MapObjectStore::revision() const
{
   return p->revision_;
}

int MapObjectStore::Add(const MapObject& object)
{
   if (object.lifecycle == MapObjectLifecycle::Temporary)
   {
      // Tier 1 is tool-local state by definition (§4.3). Rejecting it here rather than trusting
      // callers is what actually enforces "probing never litters the map".
      logger_->warn("Refusing to store a Temporary object - tier 1 is tool-local UI state");
      return -1;
   }

   if (object.latitudes.isEmpty() || object.latitudes.size() != object.longitudes.size())
   {
      logger_->error("Refusing to store an object with empty or mismatched geometry");
      return -1;
   }

   MapObject stored = object;
   stored.id        = p->nextId_++;

   const int row = static_cast<int>(p->objects_.size());
   beginInsertRows(QModelIndex(), row, row);
   p->objects_.push_back(stored);
   endInsertRows();

   ++p->revision_;
   Q_EMIT revisionChanged();

   return stored.id;
}

bool MapObjectStore::Remove(int id)
{
   const auto it = std::find_if(p->objects_.begin(),
                                p->objects_.end(),
                                [id](const MapObject& o) { return o.id == id; });
   if (it == p->objects_.end())
   {
      return false;
   }

   const int row = static_cast<int>(std::distance(p->objects_.begin(), it));
   beginRemoveRows(QModelIndex(), row, row);
   p->objects_.erase(it);
   endRemoveRows();

   ++p->revision_;
   Q_EMIT revisionChanged();

   return true;
}

bool MapObjectStore::Update(int id, const MapObject& object)
{
   const auto it = std::find_if(p->objects_.begin(), p->objects_.end(),
                                [id](const MapObject& candidate) { return candidate.id == id; });
   if (it == p->objects_.end() || object.lifecycle == MapObjectLifecycle::Temporary ||
       object.latitudes.isEmpty() || object.latitudes.size() != object.longitudes.size())
   {
      return false;
   }
   const int row = static_cast<int>(std::distance(p->objects_.begin(), it));
   *it = object;
   it->id = id;
   Q_EMIT dataChanged(index(row, 0), index(row, 0));
   ++p->revision_;
   Q_EMIT revisionChanged();
   return true;
}

void MapObjectStore::Clear()
{
   if (p->objects_.empty())
   {
      return;
   }

   beginResetModel();
   p->objects_.clear();
   endResetModel();

   ++p->revision_;
   Q_EMIT revisionChanged();
}

const MapObject* MapObjectStore::Find(int id) const
{
   const auto it = std::find_if(p->objects_.cbegin(),
                                p->objects_.cend(),
                                [id](const MapObject& o) { return o.id == id; });
   return (it != p->objects_.cend()) ? &(*it) : nullptr;
}

std::vector<MapObject> MapObjectStore::Objects() const
{
   return p->objects_;
}

bool MapObjectStore::IsVisibleInPane(const MapObject&             object,
                                     const panes::PaneController* pane)
{
   if (pane == nullptr || !object.visible)
   {
      return false;
   }

   switch (object.scope.kind)
   {
   case MapObjectScopeKind::AllPanes:
      return true;

   case MapObjectScopeKind::CurrentPaneOnly:
      return pane->paneId() == object.scope.originPaneId;

   case MapObjectScopeKind::SyncGroup:
   {
      // Visible wherever the pane shares the origin pane's group on the scope's channel. This is
      // the resolution that makes "show this on my linked panes" fall out of the sync model
      // rather than needing its own bookkeeping.
      const int paneGroup = pane->syncGroup(object.scope.channel);
      if (paneGroup == panes::kNoSyncGroup)
      {
         // An ungrouped pane still sees objects it created itself; otherwise drawing into an
         // unlinked pane would make the object vanish immediately.
         return pane->paneId() == object.scope.originPaneId;
      }
      return paneGroup == object.scope.originGroupId;
   }

   case MapObjectScopeKind::SameLocation:
      return std::abs(pane->centerLatitude() - object.latitudes.front()) <=
                kSameLocationToleranceDegrees &&
             std::abs(pane->centerLongitude() - object.longitudes.front()) <=
                kSameLocationToleranceDegrees;

   default:
      return false;
   }
}

QVariantList MapObjectStore::objectsForPane(panes::PaneController* pane) const
{
   QVariantList result;

   for (const MapObject& object : p->objects_)
   {
      if (!IsVisibleInPane(object, pane))
      {
         continue;
      }

      QVariantList latitudes;
      QVariantList longitudes;
      for (int i = 0; i < object.latitudes.size(); ++i)
      {
         latitudes.append(object.latitudes[i]);
         longitudes.append(object.longitudes[i]);
      }

      QVariantMap entry;
      entry[QStringLiteral("objectId")]     = object.id;
      entry[QStringLiteral("objectType")]   = static_cast<int>(object.type);
      QString label = object.label;
      if (object.type == MapObjectType::Measurement)
      {
         double totalMeters = 0.0;
         for (int i = 1; i < object.latitudes.size(); ++i)
         {
            totalMeters += util::GeodesicInverse(object.latitudes[i - 1],
                                                  object.longitudes[i - 1],
                                                  object.latitudes[i],
                                                  object.longitudes[i])
                              .distanceMeters;
         }
         label = util::FormatGroundDistance(totalMeters);
      }
      entry[QStringLiteral("label")]        = label;
      entry[QStringLiteral("color")]        = object.color;
      entry[QStringLiteral("radiusMeters")] = object.radiusMeters;
      entry[QStringLiteral("latitudes")]    = latitudes;
      entry[QStringLiteral("longitudes")]   = longitudes;
      entry[QStringLiteral("scopeKind")]    = static_cast<int>(object.scope.kind);

      result.append(entry);
   }

   return result;
}

namespace
{
void ApplyOrigin(MapObject& object, const panes::PaneController* originPane, int scopeKind)
{
   object.scope.kind = static_cast<MapObjectScopeKind>(scopeKind);

   if (originPane == nullptr)
   {
      return;
   }

   object.scope.originPaneId  = originPane->paneId();
   object.scope.originGroupId = originPane->syncGroup(object.scope.channel);
}
} // namespace

int MapObjectStore::addMarker(double                  latitude,
                              double                  longitude,
                              const QString&          label,
                              panes::PaneController*  originPane,
                              int                     scopeKind)
{
   MapObject object;
   object.type       = MapObjectType::Marker;
   object.latitudes  = {latitude};
   object.longitudes = {longitude};
   object.label      = label;
   object.lifecycle  = MapObjectLifecycle::Pinned;
   ApplyOrigin(object, originPane, scopeKind);

   return Add(object);
}

int MapObjectStore::addRangeRing(double                 latitude,
                                 double                 longitude,
                                 double                 radiusMeters,
                                 const QString&         label,
                                 panes::PaneController* originPane,
                                 int                    scopeKind)
{
   MapObject object;
   object.type         = MapObjectType::RangeRing;
   object.latitudes    = {latitude};
   object.longitudes   = {longitude};
   object.radiusMeters = radiusMeters;
   object.label        = label;
   object.color        = QStringLiteral("#5ec8f2");
   object.lifecycle    = MapObjectLifecycle::Pinned;
   ApplyOrigin(object, originPane, scopeKind);

   return Add(object);
}

int MapObjectStore::addLine(const QVariantList&          latitudes,
                            const QVariantList&          longitudes,
                            const QString&               label,
                            panes::PaneController*       originPane,
                            int                          scopeKind)
{
   if (latitudes.size() < 2 || latitudes.size() != longitudes.size())
   {
      return -1;
   }

   MapObject object;
   object.type      = MapObjectType::Line;
   object.label     = label;
   object.color     = QStringLiteral("#ffb300");
   object.lifecycle = MapObjectLifecycle::Pinned;
   object.latitudes.reserve(latitudes.size());
   object.longitudes.reserve(longitudes.size());
   for (qsizetype i = 0; i < latitudes.size(); ++i)
   {
      object.latitudes.append(latitudes[i].toDouble());
      object.longitudes.append(longitudes[i].toDouble());
   }
   ApplyOrigin(object, originPane, scopeKind);
   return Add(object);
}

namespace
{
/// Shortest pixel distance from `point` to the segment ab. Degenerate segments fall back to the
/// point-to-point distance rather than dividing by zero.
double DistanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b)
{
   const double dx     = b.x() - a.x();
   const double dy     = b.y() - a.y();
   const double lengthSq = dx * dx + dy * dy;

   double t = 0.0;
   if (lengthSq > 0.0)
   {
      t = ((point.x() - a.x()) * dx + (point.y() - a.y()) * dy) / lengthSq;
      t = std::clamp(t, 0.0, 1.0);
   }

   const double px = a.x() + t * dx - point.x();
   const double py = a.y() + t * dy - point.y();
   return std::sqrt(px * px + py * py);
}

double DistanceBetween(const QPointF& a, const QPointF& b)
{
   const double dx = a.x() - b.x();
   const double dy = a.y() - b.y();
   return std::sqrt(dx * dx + dy * dy);
}

/// How closely `point` comes to an object as drawn in `pane`, in pixels, or infinity if the
/// object has no geometry to test. Mirrors what MapObjectsLayer draws, type for type - a hit test
/// that disagreed with the rendering would feel broken however correct its own maths was.
double PixelDistanceToObject(const MapObject&             object,
                             const panes::PaneController* pane,
                             const QPointF&               point)
{
   if (object.latitudes.isEmpty() || object.longitudes.isEmpty())
   {
      return std::numeric_limits<double>::infinity();
   }

   switch (object.type)
   {
   case MapObjectType::Marker:
   case MapObjectType::TextAnnotation:
      return DistanceBetween(
         point, pane->pixelForCoordinate(object.latitudes[0], object.longitudes[0]));

   case MapObjectType::Line:
   case MapObjectType::Polygon:
   case MapObjectType::Measurement:
   {
      // A single-vertex path is a point, not a line: without this it would report infinity and
      // become undeletable.
      if (object.latitudes.size() == 1)
      {
         return DistanceBetween(
            point, pane->pixelForCoordinate(object.latitudes[0], object.longitudes[0]));
      }

      double  best     = std::numeric_limits<double>::infinity();
      QPointF previous = pane->pixelForCoordinate(object.latitudes[0], object.longitudes[0]);
      for (int i = 1; i < object.latitudes.size(); ++i)
      {
         const QPointF current =
            pane->pixelForCoordinate(object.latitudes[i], object.longitudes[i]);
         best     = std::min(best, DistanceToSegment(point, previous, current));
         previous = current;
      }

      if (object.type == MapObjectType::Polygon)
      {
         // Closing edge: a polygon's last-to-first side is drawn, so it must be hittable too.
         const QPointF first = pane->pixelForCoordinate(object.latitudes[0], object.longitudes[0]);
         best                = std::min(best, DistanceToSegment(point, previous, first));
      }

      return best;
   }

   case MapObjectType::RangeRing:
   {
      // Sampled the same way the overlay draws it - a true geodesic circle, not a screen-space
      // one - so the ring you can grab is the ring you can see. Grabbing is on the *outline*, not
      // the interior: a large ring's interior would otherwise swallow every click inside it.
      constexpr int kSteps = 72;

      double  best  = std::numeric_limits<double>::infinity();
      QPointF previous {};
      for (int i = 0; i <= kSteps; ++i)
      {
         const double bearing = (360.0 / kSteps) * i;
         const auto [latitude, longitude] = util::GeodesicDirect(
            object.latitudes[0], object.longitudes[0], bearing, object.radiusMeters);
         const QPointF current = pane->pixelForCoordinate(latitude, longitude);

         if (i > 0)
         {
            best = std::min(best, DistanceToSegment(point, previous, current));
         }
         previous = current;
      }
      return best;
   }

   default:
      return std::numeric_limits<double>::infinity();
   }
}
} // namespace

int MapObjectStore::objectAtPixel(panes::PaneController* pane,
                                  double                 x,
                                  double                 y,
                                  double                 tolerancePixels) const
{
   // No map means every coordinate projects to the same (-1, -1), which would make a click near
   // the pane's top-left corner "hit" every object at once. Nothing is on screen to be clicked
   // in that state anyway.
   if (pane == nullptr || !pane->hasMap())
   {
      return -1;
   }

   const QPointF point {x, y};

   int    hitId = -1;
   double hitDistance = std::numeric_limits<double>::infinity();

   for (const MapObject& object : p->objects_)
   {
      if (!IsVisibleInPane(object, pane))
      {
         continue;
      }

      const double distance = PixelDistanceToObject(object, pane, point);
      if (distance > tolerancePixels)
      {
         continue;
      }

      // <= rather than <, so later objects win ties. They are drawn last, so they are the ones
      // the user sees on top and expects to act on.
      if (distance <= hitDistance)
      {
         hitDistance = distance;
         hitId       = object.id;
      }
   }

   return hitId;
}

int MapObjectStore::removeObjectsInPane(panes::PaneController* pane)
{
   if (pane == nullptr)
   {
      return 0;
   }

   // Collected first, then removed: Remove() erases from the same vector this would otherwise be
   // iterating.
   std::vector<int> ids;
   for (const MapObject& object : p->objects_)
   {
      if (IsVisibleInPane(object, pane))
      {
         ids.push_back(object.id);
      }
   }

   int removed = 0;
   for (const int id : ids)
   {
      if (Remove(id))
      {
         ++removed;
      }
   }

   return removed;
}

bool MapObjectStore::removeObject(int id)
{
   return Remove(id);
}

void MapObjectStore::clearObjects()
{
   Clear();
}

bool MapObjectStore::setObjectScope(int id, int scopeKind)
{
   const auto it = std::find_if(p->objects_.begin(),
                                p->objects_.end(),
                                [id](const MapObject& o) { return o.id == id; });
   if (it == p->objects_.end())
   {
      return false;
   }

   it->scope.kind = static_cast<MapObjectScopeKind>(scopeKind);

   const int row = static_cast<int>(std::distance(p->objects_.begin(), it));
   Q_EMIT dataChanged(index(row, 0), index(row, 0), {ScopeKindRole});

   ++p->revision_;
   Q_EMIT revisionChanged();

   return true;
}

void MapObjectStore::refreshFormatting()
{
   ++p->revision_;
   Q_EMIT revisionChanged();
}

} // namespace objects
} // namespace wxlens
