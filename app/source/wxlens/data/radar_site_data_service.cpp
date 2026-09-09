#include <wxlens/data/frame_cache.hpp>
#include <wxlens/data/radar_site_data_service.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/products/level3_product_catalog.hpp>

#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/wsr88d/rda/generic_radar_data.hpp>
#include <scwx/wsr88d/rpg/level3_message.hpp>

#include <algorithm>
#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <QTimer>
#include <QElapsedTimer>

namespace wxlens
{
namespace data
{

static const std::string logPrefix_ = "data.radar_site_data_service";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

namespace
{

/**
 * Per-site retention budgets (docs/ROADMAP.md, 2026-09-09 checklist). These are
 * per RadarSiteDataService instance, and one instance exists per radar site, so
 * a user watching several sites holds several budgets. Sized against the
 * roadmap's 8 GB low-end floor rather than this development machine.
 *
 * Provisional: no decoded-frame size has been measured on the modest-laptop
 * target yet, so every load logs its estimated size to let the next measurement
 * session calibrate these rather than guess again.
 */
constexpr std::size_t kLevel2CapacityBytes = 256U * 1024U * 1024U;
constexpr std::size_t kLevel3CapacityBytes = 64U * 1024U * 1024U;

/**
 * Floor applied to every size estimate. wxdata reports decoded payload sizes,
 * not allocation footprints, and a family that reports zero would otherwise let
 * the cache grow without bound in entry count while staying "within budget".
 * The floor makes retention monotonic, so the byte budget also bounds the entry
 * count (256 Level 3 frames, 1024 Level 2 volumes at the budgets above).
 */
constexpr std::size_t kMinimumFrameBytes = 256U * 1024U;

/// Sums the decoded moment payloads across every radial of every elevation
/// scan. This is an estimate of retained data, not an exact allocation size:
/// it excludes the map/shared_ptr overhead wxdata does not expose.
std::size_t EstimateLevel2Bytes(const scwx::wsr88d::Ar2vFile& file)
{
   std::size_t bytes = 0U;

   for (const auto& [elevationNumber, scan] : file.radar_data())
   {
      if (scan == nullptr)
      {
         continue;
      }

      for (const auto& [radialNumber, radial] : *scan)
      {
         if (radial != nullptr)
         {
            bytes += radial->data_size();
         }
      }
   }

   return std::max(bytes, kMinimumFrameBytes);
}

std::size_t EstimateLevel3Bytes(const scwx::wsr88d::Level3File& file)
{
   const auto message = file.message();
   const std::size_t bytes = (message != nullptr) ? message->data_size() : 0U;
   return std::max(bytes, kMinimumFrameBytes);
}

/// Log-friendly name for how a load was served, so the metrics lines report
/// measured cache behaviour instead of a hard-coded value.
const char* OriginName(FrameCache<scwx::wsr88d::Ar2vFile>::Origin origin)
{
   using Origin = FrameCache<scwx::wsr88d::Ar2vFile>::Origin;
   switch (origin)
   {
   case Origin::Cache:
      return "cache";
   case Origin::Deduplicated:
      return "deduplicated";
   case Origin::Loaded:
   default:
      return "loaded";
   }
}

const char* OriginName(FrameCache<scwx::wsr88d::Level3File>::Origin origin)
{
   using Origin = FrameCache<scwx::wsr88d::Level3File>::Origin;
   switch (origin)
   {
   case Origin::Cache:
      return "cache";
   case Origin::Deduplicated:
      return "deduplicated";
   case Origin::Loaded:
   default:
      return "loaded";
   }
}

} // namespace

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

   // Both caches are shared by every pane viewing this site (§4.6). They retain
   // *decoded* files keyed by provider object key, so reselecting a frame skips
   // the download and the parse; per-pane product/time independence is
   // unaffected, because the key - not the pane - identifies the entry.
   FrameCache<scwx::wsr88d::Ar2vFile>  level2Cache_ {kLevel2CapacityBytes};
   FrameCache<scwx::wsr88d::Level3File> level3Cache_ {kLevel3CapacityBytes};

