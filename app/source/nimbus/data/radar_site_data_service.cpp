#include <nimbus/data/radar_site_data_service.hpp>
#include <nimbus/log/logger.hpp>

#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/threads.hpp>

#include <map>
#include <atomic>
#include <shared_mutex>
#include <QTimer>

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
   std::atomic_uint64_t nextRequestId_ {1};
   std::atomic_bool liveLoadInProgress_ {false};
   QTimer refreshTimer_;
};

RadarSiteDataService::RadarSiteDataService(const std::string& radarSite) :
    p {std::make_unique<Impl>(radarSite)}
{
   p->refreshTimer_.setInterval(std::chrono::minutes {1});
   connect(&p->refreshTimer_, &QTimer::timeout, this,
           &RadarSiteDataService::LoadLatestLevel2Data);
   p->refreshTimer_.start();
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
   if (p->liveLoadInProgress_.exchange(true)) return;
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
               p->liveLoadInProgress_ = false;
               QMetaObject::invokeMethod(this, [this]() {
                  Q_EMIT LoadFailed(QStringLiteral("No data available"));
               }, Qt::QueuedConnection);
               return;
            }

            auto nexradFile = p->level2Provider_->LoadObjectByKey(key);
            auto ar2vFile =
               std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(nexradFile);

            if (ar2vFile == nullptr)
            {
               logger_->warn("Failed to load/parse Level 2 data for {}",
                             p->radarSite_);
               p->liveLoadInProgress_ = false;
               QMetaObject::invokeMethod(this, [this]() {
                  Q_EMIT LoadFailed(QStringLiteral("Failed to load data"));
               }, Qt::QueuedConnection);
               return;
            }

            logger_->info("Loaded {} messages for {} ({} elevation scans)",
                          ar2vFile->message_count(),
                          p->radarSite_,
                          ar2vFile->radar_data().size());
            p->liveLoadInProgress_ = false;
            QMetaObject::invokeMethod(this, [this, ar2vFile]() {
               Q_EMIT LevelTwoDataLoaded(ar2vFile);
            }, Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            logger_->error("Exception loading Level 2 data for {}: {}",
                           p->radarSite_,
                           ex.what());
            p->liveLoadInProgress_ = false;
            const QString reason = QString::fromStdString(ex.what());
            QMetaObject::invokeMethod(this, [this, reason]() {
               Q_EMIT LoadFailed(reason);
            }, Qt::QueuedConnection);
         }
      });
}

std::uint64_t RadarSiteDataService::LoadLevel2DataAt(
   std::chrono::system_clock::time_point time)
{
   const std::uint64_t requestId = p->nextRequestId_.fetch_add(1);
   logger_->info("Requesting archived Level 2 data for {} (request {})",
                 p->radarSite_, requestId);

   scwx::util::async(
      [this, requestId, time]()
      {
         try
         {
            const auto [success, newObjects, totalObjects] = p->level2Provider_->ListObjects(time);
            if (!success)
            {
               QMetaObject::invokeMethod(this, [this, requestId]() {
                  Q_EMIT RequestFailed(requestId, QStringLiteral("Archive listing failed"));
               }, Qt::QueuedConnection);
               return;
            }

            const std::string key = p->level2Provider_->FindKey(time);
            if (key.empty())
            {
               QMetaObject::invokeMethod(this, [this, requestId]() {
                  Q_EMIT RequestFailed(requestId,
                                       QStringLiteral("No volume available at that time"));
               }, Qt::QueuedConnection);
               return;
            }

            auto file = std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
               p->level2Provider_->LoadObjectByKey(key));
            if (file == nullptr)
            {
               QMetaObject::invokeMethod(this, [this, requestId]() {
                  Q_EMIT RequestFailed(requestId,
                                       QStringLiteral("Failed to load archived volume"));
               }, Qt::QueuedConnection);
               return;
            }

            const auto actualTime = p->level2Provider_->GetTimePointByKey(key);
            logger_->info("Loaded archived Level 2 data for {} (request {}, {} objects)",
                          p->radarSite_, requestId, totalObjects);
            QMetaObject::invokeMethod(this, [this, requestId, file, actualTime]() {
               Q_EMIT LevelTwoDataLoadedForRequest(requestId, file, actualTime);
            }, Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            logger_->error("Archive request {} for {} failed: {}",
                           requestId, p->radarSite_, ex.what());
            const QString reason = QString::fromStdString(ex.what());
            QMetaObject::invokeMethod(this, [this, requestId, reason]() {
               Q_EMIT RequestFailed(requestId, reason);
            }, Qt::QueuedConnection);
         }
      });
   return requestId;
}

} // namespace data
} // namespace nimbus
