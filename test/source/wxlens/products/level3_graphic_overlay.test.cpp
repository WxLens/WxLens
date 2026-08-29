#include <wxlens/products/level3_graphic_overlay.hpp>

#include <scwx/wsr88d/level3_file.hpp>
#include <gtest/gtest.h>

#include <filesystem>

namespace wxlens { namespace products { namespace {

TEST(Level3GraphicOverlay, ConvertsRealStormTrackingFixture)
{
   const auto path = std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
                     "KLSX_SDUS33_NSTLSX_202112110215";
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(path.string()));
   const auto snapshot = BuildLevel3GraphicOverlaySnapshot(file);
   ASSERT_TRUE(snapshot.has_value());
   EXPECT_NE(snapshot->productTime.time_since_epoch().count(), 0);
   EXPECT_FALSE(snapshot->primitives.empty());
   EXPECT_TRUE(std::any_of(snapshot->primitives.begin(), snapshot->primitives.end(),
      [](const auto& p) { return p.kind == GraphicOverlayKind::StormId && !p.stormId.empty(); }));
   EXPECT_TRUE(std::any_of(snapshot->primitives.begin(), snapshot->primitives.end(),
      [](const auto& p) { return p.kind == GraphicOverlayKind::Polyline && p.geometry.size() > 1; }));
}

TEST(Level3GraphicOverlay, RejectsRadialProduct)
{
   const auto path = std::filesystem::path {SCWX_TEST_DATA_DIR} / "nexrad" / "level3" /
                     "LSX_N0B_2022_03_30_15_40_41";
   scwx::wsr88d::Level3File file;
   ASSERT_TRUE(file.LoadFile(path.string()));
   EXPECT_FALSE(BuildLevel3GraphicOverlaySnapshot(file).has_value());
}

} } } // namespace wxlens::products
