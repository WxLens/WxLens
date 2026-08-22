#pragma once

#include <optional>
#include <string>

namespace nimbus
{
namespace data
{

struct RadarSiteInfo
{
   std::string id;
   std::string place;
   std::string state;
   std::string country;
   double      latitude;
   double      longitude;
   double      elevationMeters;
   std::string timeZoneId; // IANA time zone id, e.g. "America/Chicago"
};

/**
 * Looks up static radar site metadata (location, elevation, time zone) bundled with the app at
 * res/config/radar_sites.json - carried forward unmodified from Supercell Wx's compiled site
 * list (scwx-qt/res/config/radar_sites.json), not re-derived. Phase 1 slice 2 adds this for
 * station-local time display; it is also the natural home for the site lat/lon/elevation §4.7's
 * beam-height work will need later - reuse this lookup then, don't build a second one.
 */
std::optional<RadarSiteInfo> FindRadarSite(const std::string& siteId);

} // namespace data
} // namespace nimbus