   std::vector<products::Level3ProductDescriptor> level3Catalog_;
   std::atomic_bool     catalogLoadInProgress_ {false};
   std::atomic_uint64_t nextRequestId_ {1};
   std::atomic_bool     liveLoadInProgress_ {false};
   QTimer               refreshTimer_;

   /// Latest-volume key most recently published to consumers. A periodic
   /// refresh that rediscovers this same key has nothing new to say, so it
   /// stops rather than making every product rebuild identical geometry.
   std::mutex  latestKeyMutex_;
   std::string lastPublishedLatestKey_;

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
   // Deliberately not connected straight to LoadLatestLevel2Data: a periodic
   // poll that finds the same volume must stay silent rather than republish it.
   connect(&p->refreshTimer_,
           &QTimer::timeout,
           this,
           [this]() { LoadLatestLevel2DataInternal(false); });
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
   LoadLatestLevel2DataInternal(true);
}

void RadarSiteDataService::LoadLatestLevel2DataInternal(bool publishUnchanged)
{
   if (p->liveLoadInProgress_.exchange(true))
      return;
   logger_->info("Requesting latest Level 2 data for {}", p->radarSite_);

   scwx::util::async(
      [this, publishUnchanged]()
      {
         try
         {
            QElapsedTimer stageTimer;
            stageTimer.start();
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

            // Nothing new since the last publish, and no consumer is waiting on
            // a first frame: stop before the download *and* before making every
            // product rebuild the geometry it already has.
            if (!publishUnchanged)
            {
               std::lock_guard lock {p->latestKeyMutex_};
               if (key == p->lastPublishedLatestKey_)
               {
                  logger_->debug(
                     "Level 2 refresh for {}: latest volume unchanged ({}), "
                     "skipping reload",
                     p->radarSite_,
                     key);
                  p->liveLoadInProgress_ = false;
                  return;
               }
            }

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            const auto load = p->level2Cache_.Load(
               key,
               [this, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
                     p->level2Provider_->LoadObjectByKey(key));
               },
               EstimateLevel2Bytes);
            auto ar2vFile = load.value;
            logger_->info(
               "Level 2 load metrics: site={} key={} listing_ms={:.3f} "
               "download_decode_ms={:.3f} decoded_cache_hit={} origin={} "
               "cache_bytes={} cache_frames={} success={}",
               p->radarSite_,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               p->level2Cache_.size_bytes(),
               p->level2Cache_.count(),
               ar2vFile != nullptr);

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
            {
               std::lock_guard lock {p->latestKeyMutex_};
               p->lastPublishedLatestKey_ = key;
            }
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
            QElapsedTimer stageTimer;
            stageTimer.start();
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

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            const auto load = p->level2Cache_.Load(
               key,
               [this, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
                     p->level2Provider_->LoadObjectByKey(key));
               },
               EstimateLevel2Bytes);
            auto file = load.value;
            logger_->info(
               "Level 2 archive metrics: site={} request={} key={} "
               "listing_ms={:.3f} download_decode_ms={:.3f} "
               "decoded_cache_hit={} origin={} cache_bytes={} cache_frames={} "
               "success={}",
               p->radarSite_,
               requestId,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               p->level2Cache_.size_bytes(),
               p->level2Cache_.count(),
               file != nullptr);
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
            QElapsedTimer stageTimer;
            stageTimer.start();
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

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            // AWIPS id stays part of the key so two products cannot collide on
            // a shared object key, exactly as the previous ad-hoc map did.
            const std::string cacheKey = awipsId + '\n' + key;
            const auto        load     = p->level3Cache_.Load(
               cacheKey,
               [&provider, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Level3File>(
                     provider->LoadObjectByKey(key));
               },
               EstimateLevel3Bytes);
            auto file = load.value;
            logger_->info(
               "Level 3 load metrics: site={} awips={} request={} key={} "
               "listing_ms={:.3f} cache_or_download_decode_ms={:.3f} "
               "decoded_cache_hit={} origin={} cache_bytes={} cache_frames={} "
               "success={}",
               p->radarSite_,
               awipsId,
               requestId,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               p->level3Cache_.size_bytes(),
               p->level3Cache_.count(),
               file != nullptr);
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
