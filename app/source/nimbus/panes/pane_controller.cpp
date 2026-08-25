#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/products/radar_sweep_product.hpp>
#include <nimbus/render/radar_sweep_layer.hpp>

#include <nimbus/util/geodesic.hpp>
#include <nimbus/util/radar_geometry.hpp>
#include <nimbus/util/unit_format.hpp>

#include <map>

#include <QPointer>
#include <QQmlEngine>
#include <QTimeZone>

namespace nimbus
{
namespace panes
{

static const std::string logPrefix_ = "panes.pane_controller";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
// CONUS center - the fallback when a pane's source has no coordinates of its own to centre on.
constexpr double kFallbackLatitude  = 39.8;
constexpr double kFallbackLongitude = -98.6;
constexpr double kDefaultZoom       = 6.0;

/// Said out loud rather than left blank wherever terrain would be needed. §4.7 is explicit that
/// the UI must state the absence: an omitted AGL field reads as "not interesting", while a
/// filled-in one implies a DEM that does not exist.
const QString kNoTerrainText = QStringLiteral("requires terrain data");
} // namespace

class PaneController::Impl
{
public:
   Impl(int paneId, products::ProductDescriptor descriptor) :
       paneId_ {paneId}, descriptor_ {std::move(descriptor)}
   {
   }

   void RebindProduct();
   void ConnectProductSignals(PaneController* self);
   void SetArchiveTime(PaneController* self,
                       std::optional<std::chrono::system_clock::time_point> time);

   int                         paneId_;
   products::ProductDescriptor descriptor_;

   std::map<SyncChannel, SyncGroupId> syncGroups_;

   std::shared_ptr<products::RadarSweepProduct> radarProduct_ {nullptr};
   std::shared_ptr<render::RadarSweepLayerBinding> radarLayerBinding_ {
      std::make_shared<render::RadarSweepLayerBinding>(nullptr)};
   QMetaObject::Connection repaintConnection_ {};
   QMetaObject::Connection sourceDataConnection_ {};
   QMetaObject::Connection loadStateConnection_ {};
   std::optional<std::chrono::system_clock::time_point> archiveTime_ {};
   std::chrono::system_clock::time_point actualTime_ {};
   bool timeLoading_ {false};
   QString timeError_ {};

   // Borrowed, not owned: MapQuickItem's unique_ptr owns this. Cleared when the map goes away so
   // the projection helpers cannot outlive it.
   QPointer<QMapLibre::Map> map_ {nullptr};

