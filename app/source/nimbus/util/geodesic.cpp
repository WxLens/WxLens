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

} // namespace util
} // namespace nimbus
