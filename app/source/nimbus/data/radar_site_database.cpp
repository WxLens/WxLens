#include <nimbus/data/radar_site_database.hpp>
#include <nimbus/log/logger.hpp>

#include <map>
#include <mutex>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace nimbus
{
namespace data
{

static const std::string logPrefix_ = "data.radar_site_database";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
/// The bundled site list records altitude in feet - see RadarSiteInfo::altitudeMslMeters and the
/// FindRadarSite comment for how that was established and why it matters.
constexpr double kMetersPerFoot = 0.3048;
} // namespace

static const std::map<std::string, RadarSiteInfo>& Sites()
{
   static const std::map<std::string, RadarSiteInfo> sites = []()
   {
      std::map<std::string, RadarSiteInfo> result;

      QFile file(":/qt/qml/Nimbus/App/res/config/radar_sites.json");
      if (!file.open(QIODevice::ReadOnly))
      {
         logger_->error("Failed to open bundled radar_sites.json");
         return result;
      }

      const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
      if (!doc.isArray())
      {
         logger_->error("radar_sites.json is not a JSON array");
         return result;
      }

      for (const QJsonValue& value : doc.array())
      {
         const QJsonObject obj = value.toObject();

         RadarSiteInfo site;
         site.id                = obj.value("id").toString().toStdString();
         site.place             = obj.value("place").toString().toStdString();
         site.state             = obj.value("state").toString().toStdString();
         site.country           = obj.value("country").toString().toStdString();
         site.latitude          = obj.value("lat").toDouble();
         site.longitude         = obj.value("lon").toDouble();
         site.altitudeMslMeters = obj.value("elevation").toDouble() * kMetersPerFoot;
         site.timeZoneId        = obj.value("tz").toString().toStdString();

         if (!site.id.empty())
         {
            result.emplace(site.id, std::move(site));
         }
      }

      logger_->info("Loaded {} radar site records", result.size());
      return result;
   }();

   return sites;
}

std::optional<RadarSiteInfo> FindRadarSite(const std::string& siteId)
{
   const auto& sites = Sites();
   const auto  it     = sites.find(siteId);
   if (it == sites.end())
   {
      return std::nullopt;
   }
   return it->second;
}

} // namespace data
} // namespace nimbus
