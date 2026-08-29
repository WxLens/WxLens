#include <wxlens/products/level3_raster_product.hpp>

#include <wxlens/util/geodesic.hpp>

#include <scwx/common/products.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/digital_precipitation_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/precipitation_rate_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/raster_data_packet.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wxlens
{
namespace products
{
namespace
{
constexpr std::uint8_t kRangeFolded = 1u;

struct CartesianPackets
{
   std::shared_ptr<scwx::wsr88d::rpg::RasterDataPacket> raster;
   Level3CartesianPacketFamily family {Level3CartesianPacketFamily::None};
};

CartesianPackets FindCartesianPacket(const scwx::wsr88d::rpg::ProductSymbologyBlock& block)
{
   CartesianPackets result;
   for (std::uint16_t layer = 0; layer < block.number_of_layers(); ++layer)
   {
      for (const auto& packet : block.packet_list(layer))
      {
         if (auto raster =
                std::dynamic_pointer_cast<scwx::wsr88d::rpg::RasterDataPacket>(packet))
         {
            return {std::move(raster), Level3CartesianPacketFamily::Raster};
         }
         if (std::dynamic_pointer_cast<
                scwx::wsr88d::rpg::DigitalPrecipitationDataArrayPacket>(packet))
         {
            result.family = Level3CartesianPacketFamily::DigitalPrecipitationArray;
         }
         else if (std::dynamic_pointer_cast<
                     scwx::wsr88d::rpg::PrecipitationRateDataArrayPacket>(packet))
         {
            result.family = Level3CartesianPacketFamily::PrecipitationRateArray;
         }
      }
   }
   return result;
}

void AppendVertex(SweepData& sweep, double latitude, double longitude, std::uint8_t value)
{
   sweep.vertices.push_back(static_cast<float>(latitude));
   sweep.vertices.push_back(static_cast<float>(longitude));
   sweep.dataMoments8.push_back(value);
}
} // namespace

Level3CartesianPacketFamily
DetectLevel3CartesianPacketFamily(const scwx::wsr88d::Level3File& file)
{
   const auto message =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::GraphicProductMessage>(file.message());
   if (message == nullptr || message->symbology_block() == nullptr)
   {
      return Level3CartesianPacketFamily::None;
   }
   return FindCartesianPacket(*message->symbology_block()).family;
}

std::optional<Level3RasterSnapshot>
BuildLevel3RasterSnapshot(const scwx::wsr88d::Level3File& file)
{
   const auto message =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::GraphicProductMessage>(file.message());
   if (message == nullptr || message->description_block() == nullptr ||
       message->symbology_block() == nullptr)
   {
      return std::nullopt;
   }

   const auto description = message->description_block();
   const auto packets     = FindCartesianPacket(*message->symbology_block());
   const auto raster      = packets.raster;
   if (raster == nullptr || raster->number_of_rows() == 0)
   {
      return std::nullopt;
   }

   std::size_t maximumColumns = 0;
   for (std::uint16_t row = 0; row < raster->number_of_rows(); ++row)
   {
      maximumColumns = std::max(maximumColumns, raster->level(row).size());
   }
   if (maximumColumns == 0 || description->x_resolution_raw() == 0 ||
       description->y_resolution_raw() == 0)
   {
      return std::nullopt;
   }

   auto sweep              = std::make_shared<SweepData>();
   sweep->dataMomentOffset = description->offset();
   sweep->dataMomentScale  = description->scale() == 0.0f ? 1.0f : description->scale();
   const std::size_t maximumVertices =
      static_cast<std::size_t>(raster->number_of_rows()) * maximumColumns * 6u;
   sweep->vertices.reserve(maximumVertices * 2u);
   sweep->dataMoments8.reserve(maximumVertices);

   const double latitude  = description->latitude_of_radar();
   const double longitude = description->longitude_of_radar();
   const double rangeKm   = description->range();
   const double xStartMeters =
      (-static_cast<double>(raster->i_coordinate_start()) - 1.0 - rangeKm) * 1000.0;
   const double yStartMeters =
      (static_cast<double>(raster->j_coordinate_start()) + 1.0 + rangeKm) * 1000.0;
   const double xResolution = description->x_resolution_raw();
   const double yResolution = description->y_resolution_raw();
   const std::uint16_t threshold = description->threshold();

   auto coordinate = [&](std::size_t row, std::size_t column)
   {
      const double east  = xStartMeters + xResolution * static_cast<double>(column);
      const double north = yStartMeters - yResolution * static_cast<double>(row);
      const double azimuth = std::atan2(east, north) * 180.0 / std::numbers::pi;
      return util::GeodesicDirect(
         latitude, longitude, azimuth, std::hypot(east, north));
   };

   for (std::size_t row = 0; row < raster->number_of_rows(); ++row)
   {
      const auto& levels = raster->level(static_cast<std::uint16_t>(row));
      for (std::size_t column = 0; column < levels.size(); ++column)
      {
         const std::uint8_t value = levels[column];
         if (value < threshold && value != kRangeFolded)
         {
            continue;
         }

         const auto [northWestLat, northWestLon] = coordinate(row, column);
         const auto [northEastLat, northEastLon] = coordinate(row, column + 1u);
         const auto [southWestLat, southWestLon] = coordinate(row + 1u, column);
         const auto [southEastLat, southEastLon] = coordinate(row + 1u, column + 1u);
         AppendVertex(*sweep, northWestLat, northWestLon, value);
         AppendVertex(*sweep, northEastLat, northEastLon, value);
         AppendVertex(*sweep, southEastLat, southEastLon, value);
         AppendVertex(*sweep, northWestLat, northWestLon, value);
         AppendVertex(*sweep, southWestLat, southWestLon, value);
         AppendVertex(*sweep, southEastLat, southEastLon, value);
      }
   }

   Level3RasterMetadata metadata;
   metadata.productCode       = description->product_code();
   metadata.threshold         = threshold;
   metadata.offset            = description->offset();
   metadata.scale             = description->scale();
   metadata.dataLevelCoded    = description->IsDataLevelCoded();
   metadata.productTime       = scwx::util::TimePoint(
      description->volume_scan_date(), description->volume_scan_start_time() * 1000u);
   metadata.defaultPalette    = scwx::common::GetLevel3Palette(metadata.productCode);
   metadata.rows              = raster->number_of_rows();
   metadata.maximumColumns    = maximumColumns;
   metadata.xResolutionMeters = description->x_resolution_raw();
   metadata.yResolutionMeters = description->y_resolution_raw();
   for (std::size_t i = 0; i < metadata.thresholds.size(); ++i)
   {
      metadata.thresholds[i] = description->data_level_threshold(i);
   }
   for (std::size_t i = 0; i < metadata.decodedValues.size(); ++i)
   {
      metadata.decodedValues[i] = description->data_value(static_cast<std::uint8_t>(i));
      metadata.decodedCodes[i] = description->data_level_code(static_cast<std::uint8_t>(i));
   }

   return Level3RasterSnapshot {std::move(sweep), std::move(metadata)};
}

} // namespace products
} // namespace wxlens
