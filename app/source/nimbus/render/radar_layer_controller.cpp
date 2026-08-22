#include <nimbus/render/radar_layer_controller.hpp>
#include <nimbus/render/radar_sweep_layer.hpp>
#include <nimbus/log/logger.hpp>

namespace nimbus
{
namespace render
{

static const std::string logPrefix_ = "render.radar_layer_controller";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

RadarLayerController::RadarLayerController(QObject* parent) : QObject(parent) {}

void RadarLayerController::attachRadarSweep(QMapLibre::Map* map,
                                            const QString&   siteId,
                                            double            latitude,
                                            double            longitude)
{
   if (map == nullptr)
   {
      logger_->error("attachRadarSweep called with a null Map");
      return;
   }

   const std::string site = siteId.toStdString();

   auto [it, inserted] = products_.try_emplace(site, nullptr);
   if (inserted)
   {
      // No QObject parent: the shared_ptr owns this. Giving it a parent as well would let Qt's
      // parent-child cleanup delete it out from under the shared_ptr (and under the custom layer
      // still holding one).
      it->second = std::make_shared<products::RadarSweepProduct>(site, latitude, longitude);
   }

   logger_->info("Registering radar sweep custom layer for {}", site);

   map->addCustomLayer("nimbus-radar-sweep", std::make_unique<RadarSweepLayer>(it->second));

   // Radar data arrives asynchronously, long after the style loaded and mbgl already drew its
   // last frame. Registering a custom layer doesn't make mbgl repaint on its own, so without
   // this the sweep stays invisible until some unrelated interaction (a scroll/pan) forces a
   // new frame - exactly what a real KEAX launch showed. `map` is the connection's context
   // object, so the connection is torn down automatically when the map is destroyed.
   connect(it->second.get(),
           &products::RadarSweepProduct::SweepUpdated,
           map,
           [map]() { map->triggerRepaint(); });

   if (it->second->sweep_data() != nullptr)
   {
      // Data beat the style load (load order isn't guaranteed - it's a network race), so
      // SweepUpdated already fired before the connection above existed.
      map->triggerRepaint();
   }
}

} // namespace render
} // namespace nimbus
