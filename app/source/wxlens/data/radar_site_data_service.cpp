#include <wxlens/data/frame_cache.hpp>
#include <wxlens/data/radar_site_data_service.hpp>
#include <wxlens/data/radar_task_queue.hpp>
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
#include <QCoreApplication>
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
std::atomic_int historyMinutes_ {30};

/**
 * Background warming posts to the shared `scwx::util::io_context()`, which main() joins on the way
 * out. Without a way to stand down, quitting part-way through a window's history made the process
 * linger for the rest of those downloads - long enough to look like a hang rather than a slow exit.
 * A download already in flight still has to finish; wxdata's provider has no cancellation, so this
 * bounds the wait at one object instead of the whole window.
 */
std::atomic_bool backgroundCancelled_ {false};

/// Unwinds the warming task from wherever it happens to be when the application quits.
struct CancelledException
{
};

constexpr std::size_t kLevel2CapacityBytes = 256U * 1024U * 1024U;
constexpr std::size_t kLevel3CapacityBytes = 64U * 1024U * 1024U;

/**
 * Measured 2026-09-25 against live KEAX: ~56 MB of decoded moments per volume,
 * a new volume every ~5 minutes. That retires the "no decoded-frame size has
 * been measured" note above for Level 2 on this hardware.
 *
 * The playback window therefore decides the budget, not the other way round: at
 * the fixed 256 MB above, a 30-minute window held four of its six volumes, so
 * every one-minute refresh re-downloaded the two it had just evicted. 15 MB per
 * minute covers a 4.5-minute VCP with headroom, and the ceiling keeps the
 * 120-minute setting from asking for 1.8 GB - past it the window genuinely does
 * not fit, which the status line says rather than silently thrashing.
 */
constexpr std::size_t kLevel2BytesPerMinute  = 15U * 1024U * 1024U;
constexpr std::size_t kLevel2MaxCapacityBytes = 1024U * 1024U * 1024U;

std::size_t Level2CapacityBytes(int minutes)
{
   return std::clamp(static_cast<std::size_t>(minutes) * kLevel2BytesPerMinute,
                     kLevel2CapacityBytes,
                     kLevel2MaxCapacityBytes);
}

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

/**
 * Everything a background task touches, owned separately from the QObject.
 *
 * This split is the whole point of the lifecycle slice. Background work used to capture the
 * service's raw `this`, which was harmless only because the registry kept every service alive
 * forever. Once the registry holds weak references, a task outliving its service would be a
 * use-after-free - and the obvious fix (let tasks hold a `shared_ptr` to the service) trades it
 * for a different bug: the last reference would then be dropped on a worker thread, destroying
 * a QObject that owns two GUI-thread-affine QTimers.
 *
 * Holding the task-visible state in its own shared object avoids both. Workers keep this alive
 * for as long as they need it; the QObject's reference count stays a true *consumer* count whose
 * final decrement is always on the GUI thread; and no custom deleter or explicit lease type is
 * needed to arrange that.
 */
struct RadarSiteDataService::TaskState
{
   explicit TaskState(const std::string& radarSite) :
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
   std::atomic_bool     historyInProgress_ {false};

   /**
    * Per-instance cancellation, distinct from the process-global `backgroundCancelled_`.
    *
    * The global flag is for application shutdown and can only ever be set once, for every site
    * at once; using it to stand one site down would silence every other radar for the rest of
    * the process. Every task checks both.
    */
   std::atomic_bool cancelled_ {false};

   /// Latest-volume key most recently published to consumers. A periodic
   /// refresh that rediscovers this same key has nothing new to say, so it
   /// stops rather than making every product rebuild identical geometry.
   std::mutex  latestKeyMutex_;
   std::string lastPublishedLatestKey_;

   [[nodiscard]] bool Cancelled() const
   {
      return cancelled_.load() || backgroundCancelled_.load();
   }

   /// Throws out of whatever stage the task is in. Callers catch CancelledException and return
   /// without publishing, exactly as the shutdown path already did.
   void ThrowIfCancelled() const
   {
      if (Cancelled())
      {
         throw CancelledException {};
      }
   }

