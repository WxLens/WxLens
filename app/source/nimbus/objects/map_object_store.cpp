#include <nimbus/objects/map_object_store.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/panes/pane_controller.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nimbus
{
namespace objects
{

static const std::string logPrefix_ = "objects.map_object_store";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

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
   if (pane == nullptr)
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
      entry[QStringLiteral("label")]        = object.label;
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

} // namespace objects
} // namespace nimbus
