#pragma once

#include <wxlens/data/radar_site_database.hpp>

#include <functional>
#include <optional>

#include <QObject>
#include <QPointF>
#include <QVariantList>

namespace wxlens
{
namespace panes { class PaneController; }
namespace data
{

struct ProjectedRadarSite
{
   RadarSiteInfo site;
   QPointF       pixel;
};

class RadarSiteMarkerSource : public QObject
{
   Q_OBJECT
public:
   using Projector = std::function<QPointF(double, double)>;

   explicit RadarSiteMarkerSource(QObject* parent = nullptr);

   [[nodiscard]] static std::vector<ProjectedRadarSite>
   Cull(const std::vector<RadarSiteInfo>& sites, const Projector& projector,
        double width, double height, double margin, bool includeTdwr);
   [[nodiscard]] static std::optional<ProjectedRadarSite>
   Nearest(const std::vector<ProjectedRadarSite>& sites, const QPointF& point, double tolerance);

   Q_INVOKABLE QVariantList visibleSites(QObject* pane, double width, double height,
                                         double margin, bool includeTdwr) const;
   Q_INVOKABLE QVariantMap nearestSite(QObject* pane, double x, double y, double width,
                                       double height, double margin, bool includeTdwr,
                                       double tolerance) const;
};

} // namespace data
} // namespace wxlens
