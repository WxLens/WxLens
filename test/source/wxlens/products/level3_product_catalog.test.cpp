#include <wxlens/products/level3_product_catalog.hpp>
#include <wxlens/products/product_descriptor.hpp>

#include <gtest/gtest.h>

namespace wxlens::products
{
namespace
{

TEST(Level3ProductCatalog, MapsAvailableAwipsIdsToCanonicalCategories)
{
   const auto catalog =
      BuildLevel3ProductCatalog({"N0B", "N0G", "N0C", "DAA", "???", "N0B"});

   ASSERT_EQ(catalog.size(), 4u);
   EXPECT_EQ(catalog[0].categoryId, QStringLiteral("REF"));
   EXPECT_EQ(catalog[0].productId, QStringLiteral("SDR"));
   EXPECT_EQ(catalog[0].awipsId, QStringLiteral("N0B"));
   EXPECT_EQ(catalog[1].categoryId, QStringLiteral("VEL"));
   EXPECT_EQ(catalog[2].categoryId, QStringLiteral("CC"));
   EXPECT_EQ(catalog[3].categoryId, QStringLiteral("ACC"));
   EXPECT_EQ(catalog[3].description,
             QStringLiteral("Digital Accumulation Array"));
}

TEST(ProductDescriptor, LevelTwoAndLevelThreeIdentitiesCannotCollide)
{
   ProductDescriptor level2;
   level2.sourceKey = QStringLiteral("KLSX");
   level2.product   = QStringLiteral("Reflectivity");

   ProductDescriptor level3 = level2;
   level3.identityKind      = ProductDescriptor::IdentityKind::Level3Awips;
   level3.identity          = QStringLiteral("N0B");

   EXPECT_NE(level2, level3);
}

} // namespace
} // namespace wxlens::products
