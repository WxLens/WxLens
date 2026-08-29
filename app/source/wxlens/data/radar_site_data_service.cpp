#include <wxlens/data/radar_site_data_service.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/products/level3_product_catalog.hpp>

#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/threads.hpp>

#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <QTimer>

namespace wxlens
{
namespace data
{

static const std::string logPrefix_ = "data.radar_site_data_service";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

class RadarSiteDataService::Impl
{
public:
   explicit Impl(const std::string& radarSite) :
       radarSite_ {radarSite},
       level2Provider_ {
          scwx::provider::NexradDataProviderFactory::CreateLevel2DataProvider(
             radarSite)}
   {
   }

   std::string                                         radarSite_;
   std::shared_ptr<scwx::provider::NexradDataProvider> level2Provider_;
   std::mutex                                          level3Mutex_;
   std::unordered_map<std::string,
                      std::shared_ptr<scwx::provider::NexradDataProvider>>
      level3Providers_;
   std::unordered_map<std::string, std::shared_ptr<scwx::wsr88d::Level3File>>
                                                  level3Cache_;
   std::vector<products::Level3ProductDescriptor> level3Catalog_;
   std::atomic_bool     catalogLoadInProgress_ {false};
   std::atomic_uint64_t nextRequestId_ {1};
   std::atomic_bool     liveLoadInProgress_ {false};
   QTimer               refreshTimer_;

   std::shared_ptr<scwx::provider::NexradDataProvider>
   GetLevel3Provider(const std::string& awipsId)
   {
      std::lock_guard lock {level3Mutex_};
      auto [it, inserted] = level3Providers_.try_emplace(awipsId);
      if (inserted)
      {
         it->second =
            scwx::provider::NexradDataProviderFactory::CreateLevel3DataProvider(
               radarSite_, awipsId);
      }
      return it->second;
   }
};

RadarSiteDataService::RadarSiteDataService(const std::string& radarSite) :
    p {std::make_unique<Impl>(radarSite)}
{
   p->refreshTimer_.setInterval(std::chrono::minutes {1});
   connect(&p->refreshTimer_,
           &QTimer::timeout,
           this,
           &RadarSiteDataService::LoadLatestLevel2Data);
   p->refreshTimer_.start();
}

RadarSiteDataService::~RadarSiteDataService()
{
   p->level2Provider_->Shutdown();
   std::lock_guard lock {p->level3Mutex_};
   for (const auto& [awipsId, provider] : p->level3Providers_)
   {
      provider->Shutdown();
   }
}

const std::string& RadarSiteDataService::radar_site() const
{ return p->radarSite_; }

std::shared_ptr<RadarSiteDataService>
RadarSiteDataService::Instance(const std::string& radarSite)
{
   static std::shared_mutex instanceMutex;
   static std::map<std::string, std::shared_ptr<RadarSiteDataService>>
      instances;

   std::shared_lock readLock {instanceMutex};
   auto             it = instances.find(radarSite);
   if (it != instances.end())
   {
      return it->second;
   }
   readLock.unlock();

   std::unique_lock writeLock {instanceMutex};
   auto [insertedIt, inserted] = instances.try_emplace(
      radarSite, std::make_shared<RadarSiteDataService>(radarSite));
   return insertedIt->second;
}

void RadarSiteDataService::LoadLatestLevel2Data()
{
   if (p->liveLoadInProgress_.exchange(true))
      return;
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
               QMetaObject::invokeMethod(
                  this,
                  [this]()
                  { Q_EMIT LoadFailed(QStringLiteral("No data available")); },
                  Qt::QueuedConnection);
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
               QMetaObject::invokeMethod(
                  this,
                  [this]()
                  { Q_EMIT LoadFailed(QStringLiteral("Failed to load data")); },
                  Qt::QueuedConnection);
               return;
            }

            logger_->info("Loaded {} messages for {} ({} elevation scans)",
                          ar2vFile->message_count(),
                          p->radarSite_,
                          ar2vFile->radar_data().size());
            p->liveLoadInProgress_ = false;
            QMetaObject::invokeMethod(
               this,
               [this, ar2vFile]() { Q_EMIT LevelTwoDataLoaded(ar2vFile); },
               Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            logger_->error("Exception loading Level 2 data for {}: {}",
                           p->radarSite_,
                           ex.what());
            p->liveLoadInProgress_ = false;
            const QString reason   = QString::fromStdString(ex.what());
            QMetaObject::invokeMethod(
               this,
               [this, reason]() { Q_EMIT LoadFailed(reason); },
               Qt::QueuedConnection);
         }
      });
}

