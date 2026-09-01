#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/products/radar_sweep_product.hpp>
#include <wxlens/products/level3_graphic_overlay.hpp>
#include <wxlens/products/level3_radial_product.hpp>
#include <wxlens/products/level3_raster_product.hpp>
#include <wxlens/products/level3_text_product.hpp>
#include <wxlens/data/radar_site_data_service.hpp>
#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/palettes/palette_defaults.hpp>
#include <wxlens/render/radar_sweep_layer.hpp>

#include <wxlens/util/geodesic.hpp>
#include <wxlens/util/radar_geometry.hpp>
#include <wxlens/util/unit_format.hpp>

#include <map>
#include <array>
#include <ranges>

#include <QPointer>
#include <QQmlEngine>
#include <QTimeZone>

#include <scwx/common/products.hpp>

namespace wxlens
{
namespace panes
{

static const std::string logPrefix_ = "panes.pane_controller";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

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

QString DefaultPalette(const products::ProductDescriptor& descriptor)
{
   if (descriptor.identityKind != products::ProductDescriptor::IdentityKind::Level2Moment)
      return {};
   return palettes::BundledPaletteName(QString::fromStdString(scwx::common::GetLevel2Palette(
      scwx::common::GetLevel2Product(descriptor.identity.toStdString()))));
}
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
   void ApplyPalette();
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
   QMetaObject::Connection level3LoadedConnection_ {};
   QMetaObject::Connection level3FailedConnection_ {};
   std::uint64_t level3RequestId_ {0};
   QVariantList productOverlays_ {};
   QString productDetailsText_ {};
   QString defaultPalette_ {};
   std::optional<std::chrono::system_clock::time_point> archiveTime_ {};
   std::chrono::system_clock::time_point actualTime_ {};
   bool timeLoading_ {false};
   QString timeError_ {};
   QString selectedStorm_ {};
   std::shared_ptr<data::RadarSiteDataService> dataService_ {nullptr};
   std::vector<products::Level3ProductDescriptor> level3Catalog_ {};
   bool productCatalogLoading_ {false};
   QString productCatalogError_ {};
   QMetaObject::Connection catalogReadyConnection_ {};
   QMetaObject::Connection catalogLoadingConnection_ {};
   QMetaObject::Connection catalogFailedConnection_ {};

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
   radarLayerBinding_->setProduct(nullptr);
   productOverlays_.clear();
   productDetailsText_.clear();
   defaultPalette_.clear();
   defaultPalette_ = DefaultPalette(descriptor_);

   if (descriptor_.kind == QStringLiteral("radar") && !descriptor_.sourceKey.isEmpty())
   {
      if (descriptor_.identityKind == products::ProductDescriptor::IdentityKind::Level2Moment)
      {
         radarProduct_ = products::RadarSweepProduct::Instance(
            descriptor_.sourceKey.toStdString(), descriptor_.product.toStdString(),
            descriptor_.elevation, archiveTime_);
      }
      if (radarProduct_ == nullptr)
      {
         logger_->error("Pane {} could not bind radar source \"{}\"",
                        paneId_,
                        descriptor_.sourceKey.toStdString());
      }
   }

