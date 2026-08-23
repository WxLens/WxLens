#pragma once

#include <utility>

namespace nimbus
{
namespace util
{

/**
 * WGS84 geodesic direct problem: given a starting point, initial azimuth, and distance, find the
 * destination point. Used by RadarSweepProduct to place each radial/gate vertex at its true
 * geographic position (docs/ROADMAP.md §7 Phase 1 slice 3), matching the legacy app's
 * util::GeographicLib::DefaultGeodesic().Direct() usage in
 * scwx-qt/source/scwx/qt/view/level2_product_view.cpp. Deliberately just this one operation for
 * now, not a full port of that file's GeographicLib wrapper (Inverse/GetDistance/beam-height etc.
 * belong to later measurement/beam-height work - §4.4/§4.8 - and should reuse this same GeodesicDirect
 * pattern, not reinvent it).
 *
 * @param latitude Starting latitude, degrees
 * @param longitude Starting longitude, degrees
 * @param azimuthDegrees Initial azimuth, degrees clockwise from north
 * @param distanceMeters Distance to travel along the geodesic, meters
 *
 * @return {latitude, longitude} of the destination point, degrees
 */
std::pair<double, double> GeodesicDirect(double latitude,
                                         double longitude,
                                         double azimuthDegrees,
                                         double distanceMeters);

struct GeodesicInverseResult
{
   double distanceMeters {0.0};
   double azimuthDegrees {0.0}; ///< forward azimuth at point 1, degrees clockwise from north
};

/**
 * WGS84 geodesic inverse problem: distance and forward azimuth between two points. Real geodesic
 * math, never flat screen-pixel distance, so results stay correct at any zoom or projection
 * (docs/ROADMAP.md §4.4). Used for range rings today and the measurement tool next.
 */
GeodesicInverseResult GeodesicInverse(double latitude1,
                                      double longitude1,
                                      double latitude2,
                                      double longitude2);

} // namespace util
} // namespace nimbus
