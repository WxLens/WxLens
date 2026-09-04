#pragma once

#include <QString>

namespace wxlens
{
namespace util
{

/**
 * Display formatting for the physical quantities the UI reports.
 *
 * One module rather than a helper on whichever class needed it first, because the same distance
 * has to read identically in a measurement readout, a beam-geometry breakdown and (later) a
 * point-info probe.
 */

/**
 * How distances and altitudes are spelled out. Slice 7 hardcoded `Both` on the reasoning that
 * showing km *and* miles beats silently picking one before a preference exists; §4.4 always
 * intended it to become a preference, and slice 17 made it one.
 */
enum class DistanceUnitPreference
{
   Both = 0, ///< metric and customary together
   Metric,
   Imperial
};

/**
 * How a velocity is spelled out in a readout.
 *
 * Unlike distance this never shows two units at once: a velocity readout sits next to a colour
 * whose palette is calibrated in one unit, and "42 kt / 48 mph" beside a single colour invites
 * reading the wrong number. WxLens's bundled velocity ramp is in mph, which is why that is the
 * default, but knots is the meteorological and aviation convention and the vendored WCT ramps use
 * it - so the two must not be hardcoded to each other.
 *
 * Level 2 velocity moments decode to m/s, which is why that is the input unit everywhere here.
 */
enum class VelocityUnitPreference
{
   MilesPerHour = 0, ///< matches the bundled DV ramp
   Knots,            ///< NWS/AWIPS and aviation convention
   KilometersPerHour,
   MetersPerSecond ///< what the Level 2 moment actually decodes to
};

/**
 * The active preferences. This is process-wide state pushed in by settings::AppSettings, rather
 * than this module reading settings itself - deliberately, so `util` keeps no dependency on the
 * config store and stays usable from tests that have no settings file. It mirrors the global
 * unit-settings pattern §4.4 points at in the legacy app.
 */
void                   SetDistanceUnitPreference(DistanceUnitPreference preference);
DistanceUnitPreference GetDistanceUnitPreference();
void                   SetVelocityUnitPreference(VelocityUnitPreference preference);
VelocityUnitPreference GetVelocityUnitPreference();

/// A velocity in metres per second converted to `preference`'s unit, unformatted - for callers
/// that need the number (a plot axis, a comparison) rather than the string.
[[nodiscard]] double ConvertVelocity(double metersPerSecond, VelocityUnitPreference preference);

/// The short label for a velocity unit ("mph", "kt", "km/h", "m/s").
[[nodiscard]] QString VelocityUnitLabel(VelocityUnitPreference preference);

/// Velocity: one unit per the active preference, signed, because inbound/outbound is the whole
/// point of a velocity readout.
QString FormatVelocity(double metersPerSecond);

/// Ground distance: kilometres and/or statute miles, per the active preference.
QString FormatGroundDistance(double meters);

/// Altitude: metres and/or feet. Distinct from ground distance because feet, not miles, is how a
/// beam altitude is read.
QString FormatAltitude(double meters);

/// Compass bearing, zero-padded to three digits (048°, not 48°) - how a bearing is written
/// everywhere else in meteorology. Not unit-dependent.
QString FormatBearing(double degrees);

} // namespace util
} // namespace wxlens
