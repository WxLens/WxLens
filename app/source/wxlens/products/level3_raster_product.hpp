#pragma once

#include <wxlens/products/radar_sweep_product.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace scwx
{
namespace wsr88d
{
class Level3File;
enum class DataLevelCode;
} // namespace wsr88d
} // namespace scwx

namespace wxlens
{
namespace products
{

enum class Level3CartesianPacketFamily
{
   None,
   Raster,
   DigitalPrecipitationArray,
   PrecipitationRateArray
};

struct Level3RasterMetadata
{
   std::int16_t                          productCode {};
   std::uint16_t                         threshold {};
   float                                 offset {};
   float                                 scale {1.0f};
   bool                                  dataLevelCoded {false};
   std::array<std::uint16_t, 16>         thresholds {};
   std::array<std::optional<float>, 256> decodedValues {};
   std::array<std::optional<scwx::wsr88d::DataLevelCode>, 256> decodedCodes {};
   std::chrono::system_clock::time_point                       productTime {};
   std::string           defaultPalette;
   std::uint16_t         rows {};
   std::size_t           maximumColumns {};
   std::uint16_t         xResolutionMeters {};
   std::uint16_t         yResolutionMeters {};
};

struct Level3RasterSnapshot
{
   std::shared_ptr<const SweepData> sweep;
   Level3RasterMetadata             metadata;
};

/** Identifies Cartesian packet families without claiming they are all renderable. */
[[nodiscard]] Level3CartesianPacketFamily
DetectLevel3CartesianPacketFamily(const scwx::wsr88d::Level3File& file);

/**
 * Converts packet BA07/BA0F Cartesian bins into immutable geographic triangles.
 * Packet 17/18 precipitation arrays are detected separately but cannot be
 * converted until wxdata exposes their decoded rows.
 */
[[nodiscard]] std::optional<Level3RasterSnapshot>
BuildLevel3RasterSnapshot(const scwx::wsr88d::Level3File& file);

} // namespace products
} // namespace wxlens
