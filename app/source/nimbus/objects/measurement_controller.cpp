#include <nimbus/objects/measurement_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/objects/map_object_store.hpp>
#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/util/geodesic.hpp>

#include <cmath>
#include <vector>

#include <QVariantMap>

namespace nimbus
{
namespace objects
{

static const std::string logPrefix_ = "objects.measurement_controller";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
constexpr double kMetersPerKilometer = 1000.0;
constexpr double kMetersPerMile      = 1609.344;

struct Point
{
   double latitude {0.0};
   double longitude {0.0};
};
} // namespace

class MeasurementController::Impl
{
public:
   [[nodiscard]] std::vector<Point> AllPoints() const
   {
      std::vector<Point> all = points_;
      if (hasCursor_)
      {
         all.push_back(cursor_);
      }
      return all;
   }

   /// PointToPoint is complete at two vertices; further clicks must not extend it into a path.
   [[nodiscard]] bool AcceptsMorePoints() const
   {
      switch (mode_)
      {
      case Mode::PointToPoint:
         return points_.size() < 2;
      case Mode::RadarToPoint:
         // One automatic origin plus one clicked point.
         return points_.size() < 2;
      case Mode::Path:
         return true;
      case Mode::None:
      default:
         return false;
      }
   }

   Mode               mode_ {Mode::None};
   std::vector<Point> points_ {};
   Point              cursor_ {};
   bool               hasCursor_ {false};
};

MeasurementController::MeasurementController(QObject* parent) :
    QObject(parent), p {std::make_unique<Impl>()}
{
}

MeasurementController::~MeasurementController() = default;

int MeasurementController::mode() const
{
   return static_cast<int>(p->mode_);
}

bool MeasurementController::active() const
{
   return p->mode_ != Mode::None && !p->points_.empty();
}

void MeasurementController::setMode(int mode)
{
   const auto requested = static_cast<Mode>(mode);
   if (p->mode_ == requested)
   {
      return;
   }

   // Switching tools abandons whatever was in progress rather than carrying half a measurement
   // into a different mode, where its vertices would mean something else.
   p->points_.clear();
   p->hasCursor_ = false;
   p->mode_      = requested;

   Q_EMIT modeChanged();
   Q_EMIT measurementChanged();
}

QVariantList MeasurementController::points() const
{
   QVariantList list;
   for (const Point& point : p->AllPoints())
   {
      list.append(point.latitude);
      list.append(point.longitude);
   }
   return list;
}

QVariantList MeasurementController::segments() const
{
   QVariantList list;

   const std::vector<Point> all = p->AllPoints();
   for (std::size_t i = 1; i < all.size(); ++i)
   {
      const auto result = util::GeodesicInverse(
         all[i - 1].latitude, all[i - 1].longitude, all[i].latitude, all[i].longitude);

      QVariantMap segment;
      segment[QStringLiteral("distanceMeters")] = result.distanceMeters;

      // GeodesicInverse returns [-180, 180); a compass bearing reads better as [0, 360).
      double bearing = result.azimuthDegrees;
      if (bearing < 0.0)
      {
         bearing += 360.0;
      }
      segment[QStringLiteral("bearingDegrees")] = bearing;

      list.append(segment);
   }

   return list;
}

double MeasurementController::totalMeters() const
{
   double total = 0.0;

   const std::vector<Point> all = p->AllPoints();
   for (std::size_t i = 1; i < all.size(); ++i)
   {
      total += util::GeodesicInverse(
                  all[i - 1].latitude, all[i - 1].longitude, all[i].latitude, all[i].longitude)
                  .distanceMeters;
   }

   return total;
}

QString MeasurementController::formatDistance(double meters)
{
   const double kilometers = meters / kMetersPerKilometer;
   const double miles      = meters / kMetersPerMile;

   // Both units until the unit-settings surface exists (§4.4 defers the preference itself, not
   // the measurement). Showing both is more useful than silently picking one.
   return QStringLiteral("%1 km / %2 mi")
      .arg(kilometers, 0, 'f', kilometers < 10.0 ? 2 : 1)
      .arg(miles, 0, 'f', miles < 10.0 ? 2 : 1);
}

QString MeasurementController::readout() const
{
   const QVariantList segmentList = segments();
   if (segmentList.isEmpty())
   {
      return {};
   }

   const QString total = formatDistance(totalMeters());

   if (p->mode_ == Mode::RadarToPoint)
   {
      // Range/azimuth is the radar-native way to say this, and is what the legacy app's
      // range/azimuth grid computed implicitly.
      const QVariantMap last = segmentList.last().toMap();
      return QStringLiteral("Range %1  Az %2°")
         .arg(total)
         .arg(last[QStringLiteral("bearingDegrees")].toDouble(), 0, 'f', 1);
   }

   if (segmentList.size() == 1)
   {
      const QVariantMap only = segmentList.first().toMap();
      return QStringLiteral("%1  Bearing %2°")
         .arg(total)
         .arg(only[QStringLiteral("bearingDegrees")].toDouble(), 0, 'f', 1);
   }

   return QStringLiteral("%1 total  (%2 segments)").arg(total).arg(segmentList.size());
}

void MeasurementController::addPoint(double                 latitude,
                                     double                 longitude,
                                     panes::PaneController* pane)
{
   if (p->mode_ == Mode::None)
   {
      return;
   }

   // RadarToPoint's origin is the pane's own source location, not a click - that is what makes it
   // a distinct named mode rather than PointToPoint with extra steps.
   if (p->mode_ == Mode::RadarToPoint && p->points_.empty())
   {
      if (pane == nullptr)
      {
         return;
      }
      p->points_.push_back({pane->homeLatitude(), pane->homeLongitude()});
   }

   if (!p->AcceptsMorePoints())
   {
      return;
   }

   p->points_.push_back({latitude, longitude});
   p->hasCursor_ = false;

   Q_EMIT measurementChanged();
}

void MeasurementController::updateCursor(double latitude, double longitude)
{
   if (p->mode_ == Mode::None || p->points_.empty() || !p->AcceptsMorePoints())
   {
      return;
   }

   p->cursor_    = {latitude, longitude};
   p->hasCursor_ = true;

   Q_EMIT measurementChanged();
}

void MeasurementController::undoPoint()
{
   if (p->points_.empty())
   {
      return;
   }

   p->points_.pop_back();

   // Removing the automatic origin as a separate step would be confusing, so RadarToPoint drops
   // it along with the clicked point.
   if (p->mode_ == Mode::RadarToPoint && p->points_.size() == 1)
   {
      p->points_.clear();
   }

   Q_EMIT measurementChanged();
}

void MeasurementController::cancel()
{
   if (p->points_.empty() && !p->hasCursor_)
   {
      return;
   }

   p->points_.clear();
   p->hasCursor_ = false;

   Q_EMIT measurementChanged();
}

int MeasurementController::commit(panes::PaneController* pane, int scopeKind)
{
   // The live cursor is not part of what gets pinned: it is where the pointer happens to be, not
   // something the user chose.
   if (p->points_.size() < 2)
   {
      return -1;
   }

   MapObject object;
   object.type      = MapObjectType::Measurement;
   object.color     = QStringLiteral("#7ee081");
   object.lifecycle = MapObjectLifecycle::Pinned;
   object.label     = readoutForPoints();

   for (const Point& point : p->points_)
   {
      object.latitudes.append(point.latitude);
      object.longitudes.append(point.longitude);
   }

   object.scope.kind = static_cast<MapObjectScopeKind>(scopeKind);
   if (pane != nullptr)
   {
      object.scope.originPaneId  = pane->paneId();
      object.scope.originGroupId = pane->syncGroup(object.scope.channel);
   }

   const int id = MapObjectStore::Instance().Add(object);
   if (id >= 0)
   {
      logger_->info("Pinned measurement {} with {} vertices", id, p->points_.size());
      p->points_.clear();
      p->hasCursor_ = false;
      Q_EMIT measurementChanged();
   }

   return id;
}

QString MeasurementController::readoutForPoints() const
{
   // The label is baked in at commit time from the committed vertices only, so a pinned
   // measurement keeps reading the same afterwards regardless of where the pointer goes.
   double total = 0.0;
   for (std::size_t i = 1; i < p->points_.size(); ++i)
   {
      total += util::GeodesicInverse(p->points_[i - 1].latitude,
                                     p->points_[i - 1].longitude,
                                     p->points_[i].latitude,
                                     p->points_[i].longitude)
                  .distanceMeters;
   }
   return formatDistance(total);
}

} // namespace objects
} // namespace nimbus
