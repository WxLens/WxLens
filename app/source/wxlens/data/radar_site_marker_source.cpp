#include <wxlens/data/radar_site_marker_source.hpp>
#include <wxlens/panes/pane_controller.hpp>

#include <algorithm>
#include <cmath>

#include <QVariantMap>

namespace wxlens
{
namespace data
{
namespace
{
QVariantMap ToMap(const ProjectedRadarSite& projected)
{
   const auto& site = projected.site;
   return {{QStringLiteral("id"), QString::fromStdString(site.id)},
           {QStringLiteral("name"), QString::fromStdString(site.place)},
           {QStringLiteral("type"), QString::fromLatin1(RadarSiteTypeName(site.type))},
           {QStringLiteral("latitude"), site.latitude},
           {QStringLiteral("longitude"), site.longitude},
           {QStringLiteral("x"), projected.pixel.x()},
           {QStringLiteral("y"), projected.pixel.y()}};
}
} // namespace

RadarSiteMarkerSource::RadarSiteMarkerSource(QObject* parent) : QObject(parent) {}

std::vector<ProjectedRadarSite> RadarSiteMarkerSource::Cull(
   const std::vector<RadarSiteInfo>& sites, const Projector& projector, double width,
   double height, double margin, bool includeTdwr)
{
   std::vector<ProjectedRadarSite> result;
   for (const auto& site : sites)
   {
      if (!includeTdwr && site.type == RadarSiteType::Tdwr) continue;
      const QPointF pixel = projector(site.latitude, site.longitude);
      if (pixel.x() >= -margin && pixel.x() <= width + margin &&
          pixel.y() >= -margin && pixel.y() <= height + margin)
         result.push_back({site, pixel});
   }
   return result;
}

std::optional<ProjectedRadarSite> RadarSiteMarkerSource::Nearest(
   const std::vector<ProjectedRadarSite>& sites, const QPointF& point, double tolerance)
{
   std::optional<ProjectedRadarSite> best;
   double bestSquared = tolerance * tolerance;
   for (const auto& site : sites)
   {
      const double dx = site.pixel.x() - point.x();
      const double dy = site.pixel.y() - point.y();
      const double squared = dx * dx + dy * dy;
      if (squared > bestSquared) continue;
      // Ties resolve to the lexicographically smallest id so a hit never depends on database
      // order. bestSquared starts at the tolerance, so a click outside it selects nothing and
      // falls through to the map (ROADMAP.md 4.10).
      if (!best || squared < bestSquared || site.site.id < best->site.id)
      {
         bestSquared = squared;
         best = site;
      }
   }
   return best;
}

QVariantList RadarSiteMarkerSource::visibleSites(QObject* object, double width, double height,
                                                  double margin, bool includeTdwr) const
{
   QVariantList result;
   auto* pane = qobject_cast<panes::PaneController*>(object);
   if (!pane || !pane->hasMap()) return result;
   const auto sites = Cull(RadarSites(), [pane](double lat, double lon) {
      return pane->pixelForCoordinate(lat, lon);
   }, width, height, margin, includeTdwr);
   for (const auto& site : sites) result.append(ToMap(site));
   return result;
}

QVariantMap RadarSiteMarkerSource::nearestSite(QObject* object, double x, double y, double width,
                                                double height, double margin, bool includeTdwr,
                                                double tolerance) const
{
   auto* pane = qobject_cast<panes::PaneController*>(object);
   if (!pane || !pane->hasMap() || tolerance < 0.0) return {};
   const auto sites = Cull(RadarSites(), [pane](double lat, double lon) {
      return pane->pixelForCoordinate(lat, lon);
   }, width, height, margin, includeTdwr);
   const auto nearest = Nearest(sites, QPointF{x, y}, tolerance);
   return nearest ? ToMap(*nearest) : QVariantMap {};
}

} // namespace data
} // namespace wxlens