   /// Tasks that read or write this site's provider listings share this key, so two of them
   /// never overlap - see RadarTaskQueue's note on wxdata's UpdateMetadata race.
   [[nodiscard]] std::string ListSerialKey() const { return radarSite_ + "/list"; }

   /// Prefix every one of this site's queue entries starts with, so deactivation can drop them
   /// all in one call.
   [[nodiscard]] std::string QueuePrefix() const { return radarSite_ + "/"; }

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

   /// Terminal. Aborts an in-flight S3 transfer through wxdata's own
   /// running_/SetContinueRequestHandler path and refuses further requests.
   void ShutdownProviders()
   {
      level2Provider_->Shutdown();
      std::lock_guard lock {level3Mutex_};
      for (const auto& [awipsId, provider] : level3Providers_)
      {
         provider->Shutdown();
      }
   }
};

class RadarSiteDataService::Impl
{
public:
   explicit Impl(const std::string& radarSite) :
       task_ {std::make_shared<TaskState>(radarSite)}
   {
   }

   /// Shared with every background task. Outlives this Impl whenever a task is still draining.
   std::shared_ptr<TaskState> task_;

   // GUI-thread-only state. No background task touches any of this.
   QTimer        refreshTimer_;
   QTimer        retentionTimer_;
   bool          historyRequested_ {false};
   QList<qint64> recentFrames_;
};

/**
 * Publishes `fn` on the GUI thread if the service is still alive when it gets there.
 *
 * `weak` is locked on the GUI thread, never on the worker, which is what keeps the resulting
 * strong reference's destruction on the GUI thread too. QCoreApplication is the context object
 * because it outlives every service; the weak pointer, not the context, is what makes a late
 * completion from an abandoned site a no-op.
 */
namespace
{
template<typename Fn>
void PublishToGui(const std::weak_ptr<RadarSiteDataService>& weak, Fn&& fn)
{
   QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [weak, fn = std::forward<Fn>(fn)]()
      {
         if (auto self = weak.lock(); self != nullptr)
         {
            fn(*self);
         }
      },
      Qt::QueuedConnection);
}
} // namespace

RadarSiteDataService::RadarSiteDataService(const std::string& radarSite) :
    p {std::make_unique<Impl>(radarSite)}
{
   // Timers are owned by this QObject and only ever touched on its thread, so capturing `this`
   // in their handlers is safe by construction: a timer cannot fire after the object that owns
   // it has been destroyed.
   p->refreshTimer_.setInterval(std::chrono::minutes {1});
   // Deliberately not connected straight to LoadLatestLevel2Data: a periodic
   // poll that finds the same volume must stay silent rather than republish it.
   connect(&p->refreshTimer_,
           &QTimer::timeout,
           this,
           [this]() { LoadLatestLevel2DataInternal(false); });
   p->refreshTimer_.start();
   connect(&p->refreshTimer_, &QTimer::timeout, this, [this]()
   { if (p->historyRequested_) RequestRecentHistory(); });
   // The window is minutes wide, so a coarse sweep keeps an idle app from waking every second.
   p->retentionTimer_.setInterval(15000);
   connect(&p->retentionTimer_, &QTimer::timeout, this, [this]()
   {
      const auto cutoff = std::chrono::system_clock::now() - std::chrono::minutes {HistoryMinutes()};
      p->task_->level2Cache_.SetCapacityBytes(Level2CapacityBytes(HistoryMinutes()));
      p->task_->level2Cache_.PruneBefore(cutoff);
      p->task_->level3Cache_.PruneBefore(cutoff);
      const auto cutoffMs = std::chrono::duration_cast<std::chrono::milliseconds>(
         cutoff.time_since_epoch()).count();
      // QList has no std::erase_if overload; removeIf is Qt's own equivalent.
      if (p->recentFrames_.removeIf([cutoffMs](qint64 time) { return time <= cutoffMs; }) > 0)
      {
         Q_EMIT RecentFramesChanged(p->recentFrames_, {});
      }
   });
   p->retentionTimer_.start();
}

