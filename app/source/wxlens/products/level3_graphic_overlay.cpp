#include <wxlens/products/level3_graphic_overlay.hpp>

#include <wxlens/util/geodesic.hpp>

#include <scwx/util/time.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/mesocyclone_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_feature_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>
#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
#include <scwx/wsr88d/rpg/sti_circle_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wxlens
{
namespace products
{
namespace
{
GeographicPoint Offset(double latitude, double longitude, double eastKm, double northKm)
{
   const double bearing = std::atan2(eastKm, northKm) * 180.0 / std::numbers::pi;
   const auto [lat, lon] = util::GeodesicDirect(
      latitude, longitude, bearing, std::hypot(eastKm, northKm) * 1000.0);
   return {lat, lon};
}

GraphicOverlayPrimitive Point(GraphicOverlayKind kind,
                              double latitude,
                              double longitude,
                              std::int16_t i,
                              std::int16_t j)
{
   GraphicOverlayPrimitive result;
   result.kind = kind;
   result.geometry.push_back(Offset(latitude, longitude, i, j));
   return result;
}

void AddLinkedVector(Level3GraphicOverlaySnapshot& result,
                     const scwx::wsr88d::rpg::LinkedVectorPacket& packet,
                     double latitude,
                     double longitude,
                     const std::string& stormId,
                     bool past,
                     bool forecast)
{
   GraphicOverlayPrimitive primitive;
   primitive.kind = GraphicOverlayKind::Polyline;
   primitive.stormId = stormId;
   primitive.past = past;
   primitive.forecast = forecast;
   primitive.value = packet.value_of_vector();
   primitive.geometry.push_back(Offset(latitude,
                                       longitude,
                                       packet.start_i_km().value(),
                                       packet.start_j_km().value()));
   const auto east = packet.end_i_km();
   const auto north = packet.end_j_km();
   for (std::size_t i = 0; i < std::min(east.size(), north.size()); ++i)
      primitive.geometry.push_back(
         Offset(latitude, longitude, east[i].value(), north[i].value()));
   if (primitive.geometry.size() > 1)
      result.primitives.push_back(std::move(primitive));
}

void AddPacket(Level3GraphicOverlaySnapshot& result,
               const std::shared_ptr<scwx::wsr88d::rpg::Packet>& packet,
               double latitude,
               double longitude,
               std::string& currentStorm)
{
   using namespace scwx::wsr88d::rpg;
   if (auto p = std::dynamic_pointer_cast<StormIdSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
      {
         auto primitive = Point(GraphicOverlayKind::StormId, latitude, longitude,
                                p->i_position(i), p->j_position(i));
         primitive.stormId = p->storm_id(i);
         primitive.label = primitive.stormId;
         result.primitives.push_back(std::move(primitive));
      }
      if (p->RecordCount() > 0) currentStorm = p->storm_id(0);
      return;
   }
   if (auto p = std::dynamic_pointer_cast<ScitDataPacket>(packet))
   {
      const bool past = p->packet_code() == static_cast<std::uint16_t>(PacketCode::ScitPastData);
      for (const auto& subpacket : p->packet_list())
      {
         if (auto vector = std::dynamic_pointer_cast<LinkedVectorPacket>(subpacket))
            AddLinkedVector(result, *vector, latitude, longitude, currentStorm, past, !past);
         else result.unsupportedPacketCodes.push_back(subpacket->packet_code());
      }
      return;
   }
   if (auto p = std::dynamic_pointer_cast<HdaHailSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
      {
         auto primitive = Point(GraphicOverlayKind::Hail, latitude, longitude,
                                p->i_position(i), p->j_position(i));
         primitive.value = p->max_hail_size(i);
         result.primitives.push_back(std::move(primitive));
      }
      return;
   }
   if (auto p = std::dynamic_pointer_cast<MesocycloneSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
      {
         auto primitive = Point(GraphicOverlayKind::Mesocyclone, latitude, longitude,
                                p->i_position(i), p->j_position(i));
         primitive.radiusMeters = p->radius_of_mesocyclone(i) * 1000.0;
         result.primitives.push_back(std::move(primitive));
      }
      return;
   }
   if (auto p = std::dynamic_pointer_cast<PointFeatureSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
      {
         auto primitive = Point(GraphicOverlayKind::PointFeature, latitude, longitude,
                                p->i_position(i), p->j_position(i));
         primitive.value = p->point_feature_type(i);
         result.primitives.push_back(std::move(primitive));
      }
      return;
   }
   if (auto p = std::dynamic_pointer_cast<PointGraphicSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
         result.primitives.push_back(Point(GraphicOverlayKind::PointSymbol, latitude, longitude,
                                           p->i_position(i), p->j_position(i)));
      return;
   }
   if (auto p = std::dynamic_pointer_cast<StiCircleSymbolPacket>(packet))
   {
      for (std::size_t i = 0; i < p->RecordCount(); ++i)
      {
         auto primitive = Point(GraphicOverlayKind::StiCircle, latitude, longitude,
                                p->i_position(i), p->j_position(i));
         primitive.radiusMeters = p->radius_of_circle(i) * 1000.0;
         result.primitives.push_back(std::move(primitive));
      }
      return;
   }
   result.unsupportedPacketCodes.push_back(packet->packet_code());
}
} // namespace

std::optional<Level3GraphicOverlaySnapshot>
BuildLevel3GraphicOverlaySnapshot(const scwx::wsr88d::Level3File& file)
{
   const auto message =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::GraphicProductMessage>(file.message());
   if (!message || !message->description_block() || !message->symbology_block())
      return std::nullopt;

   const auto description = message->description_block();
   Level3GraphicOverlaySnapshot result;
   result.productTime = scwx::util::TimePoint(description->volume_scan_date(),
                                              description->volume_scan_start_time() * 1000u);
   std::string currentStorm;
   for (std::uint16_t layer = 0; layer < message->symbology_block()->number_of_layers(); ++layer)
      for (const auto& packet : message->symbology_block()->packet_list(layer))
         AddPacket(result, packet, description->latitude_of_radar(),
                   description->longitude_of_radar(), currentStorm);

   std::sort(result.unsupportedPacketCodes.begin(), result.unsupportedPacketCodes.end());
   result.unsupportedPacketCodes.erase(
      std::unique(result.unsupportedPacketCodes.begin(), result.unsupportedPacketCodes.end()),
      result.unsupportedPacketCodes.end());
   if (result.primitives.empty()) return std::nullopt;
   return result;
}

} // namespace products
} // namespace wxlens
