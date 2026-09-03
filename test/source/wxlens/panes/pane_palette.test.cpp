// Palette-ownership tests at the renderer seam (docs/phase1-ux-feedback-2026-08-31.md, P0
// "Product-aware palette ownership and defaults").
//
// These compare the colour LUTs panes actually hand to their radar layer, not just palette names:
// the failure mode being guarded against - a velocity edit recolouring a reflectivity pane, or an
// applied edit silently not reaching the panes it should - is invisible to a name comparison,
// because the names were already right when the bug was found.

#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/panes/sync_types.hpp>
#include <wxlens/products/radar_sweep_product.hpp>
#include <wxlens/render/radar_sweep_layer.hpp>

#include <memory>

#include <QColor>
#include <gtest/gtest.h>

namespace wxlens::panes::test
{

namespace
{

using palettes::PaletteManager;
using products::BuildColorTableLut;
using products::ColorTableLut;
using products::SweepData;
using products::SweepSnapshot;

/// A three-gate sweep with real Level 2 encoding parameters - reflectivity's offset/scale, or
/// velocity's plus the m/s unit that the LUT builder converts onto the knots ramps.
std::shared_ptr<const SweepData> MakeSweep(bool velocity)
{
   auto sweep              = std::make_shared<SweepData>();
   sweep->vertices         = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
   sweep->dataMoments8     = {2, 100, 200};
   sweep->dataMomentOffset = velocity ? 129.0f : 66.0f;
   sweep->dataMomentScale  = 2.0f;
   if (velocity) sweep->dataMomentUnits = "M/S";
   return sweep;
}

std::shared_ptr<const ColorTableLut> ExpectedLut(const std::shared_ptr<const SweepData>& sweep,
                                                 const QString&                          palette)
{
   return BuildColorTableLut(*sweep, PaletteManager::Instance().paletteText(palette));
}

bool SameColors(const std::shared_ptr<const ColorTableLut>& a,
                const std::shared_ptr<const ColorTableLut>& b)
{
   return a != nullptr && b != nullptr && a->colors == b->colors;
}

class PanePaletteTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // An empty source key keeps PaneController from binding a real radar product, so the sweep
      // each pane renders is the synthetic one installed below and nothing touches the network.
      model_.setDefaultSourceKey(QString {});
      model_.setGridSize(1, 2);
      ResetManager();
   }

   void TearDown() override { ResetManager(); }

   /// PaletteManager is process-wide, so leave it as found: factory texts, no family defaults,
   /// nothing dirty in the editor.
   static void ResetManager()
   {
      auto& manager = PaletteManager::Instance();
      if (manager.confirmationRequired())
         manager.resolveUnsavedChanges(PaletteManager::UnsavedDecision::Discard);
      manager.requestResetAll();
      if (manager.confirmationRequired())
         manager.resolveUnsavedChanges(PaletteManager::UnsavedDecision::Discard);
      manager.select(QStringLiteral("DR"));
   }

   [[nodiscard]] PaneController* Pane(int row) const
   {
      const QModelIndex index = model_.index(row, 0);
      return qvariant_cast<PaneController*>(model_.data(index, PaneGridModel::PaneRole));
   }

   /// Installs a sweep on the pane's layer binding and bakes its LUT the way an arriving sweep
   /// would - by applying the pane's current effective palette to it.
   static void GiveSweep(PaneController* pane, const std::shared_ptr<const SweepData>& sweep)
   {
      pane->layerBinding()->setSnapshot(SweepSnapshot {sweep, nullptr});
      auto& manager = PaletteManager::Instance();
      manager.select(pane->effectivePaletteName());
      manager.applyActive();
   }

   static std::shared_ptr<const ColorTableLut> Lut(PaneController* pane)
   {
      return pane->layerBinding()->snapshot().colorTableLut;
   }

   PaneGridModel model_;
};

} // namespace

TEST_F(PanePaletteTest, ReflectivityAndVelocityPanesRenderFromTheirOwnFamilies)
{
   const auto reflectivity = MakeSweep(false);
   const auto velocity     = MakeSweep(true);
   Pane(1)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), reflectivity);
   GiveSweep(Pane(1), velocity);

   EXPECT_EQ(Pane(0)->effectivePaletteName(), QStringLiteral("DR"));
   EXPECT_EQ(Pane(1)->effectivePaletteName(), QStringLiteral("DV"));
   EXPECT_TRUE(SameColors(Lut(Pane(0)), ExpectedLut(reflectivity, QStringLiteral("DR"))));
   EXPECT_TRUE(SameColors(Lut(Pane(1)), ExpectedLut(velocity, QStringLiteral("DV"))));
   EXPECT_FALSE(SameColors(Lut(Pane(0)), Lut(Pane(1))))
      << "the two families must bake visibly different LUTs or the rest of this file proves nothing";
}

TEST_F(PanePaletteTest, ApplyingAVelocityPaletteNeverRecoloursReflectivity)
{
   const auto reflectivity = MakeSweep(false);
   const auto velocity     = MakeSweep(true);
   Pane(1)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), reflectivity);
   GiveSweep(Pane(1), velocity);
   const auto reflectivityBefore = Lut(Pane(0));

   auto& manager = PaletteManager::Instance();
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   manager.applyActive();

   EXPECT_EQ(Pane(1)->effectivePaletteName(), QStringLiteral("SRV"));
   EXPECT_TRUE(SameColors(Lut(Pane(1)), ExpectedLut(velocity, QStringLiteral("SRV"))));
   EXPECT_EQ(Pane(0)->effectivePaletteName(), QStringLiteral("DR"));
   EXPECT_TRUE(SameColors(Lut(Pane(0)), reflectivityBefore))
      << "a velocity-family apply must not touch a reflectivity pane's LUT";

   // Editing velocity's ramp, likewise.
   ASSERT_TRUE(manager.select(QStringLiteral("DV")));
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(250, 10, 10)));
   manager.applyActive();
   EXPECT_TRUE(SameColors(Lut(Pane(0)), reflectivityBefore));
}