RadarSiteDataService::~RadarSiteDataService()
{
   // Reached only when the last consumer releases its reference, which is always on this
   // object's own thread - see the class comment for why a background task cannot get here.
   p->refreshTimer_.stop();
   p->retentionTimer_.stop();

   // Order matters. Setting the flag first means a task between stages unwinds rather than
   // starting another provider call; dropping the queue entries then guarantees nothing that
   // has not started ever will; shutting the providers down last aborts whatever is already
   // mid-transfer. The reverse order leaves a window where a task passes its cancellation check
   // and then issues a request against a provider that is about to be shut down.
   p->task_->cancelled_.store(true);
   const auto dropped = RadarTaskQueue::Instance().DropQueued(p->task_->QueuePrefix());
   p->task_->ShutdownProviders();

   logger_->info("Released radar site service {} (dropped {} queued tasks)",
                 p->task_->radarSite_,
                 dropped);
}

bool RadarSiteDataService::active() const
{
   return !p->task_->Cancelled();
}

void RadarSiteDataService::SetHistoryMinutes(int minutes)
{
   historyMinutes_.store(std::clamp(minutes, 5, 120));
}
int RadarSiteDataService::HistoryMinutes() { return historyMinutes_.load(); }
QList<qint64> RadarSiteDataService::recentFrames() const { return p->recentFrames_; }

void RadarSiteDataService::CancelBackgroundWork()
{
   backgroundCancelled_.store(true);
}

void RadarSiteDataService::RequestRecentHistory()
{
   if (backgroundCancelled_.load() || !active()) return;
   p->historyRequested_ = true;
   if (p->task_->liveLoadInProgress_)
   {
      QTimer::singleShot(1000, this, [this]() { RequestRecentHistory(); });
      return;
   }
   if (p->task_->historyInProgress_.exchange(true)) return;

   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;

   // Listing only. The per-frame downloads this discovers are posted as their own tasks by
   // WarmHistoryFrames rather than run inline: one task doing all of them held this site's
   // serial key for the whole window (~25 s measured), which is exactly what a foreground seek
   // on this site then queued behind. Splitting them means the key is held for one listing and
   // every subsequent frame is independently preemptible by foreground work.
   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Background,
      state->ListSerialKey(),
      [weak, state]()
   {
      QList<qint64> frames;
      QString error;
      try
      {
         state->ThrowIfCancelled();
         const auto now = std::chrono::system_clock::now();
         const auto start = now - std::chrono::minutes {HistoryMinutes()};
         const auto capacityBytes = Level2CapacityBytes(HistoryMinutes());
         state->level2Cache_.SetCapacityBytes(capacityBytes);
         state->level2Cache_.PruneBefore(start);
         for (auto day = std::chrono::floor<std::chrono::days>(start);
              day <= std::chrono::floor<std::chrono::days>(now); day += std::chrono::days {1})
         {
            state->ThrowIfCancelled();
            const auto [success, added, total] = state->level2Provider_->ListObjects(day);
            if (!success) { error = QStringLiteral("Recent scan listing failed"); continue; }
            for (const auto time : state->level2Provider_->GetTimePointsByDate(day, false))
               if (time > start && time <= now)
                  frames.append(std::chrono::duration_cast<std::chrono::milliseconds>(
                     time.time_since_epoch()).count());
         }
         std::sort(frames.begin(), frames.end());
         frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
         PublishToGui(weak, [frames, error](RadarSiteDataService& self)
         { self.p->recentFrames_ = frames; Q_EMIT self.RecentFramesChanged(frames, error); });

         state->ThrowIfCancelled();
         WarmHistoryFrames(weak, state, frames, capacityBytes, error);
      }
      catch (const CancelledException&)
      {
         // Quitting, or this site was released. Neither is an error the user needs told about,
         // and a queued reply would touch a service that is on its way out.
         state->historyInProgress_ = false;
      }
      catch (const std::exception& ex)
      {
         const QString reason = QString::fromStdString(ex.what());
         state->historyInProgress_ = false;
         PublishToGui(weak, [reason](RadarSiteDataService& self)
         { Q_EMIT self.RecentFramesChanged(self.p->recentFrames_, reason); });
      }
   });
}

