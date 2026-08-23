#include <nimbus/util/geodesic.hpp>

#include <GeographicLib/Geodesic.hpp>

namespace nimbus
{
namespace util
{

std::pair<double, double> GeodesicDirect(double latitude,
                                         double longitude,
                                         double azimuthDegrees,
                                         double distanceMeters)
{
   double destLatitude  = 0.0;
   double destLongitude = 0.0;

   GeographicLib::Geodesic::WGS84().Direct(
      latitude, longitude, azimuthDegrees, distanceMeters, destLatitude, destLongitude);

   return {destLatitude, destLongitude};
}

GeodesicInverseResult GeodesicInverse(double latitude1,
                                      double longitude1,
                                      double latitude2,
                                      double longitude2)
{
   GeodesicInverseResult result {};
   double                reverseAzimuth = 0.0;

   GeographicLib::Geodesic::WGS84().Inverse(latitude1,
                                            longitude1,
                                            latitude2,
                                            longitude2,
                                            result.distanceMeters,
                                            result.azimuthDegrees,
                                            reverseAzimuth);

   return result;
}

} // namespace util
} // namespace nimbus
