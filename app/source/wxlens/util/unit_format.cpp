#include <wxlens/util/unit_format.hpp>

#include <atomic>
#include <cmath>

namespace wxlens
{
namespace util
{

namespace
{
constexpr double kMetersPerKilometer = 1000.0;
constexpr double kMetersPerMile      = 1609.344;
constexpr double kMetersPerFoot      = 0.3048;

// Atomic because formatting is called from the GUI thread while settings can be written from a
// QML callback on that same thread today - but the readout is also the kind of thing a future
// background producer will want to format, and a torn read of an enum is a silent wrong-units bug.
std::atomic<DistanceUnitPreference> distancePreference_ {DistanceUnitPreference::Both};

/// Two decimals below 10 units, one above: a 3.42 km measurement wants the precision, a 148 km one
/// does not, and a fixed width would waste digits on the far more common long distances.
int DecimalsFor(double value)
{
   return value < 10.0 ? 2 : 1;
}
} // namespace

void SetDistanceUnitPreference(DistanceUnitPreference preference)
{
   distancePreference_.store(preference, std::memory_order_relaxed);
}

DistanceUnitPreference GetDistanceUnitPreference()
{
   return distancePreference_.load(std::memory_order_relaxed);
}

QString FormatGroundDistance(double meters)
{
   const double kilometers = meters / kMetersPerKilometer;
   const double miles      = meters / kMetersPerMile;

   switch (GetDistanceUnitPreference())
   {
   case DistanceUnitPreference::Metric:
      return QStringLiteral("%1 km").arg(kilometers, 0, 'f', DecimalsFor(kilometers));

   case DistanceUnitPreference::Imperial:
      return QStringLiteral("%1 mi").arg(miles, 0, 'f', DecimalsFor(miles));

   case DistanceUnitPreference::Both:
   default:
      return QStringLiteral("%1 km / %2 mi")
         .arg(kilometers, 0, 'f', DecimalsFor(kilometers))
         .arg(miles, 0, 'f', DecimalsFor(miles));
   }
}

QString FormatAltitude(double meters)
{
   const double feet = meters / kMetersPerFoot;

   switch (GetDistanceUnitPreference())
   {
   case DistanceUnitPreference::Metric:
      return QStringLiteral("%1 m").arg(meters, 0, 'f', 0);

   case DistanceUnitPreference::Imperial:
      return QStringLiteral("%1 ft").arg(feet, 0, 'f', 0);

   case DistanceUnitPreference::Both:
   default:
      return QStringLiteral("%1 m / %2 ft").arg(meters, 0, 'f', 0).arg(feet, 0, 'f', 0);
   }
}

QString FormatBearing(double degrees)
{
   double normalized = std::fmod(degrees, 360.0);
   if (normalized < 0.0)
   {
      normalized += 360.0;
   }

   return QStringLiteral("%1°").arg(
      static_cast<int>(std::llround(normalized)) % 360, 3, 10, QLatin1Char('0'));
}

} // namespace util
} // namespace wxlens