/**
 * Warms the discovered frames newest-first, one queued task per frame.
 *
 * Uses the same cache and deduplication as a foreground load. Stopping *before* the budget is
 * exceeded rather than after is what keeps this idempotent: one frame too many evicts the
 * oldest, which the next refresh then downloads again. A frame's size is only known once it is
 * decoded, so the running total is carried from one task to the next rather than decided up
 * front - which also preserves newest-first order without serializing the chain behind a queue
 * key that would block foreground work on this site.
 */
void RadarSiteDataService::WarmHistoryFrames(
   const std::weak_ptr<RadarSiteDataService>& weak,
   const std::shared_ptr<TaskState>&          state,
   const QList<qint64>&                       frames,
   std::size_t                                capacityBytes,
   const QString&                             listingError)
{
   struct Progress
   {
      std::size_t index {0};
      std::size_t retainedBytes {0};
      std::size_t largestFrameBytes {0};
      int         loaded {0};
      QString     error {};
   };

   auto ordered  = std::make_shared<std::vector<qint64>>(frames.crbegin(), frames.crend());
   auto progress = std::make_shared<Progress>();
   progress->error = listingError;

   auto step = std::make_shared<std::function<void()>>();
   *step     = [weak, state, ordered, progress, capacityBytes, step]()
   {
      const auto finish = [&](bool truncated)
      {
         logger_->info("History cache: site={} minutes={} scans={} downloads={} cache_frames={} "
                       "cache_bytes={} capacity_bytes={} truncated={}",
                       state->radarSite_, HistoryMinutes(), ordered->size(), progress->loaded,
                       state->level2Cache_.count(), state->level2Cache_.size_bytes(),
                       capacityBytes, truncated);
         if (truncated)
            progress->error =
               QStringLiteral("History exceeds memory cache; older frames load on demand");
         state->historyInProgress_ = false;
         const QString error = progress->error;
         PublishToGui(weak, [error](RadarSiteDataService& self)
         { Q_EMIT self.RecentFramesChanged(self.p->recentFrames_, error); });
      };

      try
      {
         state->ThrowIfCancelled();

         if (progress->index >= ordered->size())
         {
            finish(false);
            return;
         }
         if (progress->retainedBytes + progress->largestFrameBytes > capacityBytes)
         {
            finish(true);
            return;
         }

         const auto time = std::chrono::system_clock::time_point {
            std::chrono::milliseconds {(*ordered)[progress->index]}};
         ++progress->index;

         if (time <= std::chrono::system_clock::now() - std::chrono::minutes {HistoryMinutes()})
         {
            // Fell outside the window while queued; every remaining frame is older still.
            finish(false);
            return;
         }

         const auto key = state->level2Provider_->FindKey(time);
         if (!key.empty())
         {
            auto file = state->level2Cache_.Find(key);
            if (!file)
            {
               file = state->level2Cache_
                         .Load(key,
                               [state, key]()
                               {
                                  return std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
                                     state->level2Provider_->LoadObjectByKey(key));
                               },
                               EstimateLevel2Bytes,
                               time)
                         .value;
               if (file) ++progress->loaded;
               else progress->error =
                  QStringLiteral("Some recent scans could not be downloaded");
            }
            if (file)
            {
               const auto bytes = EstimateLevel2Bytes(*file);
               progress->largestFrameBytes = std::max(progress->largestFrameBytes, bytes);
               progress->retainedBytes += bytes;
            }
         }

         // FindKey and LoadObjectByKey are both safe to run concurrently against one provider
         // (shared_lock lookup; no shared mutable state on the download path), so the warm
         // chain carries a key distinct from the listing key - it bounds this site to one warm
         // download at a time without ever blocking a foreground listing.
         RadarTaskQueue::Instance().Post(
            RadarTaskQueue::Priority::Background, state->QueuePrefix() + "warm", *step);
      }
      catch (const CancelledException&)
      {
         state->historyInProgress_ = false;
      }
      catch (const std::exception& ex)
      {
         progress->error = QString::fromStdString(ex.what());
         finish(false);
      }
   };

   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Background, state->QueuePrefix() + "warm", *step);
}