   double centerLatitude_ {kFallbackLatitude};
   double centerLongitude_ {kFallbackLongitude};
   double zoom_ {kDefaultZoom};
   double bearing_ {0.0};
   double pitch_ {0.0};
};

void PaneController::Impl::RebindProduct()
{
   radarProduct_.reset();

   if (descriptor_.kind == QStringLiteral("radar") && !descriptor_.sourceKey.isEmpty())
   {
      radarProduct_ = products::RadarSweepProduct::Instance(
         descriptor_.sourceKey.toStdString(), archiveTime_);
      if (radarProduct_ == nullptr)
      {
         logger_->error("Pane {} could not bind radar source \"{}\"",
                        paneId_,
                        descriptor_.sourceKey.toStdString());
      }
   }

   radarLayerBinding_->setProduct(radarProduct_);
}

void PaneController::Impl::ConnectProductSignals(PaneController* self)
{
   QObject::disconnect(repaintConnection_);
   QObject::disconnect(sourceDataConnection_);
   QObject::disconnect(loadStateConnection_);
   repaintConnection_    = {};
   sourceDataConnection_ = {};
   loadStateConnection_  = {};

   if (radarProduct_ == nullptr)
   {
      if (!map_.isNull())
      {
         map_->triggerRepaint();
      }
      return;
   }

   if (!map_.isNull())
   {
      repaintConnection_ = QObject::connect(radarProduct_.get(),
                                            &products::RadarSweepProduct::SweepUpdated,
                                            map_,
                                            [map = map_]()
                                            {
                                               if (!map.isNull())
                                               {
                                                  map->triggerRepaint();
                                               }
                                            });
      map_->triggerRepaint();
   }

   sourceDataConnection_ = QObject::connect(radarProduct_.get(),
                                             &products::RadarSweepProduct::SweepUpdated,
                                             self,
                                             [self]() { Q_EMIT self->sourceDataChanged(); });
   loadStateConnection_ = QObject::connect(
      radarProduct_.get(),
      &products::RadarSweepProduct::LoadStateChanged,
      self,
      [this, self](bool loading, const QString& error, qint64 actualTimeMs)
      {
         timeLoading_ = loading;
         timeError_   = error;
         if (actualTimeMs > 0)
         {
            actualTime_ = std::chrono::system_clock::time_point {
               std::chrono::milliseconds {actualTimeMs}};
         }
         Q_EMIT self->timeChanged();
      });
}

void PaneController::Impl::SetArchiveTime(
   PaneController* self,
   std::optional<std::chrono::system_clock::time_point> time)
{
   archiveTime_ = time;
   actualTime_  = {};
   timeError_.clear();
   timeLoading_ = time.has_value();
   RebindProduct();
   ConnectProductSignals(self);
   Q_EMIT self->timeChanged();
   Q_EMIT self->channelChanged(SyncChannel::Time, ChangeOrigin::UserInput);
}

PaneController::PaneController(int                                paneId,
                               const products::ProductDescriptor& descriptor,
                               QObject*                           parent) :
    QObject(parent), p {std::make_unique<Impl>(paneId, descriptor)}
{
   p->RebindProduct();

   p->centerLatitude_  = homeLatitude();
   p->centerLongitude_ = homeLongitude();
}

PaneController::~PaneController() = default;

int PaneController::paneId() const
{
   return p->paneId_;
}

QString PaneController::productKind() const
{
   return p->descriptor_.kind;
}

QString PaneController::sourceKey() const
{
   return p->descriptor_.sourceKey;
}

QString PaneController::productName() const
{
   return p->descriptor_.product;
}

void PaneController::setSourceKey(const QString& sourceKey)
{
   if (p->descriptor_.sourceKey == sourceKey)
   {
      return;
   }

   p->descriptor_.sourceKey = sourceKey;
   p->RebindProduct();
   p->ConnectProductSignals(this);

   Q_EMIT productChanged();
   Q_EMIT channelChanged(SyncChannel::RadarSite, ChangeOrigin::UserInput);
}

double PaneController::homeLatitude() const
{
   return (p->radarProduct_ != nullptr) ? p->radarProduct_->site_latitude() : kFallbackLatitude;
}

double PaneController::homeLongitude() const
{
   return (p->radarProduct_ != nullptr) ? p->radarProduct_->site_longitude() : kFallbackLongitude;
}

double PaneController::centerLatitude() const
{
   return p->centerLatitude_;
}

double PaneController::centerLongitude() const
{
   return p->centerLongitude_;
}

double PaneController::zoom() const
{
   return p->zoom_;
}

double PaneController::bearing() const
{
   return p->bearing_;
}

double PaneController::pitch() const
{
   return p->pitch_;
}

bool PaneController::liveMode() const { return !p->archiveTime_.has_value(); }

QString PaneController::selectedTimeText() const
{
   if (!p->archiveTime_.has_value()) return QStringLiteral("Live");
   const auto value = p->actualTime_ == std::chrono::system_clock::time_point {}
      ? *p->archiveTime_ : p->actualTime_;
   const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      value.time_since_epoch()).count();
   return QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm 'UTC'"));
}

bool PaneController::timeLoading() const { return p->timeLoading_; }
QString PaneController::timeError() const { return p->timeError_; }

void PaneController::selectLive() { p->SetArchiveTime(this, std::nullopt); }

bool PaneController::selectArchiveTime(const QString& isoUtc)
{
   const QString input = isoUtc.trimmed();
   const QDateTime fields =
      QDateTime::fromString(input, QStringLiteral("yyyy-MM-dd HH:mm"));
   QDateTime dateTime = fields.isValid() && input.size() == 16
      ? QDateTime(fields.date(), fields.time(), QTimeZone::UTC)
      : QDateTime::fromString(input, Qt::ISODate);
   if (!dateTime.isValid())
   {
      p->timeError_ = QStringLiteral("Use YYYY-MM-DD HH:MM or ISO 8601");
      Q_EMIT timeChanged();
      return false;
   }
   dateTime = dateTime.toUTC();
   if (dateTime > QDateTime::currentDateTimeUtc())
   {
      p->timeError_ = QStringLiteral("Archive time cannot be in the future");
      Q_EMIT timeChanged();
      return false;
   }
   p->SetArchiveTime(this, std::chrono::system_clock::time_point {
      std::chrono::milliseconds {dateTime.toMSecsSinceEpoch()}});
   return true;
}

