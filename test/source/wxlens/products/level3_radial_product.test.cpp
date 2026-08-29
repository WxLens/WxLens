#include <wxlens/products/level3_radial_product.hpp>

#include <scwx/wsr88d/level3_file.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace wxlens
{
namespace products
{
namespace
{
std::filesystem::path Fixture(const char* name)
{
   return std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
          name;
}

TEST(Level3RadialProduct, ConvertsDigitalReflectivityIntoRendererContract)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(Fixture("LSX_N0B_2022_03_30_15_40_41").string()));

   const auto result = BuildLevel3RadialSnapshot(file);
   ASSERT_TRUE(result.has_value());
   ASSERT_NE(result->sweep, nullptr);
   EXPECT_FALSE(result->sweep->vertices.empty());
   EXPECT_EQ(result->sweep->vertices.size(),
             result->sweep->dataMoments8.size() * 2u);
   EXPECT_EQ(result->sweep->dataMoments8.size() % 6u, 0u);
   EXPECT_GT(result->metadata.threshold, 0u);
   EXPECT_FALSE(result->metadata.defaultPalette.empty());
   EXPECT_NE(result->metadata.productTime.time_since_epoch().count(), 0);
}

class RepresentativeLevel3RadialProduct :
    public testing::TestWithParam<const char*>
{
};

TEST_P(RepresentativeLevel3RadialProduct,
       PreservesDecodedMetadataAndRenderableGeometry)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(Fixture(GetParam()).string()));

   const auto result = BuildLevel3RadialSnapshot(file);
   ASSERT_TRUE(result.has_value()) << GetParam();
   ASSERT_NE(result->sweep, nullptr);
   EXPECT_FALSE(result->sweep->vertices.empty());
   EXPECT_EQ(result->sweep->vertices.size(),
             result->sweep->dataMoments8.size() * 2u);
   EXPECT_FALSE(result->metadata.defaultPalette.empty());

   bool hasDecodedValue = false;
   bool hasDecodedCode  = false;
   for (std::size_t i = 0; i < result->metadata.decodedValues.size(); ++i)
   {
      hasDecodedValue |= result->metadata.decodedValues[i].has_value();
      hasDecodedCode |= result->metadata.decodedCodes[i].has_value();
   }
   EXPECT_TRUE(hasDecodedValue || hasDecodedCode);
}

INSTANTIATE_TEST_SUITE_P(VelocityDualPolAndCategorical,
                         RepresentativeLevel3RadialProduct,
                         testing::Values("Level3_LSX_N1U_20211228_0446.nids",
                                         "KLSX_SDUS83_N0XLSX_202112110212",
                                         "KLSX_SDUS83_N0HLSX_202112110212"));

TEST(Level3RadialProduct, RejectsNonRadialProduct)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(
      file.LoadFile(Fixture("KLSX_SDUS33_NSTLSX_202112110215").string()));
   EXPECT_FALSE(BuildLevel3RadialSnapshot(file).has_value());
}
} // namespace
} // namespace products
} // namespace wxlens
