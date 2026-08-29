#include <wxlens/objects/snap_target_registry.hpp>

#include <wxlens/data/radar_site_database.hpp>
#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/panes/pane_controller.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace wxlens
{
namespace objects
{

SnapTargetRegistry::SnapTargetRegistry(QObject* parent) : QObject(parent) {}

QVariantMap SnapTargetRegistry::resolve(panes::PaneController* pane,
                                        double                 x,
                                        double                 y,
                                        double                 tolerancePixels,
                                        bool                   suppressed) const
{
   QVariantMap result {{QStringLiteral("snapped"), false}};
   if (pane == nullptr || !pane->hasMap() || suppressed || tolerancePixels <= 0.0)
   {
      return result;
   }

   double bestSquared = tolerancePixels * tolerancePixels;
   auto consider = [&](double latitude,
                       double longitude,
                       const QString& kind,
                       const QString& label)
   {
      const QPointF pixel = pane->pixelForCoordinate(latitude, longitude);
      const double dx = pixel.x() - x;
      const double dy = pixel.y() - y;
      const double distanceSquared = dx * dx + dy * dy;
      if (distanceSquared > bestSquared)
      {
         return;
      }
      bestSquared = distanceSquared;
      result = {{QStringLiteral("snapped"), true},
                {QStringLiteral("latitude"), latitude},
                {QStringLiteral("longitude"), longitude},
                {QStringLiteral("pixelX"), pixel.x()},
                {QStringLiteral("pixelY"), pixel.y()},
                {QStringLiteral("kind"), kind},
                {QStringLiteral("label"), label}};
   };

   // Sites are registered as a population, not as a special case for the pane's active source.
   // Projection plus the pixel tolerance naturally limits this to sites visible near the pointer.
   for (const data::RadarSiteInfo& site : data::RadarSites())
   {
      consider(site.latitude,
               site.longitude,
               QStringLiteral("radar"),
               QString::fromStdString(site.id));
   }

   for (const MapObject& object : MapObjectStore::Instance().Objects())
   {
      if (!MapObjectStore::IsVisibleInPane(object, pane))
      {
         continue;
      }
      const qsizetype count = std::min(object.latitudes.size(), object.longitudes.size());
      for (qsizetype i = 0; i < count; ++i)
      {
         consider(object.latitudes[i],
                  object.longitudes[i],
                  object.savedPlaceGroupId.isEmpty() ? QStringLiteral("object")
                                                     : QStringLiteral("saved-place"),
                  object.label.isEmpty() ? QStringLiteral("Map object") : object.label);
      }
      if (count > 1)
      {
         double latitude = 0.0;
         double longitude = 0.0;
         for (qsizetype i = 0; i < count; ++i)
         {
            latitude += object.latitudes[i];
            longitude += object.longitudes[i];
         }
         consider(latitude / static_cast<double>(count),
                  longitude / static_cast<double>(count),
                  QStringLiteral("object-center"),
                  object.label.isEmpty() ? QStringLiteral("Map object centre") : object.label);
      }
   }

   return result;
}

} // namespace objects
} // namespace wxlens