const std::string& RadarSiteDataService::radar_site() const
{ return p->task_->radarSite_; }

namespace
{
std::shared_mutex& InstanceMutex()
{
   static std::shared_mutex mutex;
   return mutex;
}

std::map<std::string, std::weak_ptr<RadarSiteDataService>>& Instances()
{
   static std::map<std::string, std::weak_ptr<RadarSiteDataService>> instances;
   return instances;
}
} // namespace

std::shared_ptr<RadarSiteDataService>
RadarSiteDataService::Instance(const std::string& radarSite)
{
   {
      std::shared_lock readLock {InstanceMutex()};
      const auto       it = Instances().find(radarSite);
      if (it != Instances().end())
      {
         // An expired entry means the last consumer released this site, so its destructor has
         // already shut the providers down for good. Fall through and build a fresh service
         // rather than handing back one that can no longer make a request.
         if (auto existing = it->second.lock(); existing != nullptr)
         {
            return existing;
         }
      }
   }

   std::unique_lock writeLock {InstanceMutex()};
   std::erase_if(Instances(), [](const auto& entry) { return entry.second.expired(); });
   auto& weak = Instances()[radarSite];
   if (auto existing = weak.lock(); existing != nullptr)
   {
      return existing;
   }
   auto created = std::make_shared<RadarSiteDataService>(radarSite);
   weak         = created;
   return created;
}

std::size_t RadarSiteDataService::InstanceCountForTesting()
{
   std::shared_lock readLock {InstanceMutex()};
   return static_cast<std::size_t>(
      std::count_if(Instances().cbegin(),
                    Instances().cend(),
                    [](const auto& entry) { return !entry.second.expired(); }));
}

void RadarSiteDataService::LoadLatestLevel2Data()
{
   LoadLatestLevel2DataInternal(true);
}

