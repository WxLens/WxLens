#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <wxlens/products/level3_product_catalog.hpp>

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
 * Phase 1 slice 2: deliberately minimal first version of the Data Source role
 * for radar (docs/ROADMAP.md §4.6). Ports RadarProductManager's *pattern* -
 * per-site singleton, background thread pool and Qt-signal completion back on
 * the caller's thread. Level 2 volumes and Level 3 products share the per-site
 * service, while Level 3 providers and parsed files are separated by AWIPS
 * identity so panes cannot overwrite one another's product/time selection.
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
    * Per-site singleton, matching RadarProductManager::Instance's pattern -
    * multiple panes showing the same site share one instance/fetch/cache once
    * caching exists.
    */
   static std::shared_ptr<RadarSiteDataService>
   Instance(const std::string& radarSite);

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

signals:
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
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace data
} // namespace wxlens
