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
 * The active preference. This is process-wide state pushed in by settings::AppSettings, rather
 * than this module reading settings itself - deliberately, so `util` keeps no dependency on the
 * config store and stays usable from tests that have no settings file. It mirrors the global
 * unit-settings pattern §4.4 points at in the legacy app.
 */
void                   SetDistanceUnitPreference(DistanceUnitPreference preference);
DistanceUnitPreference GetDistanceUnitPreference();

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
