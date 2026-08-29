#pragma once

#include <wxlens/products/radar_sweep_product.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace scwx::wsr88d
{
class Level3File;
enum class DataLevelCode;
} // namespace scwx::wsr88d

namespace wxlens
{
namespace products
{

/** Metadata which must survive the Level 3 parser -> renderer boundary. */
struct Level3RadialMetadata
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
   std::optional<double> elevationDegrees {};
   std::string           defaultPalette;
};

struct Level3RadialSnapshot
{
   std::shared_ptr<const SweepData> sweep;
   Level3RadialMetadata             metadata;
};

/**
 * Converts either packet-16 digital radial data or packet-AF1F legacy radial
 * data from a parsed Level 3 file into the same immutable GPU geometry contract
 * used by RadarSweepLayer. Digital packets are preferred when both occur,
 * matching the legacy Level3RadialView.
 */
[[nodiscard]] std::optional<Level3RadialSnapshot>
BuildLevel3RadialSnapshot(const scwx::wsr88d::Level3File& file);

} // namespace products
} // namespace wxlens
