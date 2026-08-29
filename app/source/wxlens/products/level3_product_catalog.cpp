#include <wxlens/products/level3_product_catalog.hpp>

#include <scwx/common/products.hpp>

#include <algorithm>
#include <unordered_set>

namespace wxlens
{
namespace products
{

std::vector<Level3ProductDescriptor>
BuildLevel3ProductCatalog(const std::vector<std::string>& availableAwipsIds)
{
   std::unordered_set<std::string> available;
   for (const auto& awipsId : availableAwipsIds)
   {
      if (scwx::common::GetLevel3ProductByAwipsId(awipsId) != "?")
      {
         available.insert(awipsId);
      }
   }

   std::vector<Level3ProductDescriptor> catalog;
   for (const auto category : scwx::common::Level3ProductCategoryIterator())
   {
      for (const auto& product :
           scwx::common::GetLevel3ProductsByCategory(category))
      {
         for (const auto& awipsId :
              scwx::common::GetLevel3AwipsIdsByProduct(product))
         {
            if (!available.contains(awipsId))
               continue;
            catalog.push_back(
               {QString::fromStdString(
                   scwx::common::GetLevel3CategoryName(category)),
                QString::fromStdString(
                   scwx::common::GetLevel3CategoryDescription(category)),
                QString::fromStdString(product),
                QString::fromStdString(
                   scwx::common::GetLevel3ProductDescription(product)),
                QString::fromStdString(awipsId)});
         }
      }
   }

   return catalog;
}

} // namespace products
} // namespace wxlens
