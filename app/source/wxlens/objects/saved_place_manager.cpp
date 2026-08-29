#include <wxlens/objects/saved_place_manager.hpp>

#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

namespace wxlens
{
namespace objects
{
namespace
{
struct Group
{
   QString id;
   QString name;
   QString color;
   bool visible {true};
};

QString LocalPath(const QString& value)
{
   const QUrl url {value};
   return url.isLocalFile() ? url.toLocalFile() : value;
}

bool ValidCoordinate(double latitude, double longitude)
{
   return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90.0 &&
          latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}
} // namespace

class SavedPlaceManager::Impl
{
public:
   Impl(SavedPlaceManager* self, MapObjectStore& store, settings::SettingsStore& settings) :
       self_ {self}, store_ {store}, settings_ {settings}
   {
   }

   bool Load(const QString& path, bool replace);
   bool Save(const QString& path) const;
   void Publish();
   const Group* FindGroup(const QString& id) const;

   SavedPlaceManager* self_;
   MapObjectStore& store_;
   settings::SettingsStore& settings_;
   std::vector<Group> groups_;
};

const Group* SavedPlaceManager::Impl::FindGroup(const QString& id) const
{
   const auto it = std::find_if(groups_.begin(), groups_.end(), [&id](const Group& group)
                                { return group.id == id; });
   return it == groups_.end() ? nullptr : &*it;
}

bool SavedPlaceManager::Impl::Save(const QString& path) const
{
   QJsonArray groups;
   for (const Group& group : groups_)
   {
      groups.append(QJsonObject {{"id", group.id}, {"name", group.name}, {"color", group.color},
                                 {"visible", group.visible}});
   }
   QJsonArray places;
   for (const MapObject& object : store_.Objects())
   {
      if (object.lifecycle != MapObjectLifecycle::Saved || object.savedPlaceGroupId.isEmpty() ||
          object.latitudes.isEmpty() || object.longitudes.isEmpty())
      {
         continue;
      }
      places.append(QJsonObject {{"name", object.label},
                                 {"latitude", object.latitudes.front()},
                                 {"longitude", object.longitudes.front()},
                                 {"group", object.savedPlaceGroupId},
                                 {"colorOverride", object.colorOverride}});
   }
   QSaveFile file {path};
   if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly))
   {
      return false;
   }
   const QByteArray data = QJsonDocument {QJsonObject {{"version", 1}, {"groups", groups},
                                                        {"places", places}}}.toJson();
   return file.write(data) == data.size() && file.commit();
}

bool SavedPlaceManager::Impl::Load(const QString& path, bool replace)
{
   QFile file {path};
   if (!file.open(QIODevice::ReadOnly)) return false;
   QJsonParseError error;
   const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
   if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
   const QJsonObject root = document.object();
   if (root.value("version").toInt() != 1 || !root.value("groups").isArray() ||
       !root.value("places").isArray()) return false;

   std::vector<Group> incomingGroups;
   std::vector<MapObject> incomingPlaces;
   for (const QJsonValue value : root.value("groups").toArray())
   {
      const QJsonObject item = value.toObject();
      Group group {item.value("id").toString(), item.value("name").toString(),
                   item.value("color").toString(), item.value("visible").toBool(true)};
      if (group.id.isEmpty() || group.name.trimmed().isEmpty() || !QColor::isValidColor(group.color))
         return false;
      incomingGroups.push_back(group);
   }
   for (const QJsonValue value : root.value("places").toArray())
   {
      const QJsonObject item = value.toObject();
      const double latitude = item.value("latitude").toDouble(999.0);
      const double longitude = item.value("longitude").toDouble(999.0);
      const QString group = item.value("group").toString();
      const QString name = item.value("name").toString().trimmed();
      const QString overrideColor = item.value("colorOverride").toString();
      const bool groupExists = std::any_of(incomingGroups.begin(), incomingGroups.end(),
                                           [&group](const Group& g) { return g.id == group; });
      if (!ValidCoordinate(latitude, longitude) || name.isEmpty() || !groupExists ||
          (!overrideColor.isEmpty() && !QColor::isValidColor(overrideColor))) return false;
      MapObject object;
      object.type = MapObjectType::Marker;
      object.latitudes = {latitude}; object.longitudes = {longitude}; object.label = name;
      object.savedPlaceGroupId = group; object.colorOverride = overrideColor;
      object.lifecycle = MapObjectLifecycle::Saved; object.scope.kind = MapObjectScopeKind::AllPanes;
      incomingPlaces.push_back(object);
   }
   if (replace)
   {
      for (const MapObject& object : store_.Objects())
         if (object.lifecycle == MapObjectLifecycle::Saved && !object.savedPlaceGroupId.isEmpty())
            store_.Remove(object.id);
      groups_.clear();
   }
   for (Group& group : incomingGroups)
   {
      if (FindGroup(group.id) != nullptr) group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      groups_.push_back(group);
   }
   for (MapObject& object : incomingPlaces) store_.Add(object);
   return true;
}

