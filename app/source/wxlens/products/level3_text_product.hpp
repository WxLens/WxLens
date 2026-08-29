#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scwx { namespace awips { class TextProductMessage; } }
namespace scwx { namespace wsr88d { class Level3File; } }

namespace wxlens
{
namespace products
{

enum class ProductTextKind
{
   GraphicAnnotation,
   TabularText,
   AwipsText
};

struct ProductTextEntry
{
   ProductTextKind kind {ProductTextKind::TabularText};
   std::string text;
   std::optional<std::uint16_t> rawValue;
   std::optional<std::int16_t> rawI;
   std::optional<std::int16_t> rawJ;
};

struct ProductTextPage
{
   std::vector<ProductTextEntry> entries;
};

/** Immutable, presentation-neutral text belonging to one product/time selection.
 * Graphic packet I/J positions are retained only as raw product coordinates. They are
 * deliberately not converted to latitude/longitude: the graphic block does not define a
 * geographic anchor for these page annotations. */
struct Level3TextSnapshot
{
   std::optional<std::chrono::system_clock::time_point> productTime;
   std::optional<std::int16_t> productCode;
   std::string awipsId;
   std::vector<ProductTextPage> graphicPages;
   std::vector<ProductTextPage> tabularPages;
   bool graphicBlockAvailable {false};
   bool tabularBlockAvailable {false};
   std::string rawMetadataNote;
};

[[nodiscard]] std::optional<Level3TextSnapshot>
BuildLevel3TextSnapshot(const scwx::wsr88d::Level3File& file);

/** Adapts a wxdata AWIPS text-only message to the same product-details contract. */
[[nodiscard]] std::optional<Level3TextSnapshot>
BuildTextProductSnapshot(const scwx::awips::TextProductMessage& message);

} // namespace products
} // namespace wxlens
