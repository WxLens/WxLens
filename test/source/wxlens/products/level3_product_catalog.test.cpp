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
   EXPECT_EQ(catalog[0].categoryDescription,
             QStringLiteral("Reflectivity Products"));
   EXPECT_EQ(catalog[0].productId, QStringLiteral("SDR"));
   EXPECT_EQ(catalog[0].awipsId, QStringLiteral("N0B"));
   EXPECT_EQ(catalog[0].family, QStringLiteral("Super-Resolution Reflectivity"));
   EXPECT_TRUE(catalog[0].recommended);
   EXPECT_EQ(catalog[1].categoryId, QStringLiteral("VEL"));
   EXPECT_EQ(catalog[2].categoryId, QStringLiteral("CC"));
   EXPECT_EQ(catalog[3].categoryId, QStringLiteral("ACC"));
   EXPECT_EQ(catalog[3].description,
             QStringLiteral("Digital Accumulation Array"));

   // Every entry, not just the first: a family/recommended bug that only showed up for one
   // category would otherwise pass. Each of these ids is the sole available variant of its
   // family here, so each is its own family's recommendation.
   for (const auto& entry : catalog)
   {
      EXPECT_EQ(entry.family, entry.description)
         << entry.awipsId.toStdString() << " should group under its own description";
      EXPECT_TRUE(entry.recommended)
         << entry.awipsId.toStdString() << " is the only available variant of its family";
      // Not yet wired through - see Level3ProductDescriptor. Pinned so the day it is populated,
      // whoever does it updates this test deliberately rather than by accident.
      EXPECT_TRUE(entry.elevationAngleText.isEmpty());
      EXPECT_FALSE(entry.elevationIsExact);
   }

   // Velocity products and spectrum width share one heading with the Level 2 moments that belong
   // beside them (see ProductSectionId) - a regrouping that broke that would show here.
   EXPECT_EQ(catalog[1].categoryDescription, QStringLiteral("Velocity Products"));
   EXPECT_EQ(catalog[2].categoryDescription, QStringLiteral("Dual-Polarization"));
   EXPECT_EQ(catalog[3].categoryDescription, QStringLiteral("Precipitation Accumulation"));
}

TEST(Level3ProductCatalog, OnlyTheFirstAvailableVariantOfAFamilyIsRecommended)
{
   // Two AWIPS ids of the same product family (base reflectivity, 0.5 and 0.9 degree tilts): the
   // picker collapses them under one row, so exactly one must be marked as the one to show.
   const auto catalog = BuildLevel3ProductCatalog({"N0B", "N1B"});

   ASSERT_EQ(catalog.size(), 2u);
   EXPECT_EQ(catalog[0].family, catalog[1].family)
      << "both tilts belong to one family";
   EXPECT_TRUE(catalog[0].recommended);
   EXPECT_FALSE(catalog[1].recommended)
      << "a second entry in the same family must not also claim to be the default";
}

TEST(ProductSection, LevelTwoAndLevelThreeVelocityShareOneHeading)
{
   // The reason ProductSectionId exists: PaneController's Level 2 table and the Level 3 catalog
   // must name the same heading for products that belong together, rather than each spelling a
   // string that can drift.
   EXPECT_EQ(ProductSectionName(ProductSectionId::Velocity),
             QStringLiteral("Velocity Products"));
   EXPECT_EQ(ProductSectionName(ProductSectionId::Reflectivity),
             QStringLiteral("Reflectivity Products"));
   EXPECT_EQ(ProductSectionName(ProductSectionId::DualPolarization),
             QStringLiteral("Dual-Polarization"));
   EXPECT_EQ(ProductSectionName(ProductSectionId::PrecipitationAccumulation),
             QStringLiteral("Precipitation Accumulation"));
   EXPECT_EQ(ProductSectionName(ProductSectionId::Other), QStringLiteral("Other Products"));
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