void PaneController::setCenter(double latitude, double longitude)
{
   if (p->centerLatitude_ == latitude && p->centerLongitude_ == longitude)
   {
      return;
   }
   p->centerLatitude_  = latitude;
   p->centerLongitude_ = longitude;
   Q_EMIT cameraChanged();
   Q_EMIT channelChanged(SyncChannel::Location, ChangeOrigin::UserInput);
}

void PaneController::setZoom(double value)
{
   if (p->zoom_ == value)
   {
      return;
   }
   p->zoom_ = value;
   Q_EMIT cameraChanged();
   Q_EMIT channelChanged(SyncChannel::Zoom, ChangeOrigin::UserInput);
}

void PaneController::setBearing(double value)
{
   if (p->bearing_ == value)
   {
      return;
   }
   p->bearing_ = value;
   Q_EMIT cameraChanged();
   Q_EMIT channelChanged(SyncChannel::Bearing, ChangeOrigin::UserInput);
}

void PaneController::setPitch(double value)
{
   if (p->pitch_ == value)
   {
      return;
   }
   p->pitch_ = value;
   Q_EMIT cameraChanged();
   Q_EMIT channelChanged(SyncChannel::Pitch, ChangeOrigin::UserInput);
}

bool PaneController::hasMap() const
{
   return !p->map_.isNull();
}

QPointF PaneController::pixelForCoordinate(double latitude, double longitude) const
{
   if (p->map_.isNull())
   {
      return {-1.0, -1.0};
   }
   return p->map_->pixelForCoordinate({latitude, longitude});
}

QVariantList PaneController::coordinateForPixel(double x, double y) const
{
   if (p->map_.isNull())
   {
      return {};
   }
   const auto coordinate = p->map_->coordinateForPixel({x, y});
   return QVariantList {coordinate.first, coordinate.second};
}

double PaneController::distanceMeters(double latitude1,
                                      double longitude1,
                                      double latitude2,
                                      double longitude2) const
{
   return util::GeodesicInverse(latitude1, longitude1, latitude2, longitude2).distanceMeters;
}

QVariantList PaneController::coordinateAtOffset(double latitude,
                                                double longitude,
                                                double bearingDegrees,
                                                double distanceMeters) const
{
   const auto [destLatitude, destLongitude] =
      util::GeodesicDirect(latitude, longitude, bearingDegrees, distanceMeters);
   return QVariantList {destLatitude, destLongitude};
}