   radarLayerBinding_->setProduct(radarProduct_);
}

void PaneController::Impl::ApplyPalette()
{
   if (descriptor_.palette.isEmpty())
   {
      if (radarProduct_ != nullptr) radarLayerBinding_->setProduct(radarProduct_);
      if (radarProduct_ != nullptr) return;
   }
   products::SweepSnapshot snapshot = radarProduct_ != nullptr
      ? radarProduct_->sweep_snapshot() : radarLayerBinding_->snapshot();
   if (snapshot.sweep == nullptr) return;
   const QString palette = descriptor_.palette.isEmpty() ? defaultPalette_ : descriptor_.palette;
   const QString text = palettes::PaletteManager::Instance().paletteText(palette);
   if (text.isEmpty()) return;
   snapshot.colorTableLut = products::BuildColorTableLut(*snapshot.sweep, text);
   radarLayerBinding_->setSnapshot(std::move(snapshot));
}

void PaneController::Impl::ConnectProductSignals(PaneController* self)
{
   QObject::disconnect(repaintConnection_);
   QObject::disconnect(sourceDataConnection_);
   QObject::disconnect(loadStateConnection_);
   QObject::disconnect(level3LoadedConnection_);
   QObject::disconnect(level3FailedConnection_);
   repaintConnection_    = {};
   sourceDataConnection_ = {};
   loadStateConnection_  = {};
   level3LoadedConnection_ = {};
   level3FailedConnection_ = {};

   if (descriptor_.identityKind == products::ProductDescriptor::IdentityKind::Level3Awips &&
       !descriptor_.sourceKey.isEmpty())
   {
      dataService_ = data::RadarSiteDataService::Instance(descriptor_.sourceKey.toStdString());
      level3LoadedConnection_ = QObject::connect(
         dataService_.get(), &data::RadarSiteDataService::LevelThreeDataLoadedForRequest, self,
         [this, self](std::uint64_t requestId, const QString& awipsId,
                      std::shared_ptr<scwx::wsr88d::Level3File> file,
                      std::chrono::system_clock::time_point actualTime)
         {
            if (requestId != level3RequestId_ || awipsId != descriptor_.identity || file == nullptr)
               return;

            products::SweepSnapshot renderSnapshot;
            QString defaultPalette;
            if (auto radial = products::BuildLevel3RadialSnapshot(*file); radial.has_value())
            {
               renderSnapshot.sweep = radial->sweep;
               defaultPalette = QString::fromStdString(radial->metadata.defaultPalette);
            }
            else if (auto raster = products::BuildLevel3RasterSnapshot(*file); raster.has_value())
            {
               renderSnapshot.sweep = raster->sweep;
               defaultPalette = QString::fromStdString(raster->metadata.defaultPalette);
            }
            if (renderSnapshot.sweep != nullptr)
            {
               auto& manager = palettes::PaletteManager::Instance();
               defaultPalette_ = palettes::BundledPaletteName(defaultPalette);
               QString palette = descriptor_.palette.isEmpty() ? defaultPalette_ : descriptor_.palette;
               QString text = manager.paletteText(palette);
               if (text.isEmpty())
                  logger_->warn("No palette {} for Level 3 product {}",
                                palette.toStdString(), descriptor_.identity.toStdString());
               renderSnapshot.colorTableLut =
                  products::BuildColorTableLut(*renderSnapshot.sweep, text);
            }
            radarLayerBinding_->setSnapshot(std::move(renderSnapshot));

            productOverlays_.clear();
            if (auto overlay = products::BuildLevel3GraphicOverlaySnapshot(*file); overlay.has_value())
            {
               for (const auto& primitive : overlay->primitives)
               {
                  QVariantList points;
                  for (const auto& point : primitive.geometry)
                     points.append(QVariantMap {{QStringLiteral("latitude"), point.latitude},
                                                {QStringLiteral("longitude"), point.longitude}});
                  productOverlays_.append(QVariantMap {
                     {QStringLiteral("kind"), static_cast<int>(primitive.kind)},
                     {QStringLiteral("points"), points},
                     {QStringLiteral("stormId"), QString::fromStdString(primitive.stormId)},
                     {QStringLiteral("label"), QString::fromStdString(primitive.label)},
                     {QStringLiteral("radiusMeters"), primitive.radiusMeters},
                     {QStringLiteral("forecast"), primitive.forecast},
                     {QStringLiteral("past"), primitive.past}});
               }
            }

            productDetailsText_.clear();
            if (auto text = products::BuildLevel3TextSnapshot(*file); text.has_value())
            {
               auto appendPages = [this](const auto& pages)
               {
                  for (const auto& page : pages)
                     for (const auto& entry : page.entries)
                     {
                        if (!productDetailsText_.isEmpty()) productDetailsText_ += QLatin1Char('\n');
                        productDetailsText_ += QString::fromStdString(entry.text);
                     }
               };
               appendPages(text->graphicPages);
               appendPages(text->tabularPages);
            }
            actualTime_ = actualTime;
            timeLoading_ = false;
            timeError_.clear();
            if (!map_.isNull()) map_->triggerRepaint();
            Q_EMIT self->sourceDataChanged();
            Q_EMIT self->productDetailsChanged();
            Q_EMIT self->timeChanged();
         });
      level3FailedConnection_ = QObject::connect(
         dataService_.get(), &data::RadarSiteDataService::LevelThreeRequestFailed, self,
         [this, self](std::uint64_t requestId, const QString&, const QString& error)
         {
            if (requestId != level3RequestId_) return;
            timeLoading_ = false;
            timeError_ = error;
            Q_EMIT self->timeChanged();
         });
      timeLoading_ = true;
      timeError_.clear();
      level3RequestId_ = archiveTime_.has_value()
         ? dataService_->LoadLevel3DataAt(descriptor_.identity.toStdString(), *archiveTime_)
         : dataService_->LoadLatestLevel3Data(descriptor_.identity.toStdString());
      Q_EMIT self->timeChanged();
      Q_EMIT self->productDetailsChanged();
      return;
   }

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
                                            [this, map = map_]()
                                            {
                                               ApplyPalette();
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

   auto& paletteManager = palettes::PaletteManager::Instance();
   connect(&paletteManager, &palettes::PaletteManager::paletteTextChanged, this,
           [this, &paletteManager](const QString&)
           {
              if (effectivePaletteName() != paletteManager.activeName()) return;
              p->ApplyPalette();
              if (!p->map_.isNull()) p->map_->triggerRepaint();
           });

   if (!p->descriptor_.sourceKey.isEmpty()) refreshProductCatalog();

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

QStringList PaneController::availableProducts() const
{
   return {QStringLiteral("Reflectivity"), QStringLiteral("Velocity"),
           QStringLiteral("Spectrum Width"), QStringLiteral("Differential Reflectivity"),
           QStringLiteral("Differential Phase"), QStringLiteral("Correlation Coefficient"),
           QStringLiteral("Clutter Filter Power Removed")};
}

QVariantList PaneController::productCatalog() const
{
   QVariantList catalog;
   const std::array<std::pair<const char*, const char*>, 7> level2 {{
      {"REF", "Reflectivity"}, {"VEL", "Velocity"}, {"SW", "Spectrum Width"},
      {"ZDR", "Differential Reflectivity"}, {"PHI", "Differential Phase"},
      {"RHO", "Correlation Coefficient"}, {"CFP", "Clutter Filter Power Removed"}}};
   for (const auto& [identity, name] : level2)
   {
      catalog.append(QVariantMap {{QStringLiteral("category"), QStringLiteral("Level 2 moments")},
                                  {QStringLiteral("description"), QString::fromLatin1(name)},
                                  {QStringLiteral("identityKind"), QStringLiteral("level2")},
                                  {QStringLiteral("identity"), QString::fromLatin1(identity)},
                                  {QStringLiteral("awipsId"), QString {}},
                                  {QStringLiteral("available"), true}});
   }
   for (const auto& item : p->level3Catalog_)
   {
      catalog.append(QVariantMap {{QStringLiteral("category"), item.categoryDescription},
                                  {QStringLiteral("description"), item.description},
                                  {QStringLiteral("identityKind"), QStringLiteral("level3")},
                                  {QStringLiteral("identity"), item.awipsId},
                                  {QStringLiteral("awipsId"), item.awipsId},
                                  {QStringLiteral("available"), true}});
   }
   return catalog;
}

bool PaneController::productCatalogLoading() const { return p->productCatalogLoading_; }
QString PaneController::productCatalogError() const { return p->productCatalogError_; }
QString PaneController::productIdentity() const { return p->descriptor_.identity; }
bool PaneController::level3Product() const
{
   return p->descriptor_.identityKind == products::ProductDescriptor::IdentityKind::Level3Awips;
}
QString PaneController::paletteName() const { return p->descriptor_.palette; }
QString PaneController::effectivePaletteName() const
{
   return p->descriptor_.palette.isEmpty() ? p->defaultPalette_ : p->descriptor_.palette;
}
QVariantList PaneController::productOverlays() const { return p->productOverlays_; }
QString PaneController::productDetailsText() const { return p->productDetailsText_; }

void PaneController::setPaletteName(const QString& name)
{
   if (p->descriptor_.palette == name) return;
   p->descriptor_.palette = name;
   p->ApplyPalette();
   if (!p->map_.isNull()) p->map_->triggerRepaint();
   Q_EMIT paletteChanged();
   Q_EMIT channelChanged(SyncChannel::Palette, ChangeOrigin::UserInput);
}

void PaneController::refreshProductCatalog()
{
   QObject::disconnect(p->catalogReadyConnection_);
   QObject::disconnect(p->catalogLoadingConnection_);
   QObject::disconnect(p->catalogFailedConnection_);
   p->level3Catalog_.clear();
   p->productCatalogError_.clear();
   p->dataService_ = data::RadarSiteDataService::Instance(p->descriptor_.sourceKey.toStdString());
   if (p->dataService_ == nullptr) return;
   p->catalogLoadingConnection_ = connect(p->dataService_.get(),
      &data::RadarSiteDataService::LevelThreeCatalogLoading, this, [this]() {
         p->productCatalogLoading_ = true; p->productCatalogError_.clear();
         Q_EMIT productCatalogChanged();
      });
   p->catalogReadyConnection_ = connect(p->dataService_.get(),
      &data::RadarSiteDataService::LevelThreeCatalogReady, this,
      [this](std::vector<products::Level3ProductDescriptor> catalog) {
         p->level3Catalog_ = std::move(catalog); p->productCatalogLoading_ = false;
         Q_EMIT productCatalogChanged();
      });
   p->catalogFailedConnection_ = connect(p->dataService_.get(),
      &data::RadarSiteDataService::LevelThreeCatalogFailed, this, [this](const QString& error) {
         p->productCatalogLoading_ = false; p->productCatalogError_ = error;
         Q_EMIT productCatalogChanged();
      });
   p->dataService_->RefreshLevel3Catalog();
}

bool PaneController::selectProduct(const QString& kind, const QString& identity, const QString& name)
{
   const bool isLevel3 = kind == QStringLiteral("level3");
   if (identity.isEmpty() || name.isEmpty()) return false;
   if (!isLevel3 && !availableProducts().contains(name)) return false;
   if (isLevel3)
   {
      const bool found = std::ranges::any_of(p->level3Catalog_, [&](const auto& value) {
         return value.awipsId == identity;
      });
      if (!found) return false;
   }
   p->descriptor_.identityKind = isLevel3
      ? products::ProductDescriptor::IdentityKind::Level3Awips
      : products::ProductDescriptor::IdentityKind::Level2Moment;
   p->descriptor_.identity = identity;
   p->descriptor_.product = name;
   p->descriptor_.elevation = 0.0f;
   p->descriptor_.palette.clear();
   p->RebindProduct(); p->ConnectProductSignals(this);
   Q_EMIT productChanged(); Q_EMIT paletteChanged();
   Q_EMIT channelChanged(SyncChannel::Product, ChangeOrigin::UserInput);
   return true;
}

QVariantList PaneController::elevationCuts() const
{
   QVariantList result;
   if (p->radarProduct_ != nullptr)
      for (float cut : p->radarProduct_->elevation_cuts()) result.append(cut);
   return result;
}

double PaneController::selectedElevation() const { return p->descriptor_.elevation; }

void PaneController::setProductName(const QString& name)
{
   if (!availableProducts().contains(name) || p->descriptor_.product == name) return;
   p->descriptor_.product = name;
   const int index = availableProducts().indexOf(name);
   static const std::array<const char*, 7> identities {
      "REF", "VEL", "SW", "ZDR", "PHI", "RHO", "CFP"};
   p->descriptor_.identityKind = products::ProductDescriptor::IdentityKind::Level2Moment;
   p->descriptor_.identity = QString::fromLatin1(identities[static_cast<std::size_t>(index)]);
   p->descriptor_.elevation = 0.0f;
   p->descriptor_.palette.clear();
   p->RebindProduct(); p->ConnectProductSignals(this);
   Q_EMIT productChanged(); Q_EMIT paletteChanged();
   Q_EMIT channelChanged(SyncChannel::Product, ChangeOrigin::UserInput);
}

void PaneController::setSelectedElevation(double elevation)
{
   if (qFuzzyCompare(p->descriptor_.elevation, static_cast<float>(elevation))) return;
   p->descriptor_.elevation = static_cast<float>(elevation);
   p->RebindProduct(); p->ConnectProductSignals(this);
   Q_EMIT productChanged();
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
   refreshProductCatalog();

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
QString PaneController::selectedStorm() const { return p->selectedStorm_; }

void PaneController::selectStorm(const QString& stormId)
{
   if (p->selectedStorm_ == stormId) return;
   p->selectedStorm_ = stormId;
   Q_EMIT selectedStormChanged();
   Q_EMIT channelChanged(SyncChannel::SelectedStorm, ChangeOrigin::UserInput);
}

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
      return QVariantMap {
         {QStringLiteral("kind"), level3Product() ? QStringLiteral("level3")
                                                   : QStringLiteral("level2")},
         {QStringLiteral("identity"), p->descriptor_.identity},
         {QStringLiteral("name"), p->descriptor_.product}};

   case SyncChannel::Time:
      if (p->archiveTime_.has_value())
         return QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(
            std::chrono::duration_cast<std::chrono::milliseconds>(
               p->archiveTime_->time_since_epoch()).count(), Qt::UTC));
      return QDateTime {};
   case SyncChannel::Animation:
   case SyncChannel::Cursor:
      return {};
   case SyncChannel::Palette:
      return p->descriptor_.palette;
   case SyncChannel::SelectedStorm:
      return p->selectedStorm_;
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
   {
      const QVariantMap selection = value.toMap();
      if (selection.isEmpty()) return;
      const bool level3 = selection.value(QStringLiteral("kind")).toString() == QStringLiteral("level3");
      const QString identity = selection.value(QStringLiteral("identity")).toString();
      const QString name = selection.value(QStringLiteral("name")).toString();
      if (p->descriptor_.product != name || p->descriptor_.identity != identity ||
          level3Product() != level3)
      {
         p->descriptor_.product = name;
         p->descriptor_.identity = identity;
         p->descriptor_.identityKind = level3
            ? products::ProductDescriptor::IdentityKind::Level3Awips
            : products::ProductDescriptor::IdentityKind::Level2Moment;
         p->descriptor_.elevation = 0.0f;
         p->descriptor_.palette.clear();
         p->RebindProduct();
         p->ConnectProductSignals(this);
         Q_EMIT productChanged();
         Q_EMIT paletteChanged();
         changed = true;
      }
      break;
   }

   case SyncChannel::Palette:
      if (p->descriptor_.palette != value.toString())
      {
         p->descriptor_.palette = value.toString();
         p->ApplyPalette();
         if (!p->map_.isNull()) p->map_->triggerRepaint();
         Q_EMIT paletteChanged();
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

   case SyncChannel::SelectedStorm:
      if (p->selectedStorm_ != value.toString())
      {
         p->selectedStorm_ = value.toString();
         Q_EMIT selectedStormChanged();
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

   // Kept for the projection helpers, which the QML object overlay needs on every frame.
   p->map_ = map;
   p->ConnectProductSignals(this);

   logger_->info("Pane {} registering replaceable radar sweep layer", p->paneId_);

   map->addCustomLayer("wxlens-radar-sweep",
                       std::make_unique<render::RadarSweepLayer>(p->radarLayerBinding_));

   if (p->radarProduct_ != nullptr && p->radarProduct_->sweep_data() != nullptr)
   {
      // The data load beat the style load (a network race, not a guaranteed order), so
      // SweepUpdated already fired before the connection above existed.
      map->triggerRepaint();
   }
}

void PaneController::applyMapDetails(const QVariantMap& visibility)
{
   if (p->map_ == nullptr)
   {
      return;
   }

   const auto groupForLayer = [](QString id) -> QString
   {
      id = id.toLower();
      if (id.contains(QStringLiteral("hillshade")) || id.contains(QStringLiteral("terrain")))
         return QStringLiteral("terrain");
      if (id.contains(QStringLiteral("building")))
         return QStringLiteral("buildings");
      if (id.contains(QStringLiteral("poi")))
         return QStringLiteral("poi");
      if (id.contains(QStringLiteral("boundary")) || id.contains(QStringLiteral("border")))
         return QStringLiteral("boundaries");
      if (id.contains(QStringLiteral("water")) && id.contains(QStringLiteral("label")))
         return QStringLiteral("water");
      if (id.contains(QStringLiteral("place")) || id.contains(QStringLiteral("city")) ||
          id.contains(QStringLiteral("town")) || id.contains(QStringLiteral("village")))
         return QStringLiteral("places");
      if (id.contains(QStringLiteral("road")) || id.contains(QStringLiteral("highway")) ||
          id.contains(QStringLiteral("street")) || id.contains(QStringLiteral("transport")))
         return QStringLiteral("roads");
      return {};
   };

   for (const QString& layerId : p->map_->layerIds())
   {
      const QString group = groupForLayer(layerId);
      if (!group.isEmpty() && visibility.contains(group))
      {
         p->map_->setLayoutProperty(layerId,
                                    QStringLiteral("visibility"),
                                    visibility.value(group).toBool() ? QStringLiteral("visible")
                                                                     : QStringLiteral("none"));
      }
   }
   p->map_->triggerRepaint();
}

} // namespace panes
} // namespace wxlens