void SavedPlaceManager::Impl::Publish()
{
   for (MapObject object : store_.Objects())
   {
      if (object.lifecycle == MapObjectLifecycle::Saved && !object.savedPlaceGroupId.isEmpty())
      {
         const Group* group = FindGroup(object.savedPlaceGroupId);
         object.visible = group != nullptr && group->visible;
         object.color = object.colorOverride.isEmpty() && group != nullptr ? group->color
                                                                          : object.colorOverride;
         store_.Update(object.id, object);
      }
   }
   Save(QDir(settings_.ConfigDirectory()).filePath(QStringLiteral("saved-places.json")));
   Q_EMIT self_->changed();
}

SavedPlaceManager::SavedPlaceManager(MapObjectStore& store, settings::SettingsStore& settings,
                                     QObject* parent) : QObject(parent),
   p {std::make_unique<Impl>(this, store, settings)}
{
   const QString path =
      QDir(settings.ConfigDirectory()).filePath(QStringLiteral("saved-places.json"));
   if (!QFileInfo::exists(path) || p->Load(path, true))
   {
      p->Publish();
   }
}
SavedPlaceManager::~SavedPlaceManager() = default;

QVariantList SavedPlaceManager::groups() const
{
   QVariantList result;
   for (const Group& group : p->groups_)
      result.append(QVariantMap {{"id", group.id}, {"name", group.name}, {"color", group.color},
                                 {"visible", group.visible}});
   return result;
}

QVariantList SavedPlaceManager::places() const
{
   QVariantList result;
   for (const MapObject& object : p->store_.Objects())
      if (object.lifecycle == MapObjectLifecycle::Saved && !object.savedPlaceGroupId.isEmpty())
         result.append(QVariantMap {{"id", object.id}, {"name", object.label},
                                    {"latitude", object.latitudes.front()},
                                    {"longitude", object.longitudes.front()},
                                    {"groupId", object.savedPlaceGroupId},
                                    {"colorOverride", object.colorOverride},
                                    {"color", EffectiveColor(object.savedPlaceGroupId, object.colorOverride)}});
   return result;
}