QVariantMap PaneController::probeSourceAt(double latitude, double longitude) const
{
   QVariantMap probe;
   probe[QStringLiteral("kind")]      = p->descriptor_.kind;
   probe[QStringLiteral("latitude")]  = latitude;
   probe[QStringLiteral("longitude")] = longitude;
   probe[QStringLiteral("available")] = false;

   // The one place that dispatches on product kind, mirroring attachLayers - see
   // products::ProductDescriptor's comment.
   if (p->descriptor_.kind != QStringLiteral("radar"))
   {
      probe[QStringLiteral("unavailableReason")] =
         QStringLiteral("no probe for %1 sources yet").arg(p->descriptor_.kind);
      return probe;
   }

   if (p->radarProduct_ == nullptr)
   {
      probe[QStringLiteral("unavailableReason")] = QStringLiteral("no radar source on this pane");
      return probe;
   }

   const auto elevationAngle = p->radarProduct_->elevation_angle_degrees();

   const util::RadarBeamProbe beam =
      util::ProbeRadarBeam(p->radarProduct_->site_latitude(),
                           p->radarProduct_->site_longitude(),
                           p->radarProduct_->site_altitude_msl_meters(),
                           elevationAngle,
                           latitude,
                           longitude);

   probe[QStringLiteral("available")] = true;
   probe[QStringLiteral("sourceKey")] = p->descriptor_.sourceKey;

   probe[QStringLiteral("siteLatitude")]  = p->radarProduct_->site_latitude();
   probe[QStringLiteral("siteLongitude")] = p->radarProduct_->site_longitude();
   probe[QStringLiteral("siteAltitudeMslMeters")] = beam.siteAltitudeMslMeters;
   probe[QStringLiteral("siteAltitudeText")] = util::FormatAltitude(beam.siteAltitudeMslMeters);

   probe[QStringLiteral("rangeMeters")]    = beam.rangeMeters;
   probe[QStringLiteral("rangeText")]      = util::FormatGroundDistance(beam.rangeMeters);
   probe[QStringLiteral("azimuthDegrees")] = beam.azimuthDegrees;
   probe[QStringLiteral("azimuthText")]    = util::FormatBearing(beam.azimuthDegrees);

   probe[QStringLiteral("elevationAngleKnown")] = beam.elevationAngleKnown;

   if (beam.elevationAngleKnown)
   {
      probe[QStringLiteral("elevationAngleDegrees")] = beam.elevationAngleDegrees;
      probe[QStringLiteral("elevationAngleText")] =
         QStringLiteral("%1°").arg(beam.elevationAngleDegrees, 0, 'f', 2);

      probe[QStringLiteral("beamCenterAltitudeMslMeters")] = beam.beamCenterAltitudeMslMeters;
      probe[QStringLiteral("beamCenterMslText")] =
         util::FormatAltitude(beam.beamCenterAltitudeMslMeters);

      probe[QStringLiteral("beamCenterAboveRadarMeters")] = beam.beamCenterAboveRadarMeters;
      probe[QStringLiteral("beamCenterArlText")] =
         util::FormatAltitude(beam.beamCenterAboveRadarMeters);
   }
   else
   {
      // No sweep has been computed yet, so there is no selected tilt to report and nothing
      // downstream of it means anything. Naming the reason beats an empty row.
      const QString pending = QStringLiteral("waiting for sweep data");
      probe[QStringLiteral("elevationAngleText")] = pending;
      probe[QStringLiteral("beamCenterMslText")]  = pending;
      probe[QStringLiteral("beamCenterArlText")]  = pending;
   }

   // Terrain, and therefore beam height above ground, until a DEM provider exists (§4.7, §6).
   // These two rows are shown, not hidden: a user reading a beam altitude needs to know it is
   // MSL and that the AGL figure they might expect is genuinely unavailable.
   probe[QStringLiteral("terrainAvailable")] = false;
   probe[QStringLiteral("terrainText")]      = QStringLiteral("terrain data unavailable");
   probe[QStringLiteral("beamCenterAglText")] = kNoTerrainText;

   return probe;
}

int PaneController::syncGroup(SyncChannel channel) const
{
   const auto it = p->syncGroups_.find(channel);
   return (it != p->syncGroups_.end()) ? it->second : kNoSyncGroup;
}

void PaneController::setSyncGroup(SyncChannel channel, int groupId)
{
   if (syncGroup(channel) == groupId)
   {
      return;
   }

   if (groupId == kNoSyncGroup)
   {
      p->syncGroups_.erase(channel);
   }
   else
   {
      p->syncGroups_[channel] = groupId;
   }

   Q_EMIT syncGroupsChanged();
}

QVariant PaneController::channelValue(SyncChannel channel) const
{
   switch (channel)
   {
   case SyncChannel::Location:
      // One channel, one value: a two-element list keeps latitude and longitude inseparable, so
      // a partially-applied coordinate can never be propagated.
      return QVariantList {p->centerLatitude_, p->centerLongitude_};
   case SyncChannel::Zoom:
      return p->zoom_;
   case SyncChannel::Bearing:
      return p->bearing_;
   case SyncChannel::Pitch:
      return p->pitch_;
   case SyncChannel::RadarSite:
      return p->descriptor_.sourceKey;
   case SyncChannel::Product:
      return p->descriptor_.product;

   case SyncChannel::Time:
      if (p->archiveTime_.has_value())
         return QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(
            std::chrono::duration_cast<std::chrono::milliseconds>(
               p->archiveTime_->time_since_epoch()).count(), Qt::UTC));
      return QDateTime {};
   case SyncChannel::Animation:
   case SyncChannel::Cursor:
   case SyncChannel::SelectedStorm:
   case SyncChannel::Palette:
   default:
      // Declared but not yet backed by state - see SyncChannel's comment. An invalid QVariant
      // makes the coordinator skip the channel rather than propagating a meaningless value.
      return {};
   }
}

