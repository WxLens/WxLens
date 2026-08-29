#include <wxlens/products/level3_text_product.hpp>

#include <scwx/awips/text_product_message.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/tabular_product_message.hpp>
#include <scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp>

#include <memory>

namespace wxlens
{
namespace products
{
namespace
{
void AddTabularPages(Level3TextSnapshot& result,
                     const std::shared_ptr<scwx::wsr88d::rpg::TabularAlphanumericBlock>& block)
{
   result.tabularBlockAvailable = block != nullptr;
   if (!block) return;
   for (const auto& sourcePage : block->page_list())
   {
      ProductTextPage page;
      for (const auto& line : sourcePage)
         page.entries.push_back({ProductTextKind::TabularText, line});
      result.tabularPages.push_back(std::move(page));
   }
}

void AddGraphicPages(Level3TextSnapshot& result,
                     const std::shared_ptr<scwx::wsr88d::rpg::GraphicAlphanumericBlock>& block)
{
   result.graphicBlockAvailable = block != nullptr;
   if (!block) return;
   for (const auto& sourcePage : block->page_list())
   {
      ProductTextPage page;
      for (const auto& packet : sourcePage)
      {
         const auto text =
            std::dynamic_pointer_cast<scwx::wsr88d::rpg::TextAndSpecialSymbolPacket>(packet);
         if (!text) continue;
         ProductTextEntry entry;
         entry.kind = ProductTextKind::GraphicAnnotation;
         entry.text = text->text();
         entry.rawValue = text->value_of_text();
         entry.rawI = text->start_i();
         entry.rawJ = text->start_j();
         page.entries.push_back(std::move(entry));
      }
      result.graphicPages.push_back(std::move(page));
   }
}
} // namespace

std::optional<Level3TextSnapshot>
BuildLevel3TextSnapshot(const scwx::wsr88d::Level3File& file)
{
   const auto graphic =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::GraphicProductMessage>(file.message());
   const auto tabular =
      std::dynamic_pointer_cast<scwx::wsr88d::rpg::TabularProductMessage>(file.message());
   if (!graphic && !tabular) return std::nullopt;

   const auto description = graphic ? graphic->description_block() : tabular->description_block();
   if (!description) return std::nullopt;

   Level3TextSnapshot result;
   result.productCode = description->product_code();
   result.productTime = scwx::util::TimePoint(description->generation_date_of_product(),
                                              description->generation_time_of_product() * 1000u);
   if (file.wmo_header())
      result.awipsId = file.wmo_header()->product_category() +
                       file.wmo_header()->product_designator();

   if (graphic)
   {
      AddGraphicPages(result, graphic->graphic_block());
      AddTabularPages(result, graphic->tabular_block());
   }
   else
   {
      AddTabularPages(result, tabular->tabular_block());
   }
   result.rawMetadataNote =
      "Graphic annotation positions are raw product-page I/J coordinates, not geographic locations.";
   return result;
}

std::optional<Level3TextSnapshot>
BuildTextProductSnapshot(const scwx::awips::TextProductMessage& message)
{
   if (message.message_content().empty()) return std::nullopt;
   Level3TextSnapshot result;
   if (message.wmo_header())
      result.awipsId = message.wmo_header()->product_category() +
                       message.wmo_header()->product_designator();
   result.tabularBlockAvailable = true;
   ProductTextPage page;
   page.entries.push_back({ProductTextKind::AwipsText, message.message_content()});
   result.tabularPages.push_back(std::move(page));
   result.rawMetadataNote = "This text product contains no geographic placement metadata.";
   return result;
}

} // namespace products
} // namespace wxlens
