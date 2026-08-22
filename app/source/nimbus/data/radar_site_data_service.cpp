#include <nimbus/data/radar_site_data_service.hpp>
#include <nimbus/log/logger.hpp>

#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/threads.hpp>

#include <map>
#include <shared_mutex>

namespace nimbus
{
namespace data
{

static const std::string logPrefix_ = "data.radar_site_data_service";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

class RadarSiteDataService::Impl
{
public:
   explicit Impl(const std::string& radarSite) :
       radarSite_ {radarSite},
       level2Provider_ {scwx::provider::NexradDataProviderFactory::
                            CreateLevel2DataProvider(radarSite)}
   {
   }

   std::string                                         radarSite_;
   std::shared_ptr<scwx::provider::NexradDataProvider> level2Provider_;
};

RadarSiteDataService::RadarSiteDataService(const std::string& radarSite) :
    p {std::make_unique<Impl>(radarSite)}
{
}

RadarSiteDataService::~RadarSiteDataService() = default;

const std::string& RadarSiteDataService::radar_site() const
{
   return p->radarSite_;
}

std::shared_ptr<RadarSiteDataService>
RadarSiteDataService::Instance(const std::string& radarSite)
{
   static std::shared_mutex                                            instanceMutex;
   static std::map<std::string, std::shared_ptr<RadarSiteDataService>> instances;

   std::shared_lock readLock {instanceMutex};
   auto             it = instances.find(radarSite);
   if (it != instances.end())
   {
      return it->second;
   }
   readLock.unlock();

   std::unique_lock writeLock {instanceMutex};
   auto             [insertedIt, inserted] = instances.try_emplace(
      radarSite, std::make_shared<RadarSiteDataService>(radarSite));
   return insertedIt->second;
}

void RadarSiteDataService::LoadLatestLevel2Data()
{
   logger_->info("Requesting latest Level 2 data for {}", p->radarSite_);

   scwx::util::async(
      [this]()
      {
         try
         {
            p->level2Provider_->Refresh();

            const std::string key = p->level2Provider_->FindLatestKey();
            if (key.empty())
            {
               logger_->warn("No Level 2 data available for {}", p->radarSite_);
               Q_EMIT LoadFailed(QStringLiteral("No data available"));
               return;
            }

            auto nexradFile = p->level2Provider_->LoadObjectByKey(key);
            auto ar2vFile =
               std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(nexradFile);

            if (ar2vFile == nullptr)
            {
               logger_->warn("Failed to load/parse Level 2 data for {}",
                             p->radarSite_);
               Q_EMIT LoadFailed(QStringLiteral("Failed to load data"));
               return;
            }

            logger_->info("Loaded {} messages for {} ({} elevation scans)",
                          ar2vFile->message_count(),
                          p->radarSite_,
                          ar2vFile->radar_data().size());
            Q_EMIT LevelTwoDataLoaded(ar2vFile);
         }
         catch (const std::exception& ex)
         {
            logger_->error("Exception loading Level 2 data for {}: {}",
                           p->radarSite_,
                           ex.what());
            Q_EMIT LoadFailed(QString::fromStdString(ex.what()));
         }
      });
}

} // namespace data
} // namespace nimbus