TEST_F(PanePaletteTest, AppliedEditToAFamilyDefaultReachesPanesWithoutAnOverride)
{
   // The regression this file exists for: a pane with no explicit palette used to fall back to
   // the shared product's factory LUT, so "Apply to product" on an edited reflectivity palette
   // changed nothing on screen.
   const auto reflectivity = MakeSweep(false);
   GiveSweep(Pane(0), reflectivity);
   const auto before = Lut(Pane(0));
   ASSERT_TRUE(Pane(0)->paletteName().isEmpty()) << "must be exercising the no-override path";

   auto& manager = PaletteManager::Instance();
   ASSERT_TRUE(manager.select(QStringLiteral("DR")));
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(1, 2, 3)));
   EXPECT_TRUE(SameColors(Lut(Pane(0)), before)) << "an unapplied draft must not render";

   manager.applyActive();
   EXPECT_FALSE(SameColors(Lut(Pane(0)), before));
   EXPECT_TRUE(SameColors(Lut(Pane(0)), ExpectedLut(reflectivity, QStringLiteral("DR"))));
}

TEST_F(PanePaletteTest, ExplicitPaneOverrideSurvivesAFamilyDefaultChange)
{
   const auto velocity = MakeSweep(true);
   Pane(0)->setProductName(QStringLiteral("Velocity"));
   Pane(1)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), velocity);
   GiveSweep(Pane(1), velocity);
   Pane(1)->setPaletteName(QStringLiteral("DV"));

   auto& manager = PaletteManager::Instance();
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   manager.applyActive();

   EXPECT_EQ(manager.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));
   EXPECT_EQ(Pane(0)->effectivePaletteName(), QStringLiteral("SRV"))
      << "a pane following the family default follows the new default";
   EXPECT_TRUE(SameColors(Lut(Pane(0)), ExpectedLut(velocity, QStringLiteral("SRV"))));
   EXPECT_EQ(Pane(1)->paletteName(), QStringLiteral("DV"));
   EXPECT_EQ(Pane(1)->effectivePaletteName(), QStringLiteral("DV"))
      << "an explicit override stays local";
   EXPECT_TRUE(SameColors(Lut(Pane(1)), ExpectedLut(velocity, QStringLiteral("DV"))));
}

TEST_F(PanePaletteTest, PaletteSyncChannelRebuildsTheReceivingPanesLut)
{
   const auto velocity = MakeSweep(true);
   Pane(0)->setProductName(QStringLiteral("Velocity"));
   Pane(1)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), velocity);
   GiveSweep(Pane(1), velocity);
   Pane(0)->setSyncGroup(SyncChannel::Palette, 3);
   Pane(1)->setSyncGroup(SyncChannel::Palette, 3);

   Pane(0)->setPaletteName(QStringLiteral("SRV"));

   EXPECT_EQ(Pane(1)->paletteName(), QStringLiteral("SRV"));
   EXPECT_TRUE(SameColors(Lut(Pane(1)), ExpectedLut(velocity, QStringLiteral("SRV"))))
      << "the linked pane must repaint from the synced palette, not just record its name";

   // And with the channel unlinked, the same gesture stays local.
   Pane(1)->setSyncGroup(SyncChannel::Palette, kNoSyncGroup);
   Pane(0)->setPaletteName(QStringLiteral("DV"));
   EXPECT_EQ(Pane(1)->paletteName(), QStringLiteral("SRV"));
   EXPECT_TRUE(SameColors(Lut(Pane(1)), ExpectedLut(velocity, QStringLiteral("SRV"))));
}

TEST_F(PanePaletteTest, SelectingAPaletteInTheEditorDoesNotRecolourAnyPane)
{
   const auto reflectivity = MakeSweep(false);
   const auto velocity     = MakeSweep(true);
   Pane(1)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), reflectivity);
   GiveSweep(Pane(1), velocity);
   const auto reflectivityBefore = Lut(Pane(0));
   const auto velocityBefore     = Lut(Pane(1));

   auto& manager = PaletteManager::Instance();
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   ASSERT_TRUE(manager.select(QStringLiteral("DR")));

   EXPECT_TRUE(SameColors(Lut(Pane(0)), reflectivityBefore));
   EXPECT_TRUE(SameColors(Lut(Pane(1)), velocityBefore));
   EXPECT_EQ(Pane(1)->effectivePaletteName(), QStringLiteral("DV"));
}

TEST_F(PanePaletteTest, ResetAllRestoresFactoryRenderingAndFamilyDefaults)
{
   const auto velocity = MakeSweep(true);
   Pane(0)->setProductName(QStringLiteral("Velocity"));
   GiveSweep(Pane(0), velocity);
   const auto factory = Lut(Pane(0));

   auto& manager = PaletteManager::Instance();
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(9, 9, 9)));
   manager.applyActive();
   ASSERT_EQ(Pane(0)->effectivePaletteName(), QStringLiteral("SRV"));
   ASSERT_FALSE(SameColors(Lut(Pane(0)), factory));

   manager.requestResetAll();
   if (manager.confirmationRequired())
      manager.resolveUnsavedChanges(PaletteManager::UnsavedDecision::Discard);

   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   EXPECT_EQ(Pane(0)->effectivePaletteName(), QStringLiteral("DV"));
   EXPECT_TRUE(SameColors(Lut(Pane(0)), factory));
}

} // namespace wxlens::panes::test
