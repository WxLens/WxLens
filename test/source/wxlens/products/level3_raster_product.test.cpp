#include <wxlens/products/level3_raster_product.hpp>

#include <scwx/wsr88d/level3_file.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

namespace wxlens
{
namespace products
{
namespace
{
std::filesystem::path Fixture(const char* name)
{
   return std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" / name;
}

TEST(Level3RasterProduct, ConvertsClassicRasterIntoGeographicRendererContract)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(Fixture("KLSX_SDUS53_NCRLSX_202112110215").string()));

   const auto result = BuildLevel3RasterSnapshot(file);
   ASSERT_TRUE(result.has_value());
   ASSERT_NE(result->sweep, nullptr);
   EXPECT_EQ(DetectLevel3CartesianPacketFamily(file), Level3CartesianPacketFamily::Raster);
   EXPECT_FALSE(result->sweep->vertices.empty());
   EXPECT_EQ(result->sweep->vertices.size(), result->sweep->dataMoments8.size() * 2u);
   EXPECT_EQ(result->sweep->dataMoments8.size() % 6u, 0u);
   EXPECT_GT(result->metadata.rows, 0u);
   EXPECT_GT(result->metadata.maximumColumns, 0u);
   EXPECT_NE(result->metadata.productTime.time_since_epoch().count(), 0);

   for (std::size_t i = 0; i < result->sweep->vertices.size(); i += 2u)
   {
      EXPECT_GE(result->sweep->vertices[i], -90.0f);
      EXPECT_LE(result->sweep->vertices[i], 90.0f);
      EXPECT_GE(result->sweep->vertices[i + 1u], -180.0f);
      EXPECT_LE(result->sweep->vertices[i + 1u], 180.0f);
   }
}

TEST(Level3RasterProduct, DetectsArchivedPrecipitationArrayWithoutFabricatingGeometry)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(Fixture("KLSX_SDUS53_DPALSX_202112110238").string()));

   EXPECT_EQ(DetectLevel3CartesianPacketFamily(file),
             Level3CartesianPacketFamily::PrecipitationRateArray);
   EXPECT_FALSE(BuildLevel3RasterSnapshot(file).has_value());
}

TEST(Level3RasterProduct, RejectsRadialFamily)
{
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(Fixture("LSX_N0B_2022_03_30_15_40_41").string()));
   EXPECT_EQ(DetectLevel3CartesianPacketFamily(file), Level3CartesianPacketFamily::None);
   EXPECT_FALSE(BuildLevel3RasterSnapshot(file).has_value());
}

} // namespace
} // namespace products
} // namespace wxlens
