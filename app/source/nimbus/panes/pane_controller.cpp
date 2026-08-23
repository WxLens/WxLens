#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/products/radar_sweep_product.hpp>
#include <nimbus/render/radar_sweep_layer.hpp>

#include <map>

#include <QQmlEngine>

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
} // namespace

class PaneController::Impl
{
public:
   Impl(int paneId, products::ProductDescriptor descriptor) :
       paneId_ {paneId}, descriptor_ {std::move(descriptor)}
   {
   }

   void RebindProduct();

   int                         paneId_;
   products::ProductDescriptor descriptor_;

   std::map<SyncChannel, SyncGroupId> syncGroups_;

   std::shared_ptr<products::RadarSweepProduct> radarProduct_ {nullptr};

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
      radarProduct_ = products::RadarSweepProduct::Instance(descriptor_.sourceKey.toStdString());
      if (radarProduct_ == nullptr)
      {
         logger_->error("Pane {} could not bind radar source \"{}\"",
                        paneId_,
                        descriptor_.sourceKey.toStdString());
      }
   }
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

   // The one place that dispatches on product kind - see products::ProductDescriptor's comment.
   if (p->radarProduct_ == nullptr)
   {
      return;
   }

   logger_->info("Pane {} registering radar sweep layer for {}",
                 p->paneId_,
                 p->descriptor_.sourceKey.toStdString());

   map->addCustomLayer("nimbus-radar-sweep",
                       std::make_unique<render::RadarSweepLayer>(p->radarProduct_));

   // mbgl does not repaint just because a custom layer was registered or its data changed, and
   // radar data lands asynchronously well after the style loaded - see AGENTS.md's "Custom map
   // layers must trigger their own repaints".
   connect(p->radarProduct_.get(),
           &products::RadarSweepProduct::SweepUpdated,
           map,
           [map]() { map->triggerRepaint(); });

   if (p->radarProduct_->sweep_data() != nullptr)
   {
      // The data load beat the style load (a network race, not a guaranteed order), so
      // SweepUpdated already fired before the connection above existed.
      map->triggerRepaint();
   }
}

} // namespace panes
} // namespace nimbus
