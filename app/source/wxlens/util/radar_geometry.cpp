#include <wxlens/util/radar_geometry.hpp>
#include <wxlens/util/geodesic.hpp>

#include <cmath>
#include <numbers>

namespace wxlens
{
namespace util
{

double BeamAltitudeMsl(double rangeMeters,
                       double elevationAngleDegrees,
                       double siteAltitudeMslMeters)
{
   // Law of cosines on the triangle formed by the earth's centre, the antenna and the beam at
   // range, with the effective radius standing in for refraction. Identical in form to the legacy
   // GetRadarBeamAltititude, minus the units-library wrapping: plain metres and degrees keep this
   // usable from tests and from a future non-radar caller without dragging nholthaus/units in.
   const double height = siteAltitudeMslMeters + kEffectiveEarthRadiusMeters;
   const double elevationRadians =
      elevationAngleDegrees * std::numbers::pi / 180.0;

   const double altitudeSquared = rangeMeters * rangeMeters + height * height +
                                  2.0 * rangeMeters * height * std::sin(elevationRadians);

   return std::sqrt(altitudeSquared) - kEffectiveEarthRadiusMeters;
}

RadarBeamProbe ProbeRadarBeam(double                siteLatitude,
                              double                siteLongitude,
                              double                siteAltitudeMslMeters,
                              std::optional<double> elevationAngleDegrees,
                              double                targetLatitude,
                              double                targetLongitude)
{
   RadarBeamProbe probe {};
   probe.siteAltitudeMslMeters = siteAltitudeMslMeters;

   const GeodesicInverseResult inverse =
      GeodesicInverse(siteLatitude, siteLongitude, targetLatitude, targetLongitude);

   probe.rangeMeters = inverse.distanceMeters;

   // GeodesicInverse returns [-180, 180); an azimuth off a radar is read as a compass bearing, so
   // 315 rather than -45 - the same normalisation MeasurementController applies to bearings.
   probe.azimuthDegrees = inverse.azimuthDegrees;
   if (probe.azimuthDegrees < 0.0)
   {
      probe.azimuthDegrees += 360.0;
   }

   if (!elevationAngleDegrees.has_value())
   {
      return probe;
   }

   probe.elevationAngleKnown   = true;
   probe.elevationAngleDegrees = *elevationAngleDegrees;
   probe.beamCenterAltitudeMslMeters =
      BeamAltitudeMsl(probe.rangeMeters, *elevationAngleDegrees, siteAltitudeMslMeters);
   probe.beamCenterAboveRadarMeters =
      probe.beamCenterAltitudeMslMeters - siteAltitudeMslMeters;

   return probe;
}

} // namespace util
} // namespace wxlens
