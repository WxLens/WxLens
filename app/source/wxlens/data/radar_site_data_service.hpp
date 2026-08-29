#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>

#include <memory>
#include <chrono>
#include <cstdint>
#include <string>

#include <QObject>
#include <QString>

namespace wxlens
{
namespace data
{

/**
 * Phase 1 slice 2: deliberately minimal first version of the Data Source role for radar
 * (docs/ROADMAP.md §4.6). Ports RadarProductManager's *pattern* - per-site singleton, background
 * thread pool, Qt-signal completion back on the caller's thread - not its full API. No caching,
 * no multi-product/multi-elevation tracking, no refresh scheduling yet; those are follow-up slice
 * work as the pattern matures (starting with slice 3's full product coverage). Level 2 only,
 * latest volume only, for now.
 */
class RadarSiteDataService : public QObject
{
   Q_OBJECT

public:
   explicit RadarSiteDataService(const std::string& radarSite);
   ~RadarSiteDataService() override;

   RadarSiteDataService(const RadarSiteDataService&)            = delete;
   RadarSiteDataService& operator=(const RadarSiteDataService&) = delete;
   RadarSiteDataService(RadarSiteDataService&&)                 = delete;
   RadarSiteDataService& operator=(RadarSiteDataService&&)      = delete;

   [[nodiscard]] const std::string& radar_site() const;

   /**
    * Per-site singleton, matching RadarProductManager::Instance's pattern - multiple panes
    * showing the same site share one instance/fetch/cache once caching exists.
    */
   static std::shared_ptr<RadarSiteDataService> Instance(const std::string& radarSite);

   /**
    * Fetches the latest available Level 2 volume for this site on a background thread. Emits
    * LevelTwoDataLoaded or LoadFailed back on the calling (GUI) thread when done.
    */
   void LoadLatestLevel2Data();

   /// Loads the volume at or immediately before `time`. The provider first lists that UTC day,
   /// then uses wxdata's bounded-time lookup. The request id lets independently-timed panes share
   /// this service without consuming one another's result.
   std::uint64_t LoadLevel2DataAt(std::chrono::system_clock::time_point time);

signals:
   void LevelTwoDataLoaded(std::shared_ptr<scwx::wsr88d::Ar2vFile> file);
   void LevelTwoDataLoadedForRequest(
      std::uint64_t requestId,
      std::shared_ptr<scwx::wsr88d::Ar2vFile> file,
      std::chrono::system_clock::time_point actualTime);
   void RequestFailed(std::uint64_t requestId, QString reason);
   void LoadFailed(QString reason);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace data
} // namespace wxlens
