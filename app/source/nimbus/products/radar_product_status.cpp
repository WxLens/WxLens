#include <nimbus/products/radar_product_status.hpp>
#include <nimbus/data/radar_site_data_service.hpp>
#include <nimbus/data/radar_site_database.hpp>
#include <nimbus/log/logger.hpp>

#include <scwx/util/time.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>

#include <chrono>

#if (__cpp_lib_chrono < 201907L)
#   include <date/tz.h>
#endif

namespace nimbus
{
namespace products
{

static const std::string logPrefix_ = "products.radar_product_status";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
// Matches the exact locate_zone pattern used throughout wxdata/the legacy app (e.g.
// config/radar_site.cpp) for the same __cpp_lib_chrono-gated std::chrono/date fallback. Returns
// nullptr (rather than throwing) if the zone name isn't in the system's tz database, so a bad or
// missing "tz" entry degrades to omitting that time rather than crashing.
const scwx::util::time_zone* TryLocateZone(const std::string& name)
{
   if (name.empty())
   {
      return nullptr;
   }

   try
   {
#if (__cpp_lib_chrono >= 201907L)
      return std::chrono::get_tzdb().locate_zone(name);
#else
      return date::get_tzdb().locate_zone(name);
#endif
   }
   catch (const std::exception& ex)
   {
      logger_->warn("Failed to locate time zone \"{}\": {}", name, ex.what());
      return nullptr;
   }
}

const scwx::util::time_zone* SystemLocalZone()
{
   try
   {
#if (__cpp_lib_chrono >= 201907L)
      return std::chrono::current_zone();
#else
      return date::current_zone();
#endif
   }
   catch (const std::exception& ex)
   {
      logger_->warn("Failed to determine system local time zone: {}", ex.what());
      return nullptr;
   }
}
} // namespace

class RadarProductStatus::Impl
{
public:
   explicit Impl(const std::string& radarSite) : radarSite_ {radarSite} {}

   std::string radarSite_;
   QString     statusText_ {QStringLiteral("Loading...")};
};

RadarProductStatus::RadarProductStatus(const std::string& radarSite, QObject* parent) :
    QObject(parent), p {std::make_unique<Impl>(radarSite)}
{
   auto service = nimbus::data::RadarSiteDataService::Instance(radarSite);

   connect(service.get(),
           &nimbus::data::RadarSiteDataService::LevelTwoDataLoaded,
           this,
           [this](std::shared_ptr<scwx::wsr88d::Ar2vFile> file)
           {
              const auto startTime = file->start_time();

              // Phase 1 slice 2: all three shown at once as an interim measure (real
              // docs/ROADMAP.md "map provider choice"-style unit/time-zone *preference*, with
              // persistence and a UI toggle, belongs to Phase 1's dedicated Settings work -
              // scwx::util::current_time_zone()/set_current_time_zone() is exactly the mechanism
              // a real settings toggle would drive; it's set at startup here, not exposed yet).
              // TimeString() already appends the zone abbreviation itself (e.g. "... UTC"), so
              // no separate "UTC" label is added here - only Station/Local get one below, since
              // their abbreviations (CDT, EDT, ...) alone don't make which-is-which obvious.
              QString timesText =
                 QString::fromStdString(scwx::util::TimeString(startTime));

              if (const auto siteInfo = nimbus::data::FindRadarSite(p->radarSite_);
                  siteInfo.has_value())
              {
                 if (const auto* stationZone = TryLocateZone(siteInfo->timeZoneId);
                     stationZone != nullptr)
                 {
                    timesText += QStringLiteral(" | Station %1").arg(QString::fromStdString(
                       scwx::util::TimeString(startTime,
                                              scwx::util::ClockFormat::_24Hour,
                                              stationZone)));
                 }
              }

              if (const auto* localZone = SystemLocalZone(); localZone != nullptr)
              {
                 timesText += QStringLiteral(" | Local %1").arg(QString::fromStdString(
                    scwx::util::TimeString(
                       startTime, scwx::util::ClockFormat::_24Hour, localZone)));
              }

              p->statusText_ = QStringLiteral("%1 elevation scans, %2 messages (volume "
                                               "start: %3)")
                                   .arg(file->radar_data().size())
                                   .arg(file->message_count())
                                   .arg(timesText);
              Q_EMIT statusTextChanged();
           });

   connect(service.get(),
           &nimbus::data::RadarSiteDataService::LoadFailed,
           this,
           [this](QString reason)
           {
              p->statusText_ = QStringLiteral("Load failed: %1").arg(reason);
              Q_EMIT statusTextChanged();
           });

   // Deliberately does not call LoadLatestLevel2Data(): RadarSweepProduct owns triggering the
   // load for its site now (docs/ROADMAP.md §7 Phase 1 slice 4). This is a passive status view of
   // whatever that site's service reports, so it stays correct whether or not a pane happens to
   // be showing this site.
}

RadarProductStatus::~RadarProductStatus() = default;

QString RadarProductStatus::siteId() const
{
   return QString::fromStdString(p->radarSite_);
}

QString RadarProductStatus::statusText() const
{
   return p->statusText_;
}

} // namespace products
} // namespace nimbus
