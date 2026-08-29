#include <wxlens/products/level3_text_product.hpp>

#include <scwx/wsr88d/level3_file.hpp>
#include <gtest/gtest.h>

#include <filesystem>

namespace wxlens { namespace products { namespace {

TEST(Level3TextProduct, PreservesGraphicAnnotationsWithoutInventingGeography)
{
   const auto path = std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
                     "KLSX_SDUS33_NSTLSX_202112110215";
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(path.string()));
   const auto snapshot = BuildLevel3TextSnapshot(file);
   ASSERT_TRUE(snapshot.has_value());
   EXPECT_TRUE(snapshot->productTime.has_value());
   EXPECT_TRUE(snapshot->graphicBlockAvailable);
   ASSERT_FALSE(snapshot->graphicPages.empty());
   EXPECT_TRUE(std::any_of(snapshot->graphicPages.begin(), snapshot->graphicPages.end(),
      [](const auto& page) { return !page.entries.empty() && page.entries.front().rawI.has_value(); }));
   EXPECT_NE(snapshot->rawMetadataNote.find("not geographic"), std::string::npos);
}

TEST(Level3TextProduct, PreservesRepresentativeTabularPages)
{
   const auto path = std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
                     "KLSX_SDUS63_SPDLSX_202112110114";
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(path.string()));
   const auto snapshot = BuildLevel3TextSnapshot(file);
   ASSERT_TRUE(snapshot.has_value());
   EXPECT_TRUE(snapshot->tabularBlockAvailable);
   ASSERT_FALSE(snapshot->tabularPages.empty());
   EXPECT_TRUE(std::any_of(snapshot->tabularPages.begin(), snapshot->tabularPages.end(),
      [](const auto& page) { return !page.entries.empty() && !page.entries.front().text.empty(); }));
}

TEST(Level3TextProduct, ReportsUnavailableBlocksHonestly)
{
   const auto path = std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
                     "LSX_N0B_2022_03_30_15_40_41";
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(path.string()));
   const auto snapshot = BuildLevel3TextSnapshot(file);
   ASSERT_TRUE(snapshot.has_value());
   EXPECT_FALSE(snapshot->graphicBlockAvailable);
   EXPECT_FALSE(snapshot->tabularBlockAvailable);
   EXPECT_TRUE(snapshot->graphicPages.empty());
   EXPECT_TRUE(snapshot->tabularPages.empty());
}

} } } // namespace wxlens::products
