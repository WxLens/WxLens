#include <wxlens/products/level3_product_catalog.hpp>

#include <scwx/common/products.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace wxlens
{
namespace products
{
namespace
{

ProductSectionId SectionOf(scwx::common::Level3ProductCategory category)
{
   using Category = scwx::common::Level3ProductCategory;
   switch (category)
   {
   case Category::Reflectivity: return ProductSectionId::Reflectivity;
   case Category::Velocity:
   case Category::StormRelativeVelocity:
   case Category::SpectrumWidth: return ProductSectionId::Velocity;
   case Category::DifferentialReflectivity:
   case Category::SpecificDifferentialPhase:
   case Category::CorrelationCoefficient:
   case Category::HydrometeorClassification: return ProductSectionId::DualPolarization;
   case Category::PrecipitationAccumulation:
      return ProductSectionId::PrecipitationAccumulation;
   case Category::VerticallyIntegratedLiquid:
   case Category::EchoTops:
   case Category::Unknown: return ProductSectionId::Other;
   }
   return ProductSectionId::Other;
}

} // namespace

QString ProductSectionName(ProductSectionId section)
{
   switch (section)
   {
   case ProductSectionId::Reflectivity: return QStringLiteral("Reflectivity Products");
   case ProductSectionId::Velocity: return QStringLiteral("Velocity Products");
   case ProductSectionId::DualPolarization: return QStringLiteral("Dual-Polarization");
   case ProductSectionId::PrecipitationAccumulation:
      return QStringLiteral("Precipitation Accumulation");
   case ProductSectionId::Other: break;
   }
   return QStringLiteral("Other Products");
}

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
         bool recommendedAssigned = false;
         for (const auto& awipsId :
              scwx::common::GetLevel3AwipsIdsByProduct(product))
         {
            if (!available.contains(awipsId))
               continue;
            const QString description = QString::fromStdString(
               scwx::common::GetLevel3ProductDescription(product));
            catalog.push_back(
               {QString::fromStdString(
                   scwx::common::GetLevel3CategoryName(category)),
                ProductSectionName(SectionOf(category)),
                QString::fromStdString(product),
                description,
                QString::fromStdString(awipsId),
                description,
                !std::exchange(recommendedAssigned, true),
                QString {},
                false});
         }
      }
   }

   return catalog;
}

} // namespace products
} // namespace wxlens