std::uint64_t RadarSiteDataService::LoadLevel2DataAt(
   std::chrono::system_clock::time_point time)
{
   const std::uint64_t requestId = p->nextRequestId_.fetch_add(1);
   logger_->info("Requesting archived Level 2 data for {} (request {})",
                 p->radarSite_,
                 requestId);

   scwx::util::async(
      [this, requestId, time]()
      {
         try
         {
            const auto [success, newObjects, totalObjects] =
               p->level2Provider_->ListObjects(time);
            if (!success)
            {
               QMetaObject::invokeMethod(
                  this,
                  [this, requestId]()
                  {
                     Q_EMIT RequestFailed(
                        requestId, QStringLiteral("Archive listing failed"));
                  },
                  Qt::QueuedConnection);
               return;
            }

            const std::string key = p->level2Provider_->FindKey(time);
            if (key.empty())
            {
               QMetaObject::invokeMethod(
                  this,
                  [this, requestId]()
                  {
                     Q_EMIT RequestFailed(
                        requestId,
                        QStringLiteral("No volume available at that time"));
                  },
                  Qt::QueuedConnection);
               return;
            }

            auto file = std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
               p->level2Provider_->LoadObjectByKey(key));
            if (file == nullptr)
            {
               QMetaObject::invokeMethod(
                  this,
                  [this, requestId]()
                  {
                     Q_EMIT RequestFailed(
                        requestId,
                        QStringLiteral("Failed to load archived volume"));
                  },
                  Qt::QueuedConnection);
               return;
            }

            const auto actualTime = p->level2Provider_->GetTimePointByKey(key);
            logger_->info(
               "Loaded archived Level 2 data for {} (request {}, {} objects)",
               p->radarSite_,
               requestId,
               totalObjects);
            QMetaObject::invokeMethod(
               this,
               [this, requestId, file, actualTime]()
               {
                  Q_EMIT LevelTwoDataLoadedForRequest(
                     requestId, file, actualTime);
               },
               Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            logger_->error("Archive request {} for {} failed: {}",
                           requestId,
                           p->radarSite_,
                           ex.what());
            const QString reason = QString::fromStdString(ex.what());
            QMetaObject::invokeMethod(
               this,
               [this, requestId, reason]()
               { Q_EMIT RequestFailed(requestId, reason); },
               Qt::QueuedConnection);
         }
      });
   return requestId;
}

