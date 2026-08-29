#include <wxlens/products/level3_radial_product.hpp>

#include <wxlens/util/geodesic.hpp>

#include <scwx/common/products.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <algorithm>
#include <cstddef>

namespace wxlens
{
namespace products
{
namespace
{
constexpr std::uint8_t kRangeFolded = 1u;

std::shared_ptr<scwx::wsr88d::rpg::GenericRadialDataPacket>
FindRadialPacket(const scwx::wsr88d::rpg::ProductSymbologyBlock& block)
{
   std::shared_ptr<scwx::wsr88d::rpg::RadialDataPacket> fallback;
   for (std::uint16_t layer = 0; layer < block.number_of_layers(); ++layer)
   {
      for (const auto& packet : block.packet_list(layer))
      {
         if (auto digital = std::dynamic_pointer_cast<
                scwx::wsr88d::rpg::DigitalRadialDataArrayPacket>(packet))
         {
            return digital;
         }
         if (fallback == nullptr)
         {
            fallback =
               std::dynamic_pointer_cast<scwx::wsr88d::rpg::RadialDataPacket>(
                  packet);
         }
      }
   }
   return fallback;
}

void AppendVertex(SweepData&   sweep,
                  double       latitude,
                  double       longitude,
                  std::uint8_t value)
{
   sweep.vertices.push_back(static_cast<float>(latitude));
   sweep.vertices.push_back(static_cast<float>(longitude));
   sweep.dataMoments8.push_back(value);
}
} // namespace

std::optional<Level3RadialSnapshot>
BuildLevel3RadialSnapshot(const scwx::wsr88d::Level3File& file)
{
   auto message =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::GraphicProductMessage>(
         file.message());
   if (message == nullptr || message->description_block() == nullptr ||
       message->symbology_block() == nullptr)
   {
      return std::nullopt;
   }

   const auto description = message->description_block();
   const auto radial      = FindRadialPacket(*message->symbology_block());
   if (radial == nullptr || radial->number_of_radials() < 1 ||
       radial->number_of_radials() > 720 || radial->number_of_range_bins() < 1)
   {
      return std::nullopt;
   }

   auto sweep              = std::make_shared<SweepData>();
   sweep->dataMomentOffset = description->offset();
   sweep->dataMomentScale =
      description->scale() == 0.0f ? 1.0f : description->scale();

   const std::size_t maximumVertices =
      static_cast<std::size_t>(radial->number_of_radials()) *
      radial->number_of_range_bins() * 6u;
   sweep->vertices.reserve(maximumVertices * 2u);
   sweep->dataMoments8.reserve(maximumVertices);

   const double        latitude   = description->latitude_of_radar();
   const double        longitude  = description->longitude_of_radar();
   const double        gateLength = description->x_resolution_raw();
   const double        firstGate  = radial->index_of_first_range_bin();
   const std::uint16_t threshold  = description->threshold();

   for (std::uint16_t r = 0; r < radial->number_of_radials(); ++r)
   {
      const auto&  levels     = radial->level(r);
      const double startAngle = radial->start_angle(r);
      const double endAngle   = startAngle + radial->delta_angle(r);
      const auto   count =
         std::min<std::size_t>(levels.size(), radial->number_of_range_bins());

      for (std::size_t gate = 0; gate < count; ++gate)
      {
         const std::uint8_t value = levels[gate];
         if (value < threshold && value != kRangeFolded)
         {
            continue;
         }

         const double innerRange =
            (firstGate + static_cast<double>(gate)) * gateLength;
         const double outerRange = innerRange + gateLength;
         const auto [innerStartLat, innerStartLon] =
            util::GeodesicDirect(latitude, longitude, startAngle, innerRange);
         const auto [outerStartLat, outerStartLon] =
            util::GeodesicDirect(latitude, longitude, startAngle, outerRange);
         const auto [innerEndLat, innerEndLon] =
            util::GeodesicDirect(latitude, longitude, endAngle, innerRange);
         const auto [outerEndLat, outerEndLon] =
            util::GeodesicDirect(latitude, longitude, endAngle, outerRange);

         AppendVertex(*sweep, innerStartLat, innerStartLon, value);
         AppendVertex(*sweep, outerStartLat, outerStartLon, value);
         AppendVertex(*sweep, outerEndLat, outerEndLon, value);
         AppendVertex(*sweep, innerStartLat, innerStartLon, value);
         AppendVertex(*sweep, innerEndLat, innerEndLon, value);
         AppendVertex(*sweep, outerEndLat, outerEndLon, value);
      }
   }

   Level3RadialMetadata metadata;
   metadata.productCode    = description->product_code();
   metadata.threshold      = threshold;
   metadata.offset         = description->offset();
   metadata.scale          = description->scale();
   metadata.dataLevelCoded = description->IsDataLevelCoded();
   metadata.productTime =
      scwx::util::TimePoint(description->volume_scan_date(),
                            description->volume_scan_start_time() * 1000u);
   if (description->has_elevation())
   {
      metadata.elevationDegrees = description->elevation().value();
   }
   metadata.defaultPalette =
      scwx::common::GetLevel3Palette(metadata.productCode);
   for (std::size_t i = 0; i < metadata.thresholds.size(); ++i)
   {
      metadata.thresholds[i] = description->data_level_threshold(i);
   }
   for (std::size_t i = 0; i < metadata.decodedValues.size(); ++i)
   {
      metadata.decodedValues[i] =
         description->data_value(static_cast<std::uint8_t>(i));
      metadata.decodedCodes[i] =
         description->data_level_code(static_cast<std::uint8_t>(i));
   }

   return Level3RadialSnapshot {std::move(sweep), std::move(metadata)};
}

} // namespace products
} // namespace wxlens