void RadarSiteDataService::LoadLatestLevel2DataInternal(bool publishUnchanged)
{
   if (!active()) return;
   if (p->task_->liveLoadInProgress_.exchange(true))
      return;
   logger_->info("Requesting latest Level 2 data for {}", p->task_->radarSite_);

   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;

   // An explicit request is a user waiting on a first frame, so it is foreground. The periodic
   // refresh is not - nobody is blocked on rediscovering a volume that may not even be new -
   // so it must not be allowed to push a site switch back in the queue.
   const auto priority = publishUnchanged ? RadarTaskQueue::Priority::Foreground
                                          : RadarTaskQueue::Priority::Background;

   RadarTaskQueue::Instance().Post(
      priority,
      state->ListSerialKey(),
      [weak, state, publishUnchanged]()
      {
         try
         {
            state->ThrowIfCancelled();

            QElapsedTimer stageTimer;
            stageTimer.start();
            state->level2Provider_->Refresh();

            const std::string key = state->level2Provider_->FindLatestKey();
            if (key.empty())
            {
               logger_->warn("No Level 2 data available for {}", state->radarSite_);
               state->liveLoadInProgress_ = false;
               PublishToGui(weak,
                            [](RadarSiteDataService& self)
                            { Q_EMIT self.LoadFailed(QStringLiteral("No data available")); });
               return;
            }

            // Nothing new since the last publish, and no consumer is waiting on
            // a first frame: stop before the download *and* before making every
            // product rebuild the geometry it already has.
            if (!publishUnchanged)
            {
               std::lock_guard lock {state->latestKeyMutex_};
               if (key == state->lastPublishedLatestKey_)
               {
                  logger_->debug(
                     "Level 2 refresh for {}: latest volume unchanged ({}), "
                     "skipping reload",
                     state->radarSite_,
                     key);
                  state->liveLoadInProgress_ = false;
                  return;
               }
            }

            state->ThrowIfCancelled();

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            const auto load = state->level2Cache_.Load(
               key,
               [state, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
                     state->level2Provider_->LoadObjectByKey(key));
               },
               EstimateLevel2Bytes, state->level2Provider_->GetTimePointByKey(key));
            auto ar2vFile = load.value;
            logger_->info(
               "Level 2 load metrics: site={} key={} listing_ms={:.3f} "
               "download_decode_ms={:.3f} decoded_cache_hit={} origin={} "
               "cache_bytes={} cache_frames={} success={}",
               state->radarSite_,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               state->level2Cache_.size_bytes(),
               state->level2Cache_.count(),
               ar2vFile != nullptr);

            if (ar2vFile == nullptr)
            {
               // A shut-down provider returns null rather than throwing, so a release that
               // lands mid-download arrives here. That is a cancellation, not a failure the
               // user should be told about.
               state->liveLoadInProgress_ = false;
               if (state->Cancelled()) return;
               logger_->warn("Failed to load/parse Level 2 data for {}", state->radarSite_);
               PublishToGui(weak,
                            [](RadarSiteDataService& self)
                            { Q_EMIT self.LoadFailed(QStringLiteral("Failed to load data")); });
               return;
            }

            logger_->info("Loaded {} messages for {} ({} elevation scans)",
                          ar2vFile->message_count(),
                          state->radarSite_,
                          ar2vFile->radar_data().size());
            {
               std::lock_guard lock {state->latestKeyMutex_};
               state->lastPublishedLatestKey_ = key;
            }
            state->liveLoadInProgress_ = false;
            PublishToGui(weak,
                         [ar2vFile](RadarSiteDataService& self)
                         { Q_EMIT self.LevelTwoDataLoaded(ar2vFile); });
         }
         catch (const CancelledException&)
         {
            state->liveLoadInProgress_ = false;
         }
         catch (const std::exception& ex)
         {
            state->liveLoadInProgress_ = false;
            if (state->Cancelled()) return;
            logger_->error("Exception loading Level 2 data for {}: {}",
                           state->radarSite_,
                           ex.what());
            const QString reason = QString::fromStdString(ex.what());
            PublishToGui(weak,
                         [reason](RadarSiteDataService& self)
                         { Q_EMIT self.LoadFailed(reason); });
         }
      });
}

std::uint64_t RadarSiteDataService::LoadLevel2DataAt(
   std::chrono::system_clock::time_point time)
{
   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;
   const std::uint64_t requestId = state->nextRequestId_.fetch_add(1);
   logger_->info("Requesting archived Level 2 data for {} (request {})",
                 state->radarSite_,
                 requestId);

   // A timeline seek is a user waiting. Foreground, always.
   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Foreground,
      state->ListSerialKey(),
      [weak, state, requestId, time]()
      {
         const auto fail = [&weak, requestId](const QString& reason)
         {
            PublishToGui(weak,
                         [requestId, reason](RadarSiteDataService& self)
                         { Q_EMIT self.RequestFailed(requestId, reason); });
         };

         try
         {
            state->ThrowIfCancelled();

            QElapsedTimer stageTimer;
            stageTimer.start();
            const auto [success, newObjects, totalObjects] =
               state->level2Provider_->IsDateCached(time)
                  ? std::make_tuple(true, std::size_t {0}, state->level2Provider_->cache_size())
                  : state->level2Provider_->ListObjects(time);
            if (!success)
            {
               if (!state->Cancelled()) fail(QStringLiteral("Archive listing failed"));
               return;
            }

            const std::string key = state->level2Provider_->FindKey(time);
            if (key.empty())
            {
               if (!state->Cancelled())
                  fail(QStringLiteral("No volume available at that time"));
               return;
            }

            state->ThrowIfCancelled();

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            const auto load = state->level2Cache_.Load(
               key,
               [state, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Ar2vFile>(
                     state->level2Provider_->LoadObjectByKey(key));
               },
               EstimateLevel2Bytes, state->level2Provider_->GetTimePointByKey(key));
            auto file = load.value;
            logger_->info(
               "Level 2 archive metrics: site={} request={} key={} "
               "listing_ms={:.3f} download_decode_ms={:.3f} "
               "decoded_cache_hit={} origin={} cache_bytes={} cache_frames={} "
               "success={}",
               state->radarSite_,
               requestId,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               state->level2Cache_.size_bytes(),
               state->level2Cache_.count(),
               file != nullptr);
            if (file == nullptr)
            {
               if (!state->Cancelled())
                  fail(QStringLiteral("Failed to load archived volume"));
               return;
            }

            const auto actualTime = state->level2Provider_->GetTimePointByKey(key);
            logger_->info(
               "Loaded archived Level 2 data for {} (request {}, {} objects)",
               state->radarSite_,
               requestId,
               totalObjects);
            PublishToGui(weak,
                         [requestId, file, actualTime](RadarSiteDataService& self)
                         {
                            Q_EMIT self.LevelTwoDataLoadedForRequest(
                               requestId, file, actualTime);
                         });
         }
         catch (const CancelledException&)
         {
            // Released mid-request. The pane that asked has already moved on.
         }
         catch (const std::exception& ex)
         {
            if (state->Cancelled()) return;
            logger_->error("Archive request {} for {} failed: {}",
                           requestId,
                           state->radarSite_,
                           ex.what());
            fail(QString::fromStdString(ex.what()));
         }
      });
   return requestId;
}