void RadarSiteDataService::RefreshLevel3Catalog()
{
   if (p->catalogLoadInProgress_.exchange(true))
      return;
   Q_EMIT LevelThreeCatalogLoading();
   logger_->info("Requesting Level 3 product catalog for {}", p->radarSite_);

   scwx::util::async(
      [this]()
      {
         try
         {
            // Availability is site-wide; wxdata providers expose it through any
            // Level 3 instance. N0B is only the discovery transport, not an
            // assumed available product.
            auto provider = p->GetLevel3Provider("N0B");
            provider->RequestAvailableProducts();
            auto catalog = products::BuildLevel3ProductCatalog(
               provider->GetAvailableProducts());
            {
               std::lock_guard lock {p->level3Mutex_};
               p->level3Catalog_ = catalog;
            }
            p->catalogLoadInProgress_ = false;
            QMetaObject::invokeMethod(
               this,
               [this, catalog = std::move(catalog)]()
               { Q_EMIT LevelThreeCatalogReady(catalog); },
               Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            p->catalogLoadInProgress_ = false;
            const QString reason      = QString::fromStdString(ex.what());
            logger_->error("Level 3 catalog request for {} failed: {}",
                           p->radarSite_,
                           ex.what());
            QMetaObject::invokeMethod(
               this,
               [this, reason]() { Q_EMIT LevelThreeCatalogFailed(reason); },
               Qt::QueuedConnection);
         }
      });
}

std::uint64_t
RadarSiteDataService::LoadLatestLevel3Data(const std::string& awipsId)
{
   return LoadLevel3DataAt(awipsId,
                           std::chrono::system_clock::time_point::max());
}

std::uint64_t RadarSiteDataService::LoadLevel3DataAt(
   const std::string& awipsId, std::chrono::system_clock::time_point time)
{
   const std::uint64_t requestId = p->nextRequestId_.fetch_add(1);
   const bool   latest = time == std::chrono::system_clock::time_point::max();
   const qint64 selectedTimeMs =
      latest ? -1 :
               std::chrono::duration_cast<std::chrono::milliseconds>(
                  time.time_since_epoch())
                  .count();
   Q_EMIT LevelThreeRequestStarted(
      requestId, QString::fromStdString(awipsId), selectedTimeMs);
   logger_->info("Requesting {} Level 3 {} for {} (request {})",
                 latest ? "latest" : "archived",
                 awipsId,
                 p->radarSite_,
                 requestId);

   scwx::util::async(
      [this, requestId, awipsId, time, latest]()
      {
         const QString qAwipsId = QString::fromStdString(awipsId);
         try
         {
            auto provider = p->GetLevel3Provider(awipsId);
            if (latest)
            {
               provider->Refresh();
            }
            else
            {
               const auto [success, newObjects, totalObjects] =
                  provider->ListObjects(time);
               if (!success)
               {
                  QMetaObject::invokeMethod(
                     this,
                     [this, requestId, qAwipsId]()
                     {
                        Q_EMIT LevelThreeRequestFailed(
                           requestId,
                           qAwipsId,
                           QStringLiteral("Archive listing failed"));
                     },
                     Qt::QueuedConnection);
                  return;
               }
            }

            const std::string key =
               latest ? provider->FindLatestKey() : provider->FindKey(time);
            if (key.empty())
            {
               QMetaObject::invokeMethod(
                  this,
                  [this, requestId, qAwipsId]()
                  {
                     Q_EMIT LevelThreeRequestFailed(
                        requestId,
                        qAwipsId,
                        QStringLiteral("No product available"));
                  },
                  Qt::QueuedConnection);
               return;
            }

            const std::string cacheKey = awipsId + '\n' + key;
            std::shared_ptr<scwx::wsr88d::Level3File> file;
            {
               std::lock_guard lock {p->level3Mutex_};
               auto            it = p->level3Cache_.find(cacheKey);
               if (it != p->level3Cache_.end())
                  file = it->second;
            }
            if (file == nullptr)
            {
               file = std::dynamic_pointer_cast<scwx::wsr88d::Level3File>(
                  provider->LoadObjectByKey(key));
               if (file != nullptr)
               {
                  std::lock_guard lock {p->level3Mutex_};
                  p->level3Cache_.insert_or_assign(cacheKey, file);
               }
            }
            if (file == nullptr)
            {
               QMetaObject::invokeMethod(
                  this,
                  [this, requestId, qAwipsId]()
                  {
                     Q_EMIT LevelThreeRequestFailed(
                        requestId,
                        qAwipsId,
                        QStringLiteral("Failed to load Level 3 product"));
                  },
                  Qt::QueuedConnection);
               return;
            }

            const auto actualTime = provider->GetTimePointByKey(key);
            QMetaObject::invokeMethod(
               this,
               [this, requestId, qAwipsId, file, actualTime]()
               {
                  Q_EMIT LevelThreeDataLoadedForRequest(
                     requestId, qAwipsId, file, actualTime);
               },
               Qt::QueuedConnection);
         }
         catch (const std::exception& ex)
         {
            const QString reason = QString::fromStdString(ex.what());
            logger_->error("Level 3 request {} for {} {} failed: {}",
                           requestId,
                           p->radarSite_,
                           awipsId,
                           ex.what());
            QMetaObject::invokeMethod(
               this,
               [this, requestId, qAwipsId, reason]()
               { Q_EMIT LevelThreeRequestFailed(requestId, qAwipsId, reason); },
               Qt::QueuedConnection);
         }
      });
   return requestId;
}

std::vector<products::Level3ProductDescriptor>
RadarSiteDataService::level3_catalog() const
{
   std::lock_guard lock {p->level3Mutex_};
   return p->level3Catalog_;
}

} // namespace data
} // namespace wxlens