void PaneController::applyChannelValue(SyncChannel     channel,
                                       const QVariant& value,
                                       ChangeOrigin    origin)
{
   if (!value.isValid())
   {
      return;
   }

   bool changed = false;

   switch (channel)
   {
   case SyncChannel::Location:
   {
      const QVariantList coordinate = value.toList();
      if (coordinate.size() != 2)
      {
         return;
      }
      const double latitude  = coordinate[0].toDouble();
      const double longitude = coordinate[1].toDouble();
      if (p->centerLatitude_ != latitude || p->centerLongitude_ != longitude)
      {
         p->centerLatitude_  = latitude;
         p->centerLongitude_ = longitude;
         changed             = true;
      }
      break;
   }

   case SyncChannel::Zoom:
      if (p->zoom_ != value.toDouble())
      {
         p->zoom_ = value.toDouble();
         changed  = true;
      }
      break;

   case SyncChannel::Bearing:
      if (p->bearing_ != value.toDouble())
      {
         p->bearing_ = value.toDouble();
         changed     = true;
      }
      break;

   case SyncChannel::Pitch:
      if (p->pitch_ != value.toDouble())
      {
         p->pitch_ = value.toDouble();
         changed   = true;
      }
      break;

   case SyncChannel::RadarSite:
      if (p->descriptor_.sourceKey != value.toString())
      {
         p->descriptor_.sourceKey = value.toString();
         p->RebindProduct();
         p->ConnectProductSignals(this);
         Q_EMIT productChanged();
         changed = true;
      }
      break;

   case SyncChannel::Product:
      if (p->descriptor_.product != value.toString())
      {
         p->descriptor_.product = value.toString();
         Q_EMIT productChanged();
         changed = true;
      }
      break;

   case SyncChannel::Time:
   {
      const QDateTime time = value.toDateTime();
      p->archiveTime_ = time.isValid()
         ? std::optional<std::chrono::system_clock::time_point> {
              std::chrono::system_clock::time_point {
                 std::chrono::milliseconds {time.toMSecsSinceEpoch()}}}
         : std::nullopt;
      p->actualTime_ = {};
      p->timeError_.clear();
      p->timeLoading_ = p->archiveTime_.has_value();
      p->RebindProduct();
      p->ConnectProductSignals(this);
      Q_EMIT timeChanged();
      changed = true;
      break;
   }

   default:
      return;
   }

   if (!changed)
   {
      return;
   }

   if (channel == SyncChannel::Location || channel == SyncChannel::Zoom ||
       channel == SyncChannel::Bearing || channel == SyncChannel::Pitch)
   {
      Q_EMIT cameraChanged();

      // Separate from cameraChanged so the view can distinguish "your own gesture moved this"
      // from "sync moved this" - only the latter should be pushed back into the map.
      Q_EMIT cameraSynced();
   }

   // Re-emitted with the incoming origin (not UserInput), which is what stops the coordinator
   // from fanning it straight back out - see ChangeOrigin.
   Q_EMIT channelChanged(channel, origin);
}

void PaneController::attachLayers(QMapLibre::Map* map)
{
   if (map == nullptr)
   {
      logger_->error("Pane {} attachLayers called with a null Map", p->paneId_);
      return;
   }

   // Qt gives JavaScript ownership to any QObject returned from a Q_INVOKABLE, so QML's garbage
   // collector considers itself responsible for the Map that MapQuickItem::mapLibreMap() handed
   // us - even though a std::unique_ptr inside MapQuickItem already owns it. At engine teardown
   // the collector deletes it, running mbgl::gl::Context::~Context with no GL context current,
   // which faults dereferencing QOpenGLContext::currentContext(). Claiming C++ ownership here
   // tells QML to keep its hands off. Must precede every early return below: the ownership
   // transfer already happened when QML evaluated mapLibreMap(), not when we use the result.
   QQmlEngine::setObjectOwnership(map, QQmlEngine::CppOwnership);

   // Kept for the projection helpers, which the QML object overlay needs on every frame.
   p->map_ = map;
   p->ConnectProductSignals(this);

   logger_->info("Pane {} registering replaceable radar sweep layer", p->paneId_);

   map->addCustomLayer("nimbus-radar-sweep",
                       std::make_unique<render::RadarSweepLayer>(p->radarLayerBinding_));

   if (p->radarProduct_ != nullptr && p->radarProduct_->sweep_data() != nullptr)
   {
      // The data load beat the style load (a network race, not a guaranteed order), so
      // SweepUpdated already fired before the connection above existed.
      map->triggerRepaint();
   }
}

} // namespace panes
} // namespace nimbus
