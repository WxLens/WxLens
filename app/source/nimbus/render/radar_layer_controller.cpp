#include <nimbus/render/radar_layer_controller.hpp>
#include <nimbus/render/radar_site_marker_layer.hpp>
#include <nimbus/log/logger.hpp>

namespace nimbus
{
namespace render
{

static const std::string logPrefix_ = "render.radar_layer_controller";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

RadarLayerController::RadarLayerController(QObject* parent) : QObject(parent) {}

void RadarLayerController::attachSiteMarker(QMapLibre::Map* map,
                                            double           latitude,
                                            double           longitude)
{
   if (map == nullptr)
   {
      logger_->error("attachSiteMarker called with a null Map");
      return;
   }

   logger_->info("Registering radar site marker custom layer at {}, {}", latitude, longitude);

   map->addCustomLayer("nimbus-radar-site-marker",
                       std::make_unique<RadarSiteMarkerLayer>(latitude, longitude));
}

} // namespace render
} // namespace nimbus
