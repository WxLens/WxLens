#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <wxlens/products/level3_product_catalog.hpp>

#include <memory>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <QObject>
#include <QList>
#include <QString>

namespace wxlens
{
namespace data
{

/**
 * Phase 1 slice 2: deliberately minimal first version of the Data Source role
 * for radar (docs/ROADMAP.md §4.6). Ports RadarProductManager's *pattern* -
 * per-site singleton, background thread pool and Qt-signal completion back on
 * the caller's thread. Level 2 volumes and Level 3 products share the per-site
 * service, while Level 3 providers and parsed files are separated by AWIPS
 * identity so panes cannot overwrite one another's product/time selection.
 */
class RadarSiteDataService :
    public QObject,
    public std::enable_shared_from_this<RadarSiteDataService>
{
   Q_OBJECT

public:
   /**
    * Only ever constructed through Instance(), which is what guarantees the shared_ptr
    * enable_shared_from_this needs. Public because std::make_shared requires it.
    */
   explicit RadarSiteDataService(const std::string& radarSite);

   /**
    * Deactivation point for one radar site (docs/ROADMAP.md performance checklist, slice 1).
    *
    * Runs when the last *consumer* releases its reference, which is always on the GUI thread:
    * background work holds a shared_ptr to the service's TaskState, never to this QObject, so
    * the reference count here is a true consumer count and its final decrement cannot happen on
    * a worker. That is what makes it safe to stop QTimers and destroy this QObject from right
    * here rather than through a custom deleter - and it is why no separate lease type is needed.
    *
    * Stops both timers, sets the per-instance cancellation flag every queued and running task
    * checks, drops this site's not-yet-started tasks from the shared queue, and shuts down the
    * Level 2 and Level 3 providers (which aborts an in-flight S3 transfer through wxdata's own
    * `running_`/SetContinueRequestHandler path). Shutdown is terminal: a later visit to this
    * site constructs a fresh service with fresh providers rather than reviving this one.
    */
   ~RadarSiteDataService() override;

   RadarSiteDataService(const RadarSiteDataService&)            = delete;
   RadarSiteDataService& operator=(const RadarSiteDataService&) = delete;
   RadarSiteDataService(RadarSiteDataService&&)                 = delete;
   RadarSiteDataService& operator=(RadarSiteDataService&&)      = delete;

   [[nodiscard]] const std::string& radar_site() const;

   /**
    * Per-site shared instance. Multiple panes showing the same site share one
    * instance/fetch/cache.
    *
    * The registry holds `weak_ptr`, not `shared_ptr`. Holding strong references made every site
    * ever visited immortal: its refresh timer kept polling, its decoded history stayed
    * allocated, and - because `PruneBefore` expires frames after the playback window - it
    * re-downloaded that whole window every 30 minutes, forever, competing for the same worker a
    * newly selected site needs. Measured on 2026-09-26: five abandoned sites retaining ~1.56 GB
    * of decoded Level 2 and 624 s of download time across 96 loads in a 90-minute session,
    * which is what turned a site switch into a ~37 s wait.
    *
    * The corollary is that **every consumer must hold the returned shared_ptr for as long as it
    * expects updates.** A caller that connects signals and drops the pointer will find the
    * service destroyed when it returns - `PaneController` acquires its own reference 23 lines
    * after `RebindProduct()` creates the sweep product, so the product cannot rely on it.
    */
   static std::shared_ptr<RadarSiteDataService>
   Instance(const std::string& radarSite);

   /// Number of live services, for the lifecycle tests. Expired registry entries are not
   /// counted; they are also cleaned out on the next Instance() call for any site.
   [[nodiscard]] static std::size_t InstanceCountForTesting();

   /// Whether this service still accepts work. False once the destructor has begun, which is
   /// only observable from a background task holding the shared TaskState.
   [[nodiscard]] bool active() const;

   /**
    * Fetches the latest available Level 2 volume for this site on a background
    * thread. Emits LevelTwoDataLoaded or LoadFailed back on the calling (GUI)
    * thread when done.
    */
   void LoadLatestLevel2Data();

   /// Loads the volume at or immediately before `time`. The provider first
   /// lists that UTC day, then uses wxdata's bounded-time lookup. The request
   /// id lets independently-timed panes share this service without consuming
   /// one another's result.
   std::uint64_t LoadLevel2DataAt(std::chrono::system_clock::time_point time);

   /**
    * Resolves, off the GUI thread, the volume actually available at or before `time` - a listing
    * call only, never the volume download LoadLevel2DataAt pays for. Lets a caller (RadarSweepProduct's
    * archive path) check a disk-backed product cache before deciding a fetch is even necessary.
    * `callback` runs on the GUI thread and receives nullopt if nothing is available at or before
    * `time` or the listing itself fails - the caller falls back to LoadLevel2DataAt either way.
    */
   void ResolveLevel2Time(
      std::chrono::system_clock::time_point time,
      std::function<void(std::optional<std::chrono::system_clock::time_point>)> callback);

   /// Discovers the Level 3 AWIPS IDs actually advertised for this site and
   /// publishes a canonical, categorized catalog. The provider request runs off
   /// the GUI thread.
   void RefreshLevel3Catalog();

   /// Loads the newest available instance of one Level 3 AWIPS product.
   std::uint64_t LoadLatestLevel3Data(const std::string& awipsId);

   /// Loads the Level 3 instance at or immediately before the selected UTC
   /// time.
   std::uint64_t LoadLevel3DataAt(const std::string&                    awipsId,
                                  std::chrono::system_clock::time_point time);

   [[nodiscard]] std::vector<products::Level3ProductDescriptor>
   level3_catalog() const;

   static void SetHistoryMinutes(int minutes);
   /// Stands down background history warming. Called once the application is quitting, because
   /// main() joins the io_context those downloads run on.
   static void CancelBackgroundWork();
   static int HistoryMinutes();
   void RequestRecentHistory();
   [[nodiscard]] QList<qint64> recentFrames() const;

signals:
   void RecentFramesChanged(QList<qint64> frames, QString error);
   void LevelTwoDataLoaded(std::shared_ptr<scwx::wsr88d::Ar2vFile> file);
   void LevelTwoDataLoadedForRequest(
      std::uint64_t                           requestId,
      std::shared_ptr<scwx::wsr88d::Ar2vFile> file,
      std::chrono::system_clock::time_point   actualTime);
   void RequestFailed(std::uint64_t requestId, QString reason);
   void LoadFailed(QString reason);
   void LevelThreeCatalogLoading();
   void LevelThreeCatalogReady(
      std::vector<wxlens::products::Level3ProductDescriptor> catalog);
   void LevelThreeCatalogFailed(QString reason);
   void LevelThreeRequestStarted(std::uint64_t requestId,
                                 QString       awipsId,
                                 qint64        selectedTimeMs);
   void LevelThreeDataLoadedForRequest(
      std::uint64_t                             requestId,
      QString                                   awipsId,
      std::shared_ptr<scwx::wsr88d::Level3File> file,
      std::chrono::system_clock::time_point     actualTime);
   void LevelThreeRequestFailed(std::uint64_t requestId,
                                QString       awipsId,
                                QString       reason);

private:
   /**
    * Shared implementation of the live path.
    *
    * `publishUnchanged` separates the two callers. An explicit
    * LoadLatestLevel2Data() comes from a consumer that has no data yet, so it
    * must publish even when the latest key has not moved (served from cache, so
    * still no download). The periodic refresh passes false: rediscovering the
    * same volume is not news, and republishing it would make every product
    * rebuild identical geometry on the GUI thread once a minute.
    */
   void LoadLatestLevel2DataInternal(bool publishUnchanged);

   /// Worker-visible state, held by shared_ptr so a background task can outlive this QObject
   /// without ever owning it. See its definition in the .cpp for why the split exists.
   struct TaskState;

   /// Queues one background task per discovered history frame. Static because it runs from a
   /// task that deliberately holds no reference to the service - only a weak one, locked on the
   /// GUI thread when there is something to publish.
   static void WarmHistoryFrames(
      const std::weak_ptr<RadarSiteDataService>& weak,
      const std::shared_ptr<TaskState>&          state,
      const QList<qint64>&                       frames,
      std::size_t                                capacityBytes,
      const QString&                             listingError);

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace data
} // namespace wxlens
