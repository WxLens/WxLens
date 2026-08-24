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

   /**
    * Site altitude above mean sea level, **metres**, converted at load time from the bundled
    * list's feet (see the note on the loader below). This is the `height` input to
    * util::BeamAltitudeMsl - the single number the whole beam-geometry readout rests on, which is
    * why it is named for its unit and reference surface rather than left as a bare `elevation`.
    */
   double      altitudeMslMeters;

   std::string timeZoneId; // IANA time zone id, e.g. "America/Chicago"
};

/**
 * Looks up static radar site metadata (location, altitude, time zone) bundled with the app at
 * res/config/radar_sites.json - carried forward unmodified from Supercell Wx's compiled site
 * list (scwx-qt/res/config/radar_sites.json), not re-derived. Phase 1 slice 2 added this for
 * station-local time display; slice 8's beam-geometry work (§4.7) is the second consumer, using
 * the same lookup rather than a second one.
 *
 * **The JSON's `elevation` field is in feet, and this converts it.** Nothing in the file says so;
 * it was verified two ways before slice 8 relied on it (§4.7 forbids guessing this input). The
 * legacy loader reads the same field as `units::length::feet` - scwx-qt/config/radar_site.cpp's
 * JSON branch - and the values only make sense as feet: PAEC (Nome, effectively at sea level)
 * reads 90, and KMSX (Point Six Mountain, ~2.4 km) reads 7978. Read as metres those are absurd.
 * An earlier field name here claimed metres and stored the raw value, which would have made every
 * beam altitude wrong by 2.3x the site's own height.
 *
 * **Known imprecision, deliberately not papered over:** the list does not record whether its
 * figure is ground level or the antenna on top of the tower, and a WSR-88D tower is tens of
 * metres. That offset is constant with range, so it shifts every beam altitude by the same small
 * amount rather than distorting the profile. The authoritative per-volume answer is in the Level 2
 * data itself - Message 31's volume data block carries site height (m MSL) and feedhorn height
 * (m AGL), which wxdata already parses but exposes no accessor for
 * (wsr88d/rda/digital_radar_data_generic.cpp). Adding those accessors is a wxdata change, so it
 * belongs upstream in the legacy repo per AGENTS.md, not here.
 */
std::optional<RadarSiteInfo> FindRadarSite(const std::string& siteId);

} // namespace data
} // namespace nimbus
