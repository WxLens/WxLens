#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/products/radar_sweep_product.hpp>
#include <nimbus/render/radar_sweep_layer.hpp>

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

void PaneController::setCenterLatitude(double value)
{
   if (p->centerLatitude_ == value)
   {
      return;
   }
   p->centerLatitude_ = value;
   Q_EMIT cameraChanged();
}

void PaneController::setCenterLongitude(double value)
{
   if (p->centerLongitude_ == value)
   {
      return;
   }
   p->centerLongitude_ = value;
   Q_EMIT cameraChanged();
}

void PaneController::setZoom(double value)
{
   if (p->zoom_ == value)
   {
      return;
   }
   p->zoom_ = value;
   Q_EMIT cameraChanged();
}

void PaneController::setBearing(double value)
{
   if (p->bearing_ == value)
   {
      return;
   }
   p->bearing_ = value;
   Q_EMIT cameraChanged();
}

void PaneController::setPitch(double value)
{
   if (p->pitch_ == value)
   {
      return;
   }
   p->pitch_ = value;
   Q_EMIT cameraChanged();
}

void PaneController::attachLayers(QMapLibre::Map* map)
{
   if (map == nullptr)
   {
      logger_->error("Pane {} attachLayers called with a null Map", p->paneId_);
      return;
   }

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
