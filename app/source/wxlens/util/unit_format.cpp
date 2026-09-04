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
// Exact by definition: 1 kt = 1852 m/h, 1 mi = 1609.344 m.
constexpr double kMetersPerSecondToKnots            = 3600.0 / 1852.0;
constexpr double kMetersPerSecondToMilesPerHour     = 3600.0 / kMetersPerMile;
constexpr double kMetersPerSecondToKilometersPerHour = 3.6;

std::atomic<DistanceUnitPreference> distancePreference_ {DistanceUnitPreference::Both};
std::atomic<VelocityUnitPreference> velocityPreference_ {VelocityUnitPreference::MilesPerHour};

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

void SetVelocityUnitPreference(VelocityUnitPreference preference)
{
   velocityPreference_.store(preference, std::memory_order_relaxed);
}

VelocityUnitPreference GetVelocityUnitPreference()
{
   return velocityPreference_.load(std::memory_order_relaxed);
}

double ConvertVelocity(double metersPerSecond, VelocityUnitPreference preference)
{
   switch (preference)
   {
   case VelocityUnitPreference::Knots: return metersPerSecond * kMetersPerSecondToKnots;
   case VelocityUnitPreference::KilometersPerHour:
      return metersPerSecond * kMetersPerSecondToKilometersPerHour;
   case VelocityUnitPreference::MetersPerSecond: return metersPerSecond;
   case VelocityUnitPreference::MilesPerHour:
   default: return metersPerSecond * kMetersPerSecondToMilesPerHour;
   }
}

QString VelocityUnitLabel(VelocityUnitPreference preference)
{
   switch (preference)
   {
   case VelocityUnitPreference::Knots: return QStringLiteral("kt");
   case VelocityUnitPreference::KilometersPerHour: return QStringLiteral("km/h");
   case VelocityUnitPreference::MetersPerSecond: return QStringLiteral("m/s");
   case VelocityUnitPreference::MilesPerHour:
   default: return QStringLiteral("mph");
   }
}

QString FormatVelocity(double metersPerSecond)
{
   const VelocityUnitPreference preference = GetVelocityUnitPreference();
   const double                 value      = ConvertVelocity(metersPerSecond, preference);
   // Whole units: radial velocity is not measured finely enough for a decimal to mean anything,
   // and the sign carries the meteorology (negative is inbound, toward the radar).
   return QStringLiteral("%1 %2")
      .arg(static_cast<int>(std::llround(value)))
      .arg(VelocityUnitLabel(preference));
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
