#pragma once

#include <optional>

namespace nimbus
{
namespace util
{

/**
 * Effective earth radius for radar beam propagation: 4/3 of the earth's real radius, the standard
 * meteorological approximation for how a beam bends downward in an atmosphere whose refractive
 * index decreases with height. Straightening the beam and inflating the earth instead is what
 * lets a single closed-form expression answer "how high is the beam here", and it is what
 * util::GeographicLib::GetRadarBeamAltititude in the legacy app uses (same constant, ported here
 * rather than re-derived - docs/ROADMAP.md §4.7's "reuse, don't rebuild").
 *
 * This is a *standard-atmosphere* model. It is not a ray trace, so it does not know about
 * ducting, superrefraction or a real temperature/moisture profile - a beam under an inversion can
 * sit well below what this returns. That limitation is inherent to the model, not to this port.
 */
inline constexpr double kEffectiveEarthRadiusMeters = 6367444.0 * 4.0 / 3.0;

/**
 * Altitude of the radar beam above mean sea level, at `rangeMeters` along the ground from a site
 * whose antenna sits `siteAltitudeMslMeters` above MSL, radiating at `elevationAngleDegrees`.
 *
 * **MSL, not AGL.** Subtracting the terrain height under the target point is what turns this into
 * a height above ground, and Nimbus has no terrain source yet (§4.7, §6) - callers must not
 * present the result as a height above ground. `beamCenterAboveRadarMeters` in RadarBeamProbe is
 * the one relative figure that *is* honest without terrain, because the radar's own altitude is
 * known.
 *
 * The elevation angle is a parameter rather than a baked-in centre line, so the later beam
 * top/bottom work (§4.7, §8 backlog) is two more calls at elevation ± half the beamwidth rather
 * than a rework: nothing here assumes the beam is an infinitely thin ray, it simply only ever
 * asks about one angle at a time.
 */
double BeamAltitudeMsl(double rangeMeters,
                       double elevationAngleDegrees,
                       double siteAltitudeMslMeters);

/**
 * Everything §4.7 asks a Radar -> Point interrogation to answer, for one target coordinate.
 *
 * The distinctions §4.7 insists on are structural here, not left to whoever writes the UI:
 * elevation *angle*, beam-centre *altitude* (MSL) and beam-centre height *above the radar* (ARL)
 * are three separate fields, and beam-centre height above *ground* (AGL) is deliberately absent
 * because computing it needs terrain data that does not exist yet. A field that is missing cannot
 * be displayed as though it were authoritative.
 */
struct RadarBeamProbe
{
   /// Ground distance from the site to the target, along a true WGS84 geodesic.
   double rangeMeters {0.0};

   /// Compass bearing from the site to the target, [0, 360).
   double azimuthDegrees {0.0};

   /// Antenna altitude above MSL - the `height` input to the beam model.
   double siteAltitudeMslMeters {0.0};

   /**
    * False when the pane has no sweep loaded yet, in which case there is no selected tilt to
    * report and every beam figure below is meaningless. Guessing 0.5° here would produce a
    * confident number for an angle the radar may not be using (§4.7 forbids exactly that).
    */
   bool   elevationAngleKnown {false};
   double elevationAngleDegrees {0.0};

   /// Only meaningful when elevationAngleKnown.
   double beamCenterAltitudeMslMeters {0.0};

   /// Beam centre relative to the antenna (ARL). Honest without terrain; AGL is not.
   double beamCenterAboveRadarMeters {0.0};
};

/**
 * Interrogates the beam geometry between a radar site and a target coordinate. Pure math over
 * real WGS84 geodesics (§4.4's "never flat screen-pixel math" applies here too), with no Qt and
 * no I/O, so it is testable on its own and reusable by §4.4's point-info tool as well as by the
 * measurement readout.
 *
 * `elevationAngleDegrees` is optional rather than defaulted: "the sweep has not loaded yet" is a
 * real state the UI must say out loud, not a hole to plug with a plausible tilt.
 */
RadarBeamProbe ProbeRadarBeam(double                siteLatitude,
                              double                siteLongitude,
                              double                siteAltitudeMslMeters,
                              std::optional<double> elevationAngleDegrees,
                              double                targetLatitude,
                              double                targetLongitude);

} // namespace util
} // namespace nimbus