void RadarSiteDataService::ResolveLevel2Time(
   std::chrono::system_clock::time_point                                     time,
   std::function<void(std::optional<std::chrono::system_clock::time_point>)> callback)
{
   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;

   // Listing only, and a pane is blocked on the answer before it can even decide whether a
   // download is needed - foreground.
   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Foreground,
      state->ListSerialKey(),
      [weak, state, time, callback = std::move(callback)]()
      {
         std::optional<std::chrono::system_clock::time_point> result;
         try
         {
            state->ThrowIfCancelled();
            const auto [success, newObjects, totalObjects] =
               state->level2Provider_->IsDateCached(time)
                  ? std::make_tuple(true, std::size_t {0}, state->level2Provider_->cache_size())
                  : state->level2Provider_->ListObjects(time);
            if (success)
            {
               const std::string key = state->level2Provider_->FindKey(time);
               if (!key.empty())
               {
                  result = state->level2Provider_->GetTimePointByKey(key);
               }
            }
         }
         catch (const CancelledException&)
         {
            return;
         }
         catch (const std::exception& ex)
         {
            if (state->Cancelled()) return;
            logger_->error(
               "Resolve Level 2 time for {} failed: {}", state->radarSite_, ex.what());
         }
         PublishToGui(weak,
                      [callback = std::move(callback), result](RadarSiteDataService&)
                      { callback(result); });
      });
}