QString SavedPlaceManager::addGroup(const QString& name, const QString& color)
{
   if (name.trimmed().isEmpty() || !QColor::isValidColor(color)) return {};
   const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
   p->groups_.push_back({id, name.trimmed(), color, true}); p->Publish(); return id;
}
bool SavedPlaceManager::editGroup(const QString& id, const QString& name, const QString& color)
{
   if (name.trimmed().isEmpty() || !QColor::isValidColor(color)) return false;
   for (Group& group : p->groups_) if (group.id == id) { group.name=name.trimmed(); group.color=color; p->Publish(); return true; }
   return false;
}
bool SavedPlaceManager::setGroupVisible(const QString& id, bool visible)
{
   for (Group& group : p->groups_) if (group.id == id) { group.visible=visible; p->Publish(); return true; }
   return false;
}
bool SavedPlaceManager::removeGroup(const QString& id)
{
   const auto oldSize=p->groups_.size();
   std::erase_if(p->groups_, [&id](const Group& group){return group.id==id;});
   if (oldSize==p->groups_.size()) return false;
   for (const MapObject& object : p->store_.Objects()) if (object.savedPlaceGroupId==id) p->store_.Remove(object.id);
   p->Publish(); return true;
}
int SavedPlaceManager::addPlace(const QString& name, double latitude, double longitude,
                                const QString& groupId, const QString& colorOverride)
{
   if (name.trimmed().isEmpty() || !ValidCoordinate(latitude, longitude) || p->FindGroup(groupId)==nullptr ||
       (!colorOverride.isEmpty() && !QColor::isValidColor(colorOverride))) return -1;
   MapObject object; object.type=MapObjectType::Marker; object.latitudes={latitude}; object.longitudes={longitude};
   object.label=name.trimmed(); object.savedPlaceGroupId=groupId; object.colorOverride=colorOverride;
   object.lifecycle=MapObjectLifecycle::Saved; object.scope.kind=MapObjectScopeKind::AllPanes;
   const int id=p->store_.Add(object); if (id>=0) p->Publish(); return id;
}
bool SavedPlaceManager::editPlace(int id, const QString& name, double latitude, double longitude,
                                  const QString& groupId, const QString& colorOverride)
{
   const MapObject* found=p->store_.Find(id);
   if (found==nullptr || found->savedPlaceGroupId.isEmpty() || name.trimmed().isEmpty() ||
       !ValidCoordinate(latitude,longitude) || p->FindGroup(groupId)==nullptr ||
       (!colorOverride.isEmpty() && !QColor::isValidColor(colorOverride))) return false;
   MapObject replacement=*found; replacement.label=name.trimmed();
   replacement.latitudes={latitude}; replacement.longitudes={longitude}; replacement.savedPlaceGroupId=groupId;
   replacement.colorOverride=colorOverride; const bool ok=p->store_.Update(id,replacement); if(ok)p->Publish(); return ok;
}
bool SavedPlaceManager::removePlace(int id)
{
   const MapObject* object=p->store_.Find(id); if(object==nullptr || object->savedPlaceGroupId.isEmpty())return false;
   const bool ok=p->store_.Remove(id); if(ok)p->Publish(); return ok;
}
QVariantList SavedPlaceManager::search(const QString& query) const
{
   QVariantList result; const QString needle=query.trimmed();
   for(const QVariant& place:places()) if(needle.isEmpty() || place.toMap().value("name").toString().contains(needle,Qt::CaseInsensitive)) result.append(place);
   return result;
}
bool SavedPlaceManager::importFile(const QString& fileUrl)
{
   if(!p->Load(LocalPath(fileUrl),false)){Q_EMIT errorOccurred(QStringLiteral("The saved-places file is invalid."));return false;} p->Publish();return true;
}
bool SavedPlaceManager::exportFile(const QString& fileUrl)
{
   const bool ok=p->Save(LocalPath(fileUrl)); if(!ok) Q_EMIT errorOccurred(QStringLiteral("Could not export saved places.")); return ok;
}
bool SavedPlaceManager::IsSavedPlaceVisible(const QString& groupId) const
{ const Group* group=p->FindGroup(groupId); return group!=nullptr && group->visible; }
QString SavedPlaceManager::EffectiveColor(const QString& groupId,const QString& overrideColor) const
{ if(!overrideColor.isEmpty())return overrideColor; const Group* group=p->FindGroup(groupId); return group?group->color:QStringLiteral("#ffb300"); }

} // namespace objects
} // namespace wxlens