void RadarSiteDataService::RefreshLevel3Catalog()
{
   if (!active()) return;
   if (p->task_->catalogLoadInProgress_.exchange(true))
      return;
   Q_EMIT LevelThreeCatalogLoading();
   logger_->info("Requesting Level 3 product catalog for {}", p->task_->radarSite_);

   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;

   // A dialog is about to show this, so it is foreground - but it lists, so it shares the
   // listing key.
   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Foreground,
      state->ListSerialKey(),
      [weak, state]()
      {
         try
         {
            state->ThrowIfCancelled();
            // Availability is site-wide; wxdata providers expose it through any
            // Level 3 instance. N0B is only the discovery transport, not an
            // assumed available product.
            auto provider = state->GetLevel3Provider("N0B");
            provider->RequestAvailableProducts();
            auto catalog = products::BuildLevel3ProductCatalog(
               provider->GetAvailableProducts());
            {
               std::lock_guard lock {state->level3Mutex_};
               state->level3Catalog_ = catalog;
            }
            state->catalogLoadInProgress_ = false;
            PublishToGui(weak,
                         [catalog = std::move(catalog)](RadarSiteDataService& self)
                         { Q_EMIT self.LevelThreeCatalogReady(catalog); });
         }
         catch (const CancelledException&)
         {
            state->catalogLoadInProgress_ = false;
         }
         catch (const std::exception& ex)
         {
            state->catalogLoadInProgress_ = false;
            if (state->Cancelled()) return;
            const QString reason = QString::fromStdString(ex.what());
            logger_->error("Level 3 catalog request for {} failed: {}",
                           state->radarSite_,
                           ex.what());
            PublishToGui(weak,
                         [reason](RadarSiteDataService& self)
                         { Q_EMIT self.LevelThreeCatalogFailed(reason); });
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
   const std::weak_ptr<RadarSiteDataService> weak  = weak_from_this();
   const auto                                state = p->task_;
   const std::uint64_t requestId = state->nextRequestId_.fetch_add(1);
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
                 state->radarSite_,
                 requestId);

   RadarTaskQueue::Instance().Post(
      RadarTaskQueue::Priority::Foreground,
      state->ListSerialKey(),
      [weak, state, requestId, awipsId, time, latest]()
      {
         const QString qAwipsId = QString::fromStdString(awipsId);
         const auto    fail     = [&weak, requestId, qAwipsId](const QString& reason)
         {
            PublishToGui(weak,
                         [requestId, qAwipsId, reason](RadarSiteDataService& self)
                         {
                            Q_EMIT self.LevelThreeRequestFailed(
                               requestId, qAwipsId, reason);
                         });
         };

         try
         {
            state->ThrowIfCancelled();

            QElapsedTimer stageTimer;
            stageTimer.start();
            auto provider = state->GetLevel3Provider(awipsId);
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
                  if (!state->Cancelled())
                     fail(QStringLiteral("Archive listing failed"));
                  return;
               }
            }

            const std::string key =
               latest ? provider->FindLatestKey() : provider->FindKey(time);
            if (key.empty())
            {
               if (!state->Cancelled()) fail(QStringLiteral("No product available"));
               return;
            }

            state->ThrowIfCancelled();

            const auto listingMs = stageTimer.nsecsElapsed() / 1.0e6;
            stageTimer.restart();
            // AWIPS id stays part of the key so two products cannot collide on
            // a shared object key, exactly as the previous ad-hoc map did.
            const std::string cacheKey = awipsId + '\n' + key;
            const auto        load     = state->level3Cache_.Load(
               cacheKey,
               [&provider, &key]()
               {
                  return std::dynamic_pointer_cast<scwx::wsr88d::Level3File>(
                     provider->LoadObjectByKey(key));
               },
               EstimateLevel3Bytes, provider->GetTimePointByKey(key));
            auto file = load.value;
            logger_->info(
               "Level 3 load metrics: site={} awips={} request={} key={} "
               "listing_ms={:.3f} cache_or_download_decode_ms={:.3f} "
               "decoded_cache_hit={} origin={} cache_bytes={} cache_frames={} "
               "success={}",
               state->radarSite_,
               awipsId,
               requestId,
               key,
               listingMs,
               stageTimer.nsecsElapsed() / 1.0e6,
               load.cache_hit(),
               OriginName(load.origin),
               state->level3Cache_.size_bytes(),
               state->level3Cache_.count(),
               file != nullptr);
            if (file == nullptr)
            {
               if (!state->Cancelled())
                  fail(QStringLiteral("Failed to load Level 3 product"));
               return;
            }

            const auto actualTime = provider->GetTimePointByKey(key);
            PublishToGui(weak,
                         [requestId, qAwipsId, file, actualTime](RadarSiteDataService& self)
                         {
                            Q_EMIT self.LevelThreeDataLoadedForRequest(
                               requestId, qAwipsId, file, actualTime);
                         });
         }
         catch (const CancelledException&)
         {
            // Released mid-request.
         }
         catch (const std::exception& ex)
         {
            if (state->Cancelled()) return;
            logger_->error("Level 3 request {} for {} {} failed: {}",
                           requestId,
                           state->radarSite_,
                           awipsId,
                           ex.what());
            fail(QString::fromStdString(ex.what()));
         }
      });
   return requestId;
}

std::vector<products::Level3ProductDescriptor>
RadarSiteDataService::level3_catalog() const
{
   std::lock_guard lock {p->task_->level3Mutex_};
   return p->task_->level3Catalog_;
}

} // namespace data
} // namespace wxlens
