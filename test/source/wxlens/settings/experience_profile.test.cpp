// Tests for experience levels (ADR 0005): the one level-to-key table, overrides held only as
// differences from the level, the one-time migration of the pre-ADR pane-linking toggle, and the
// display-only pane cap that keeps a stored layout while multiple panes are hidden.

#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/settings/app_settings.hpp>
#include <wxlens/settings/experience_profile.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <memory>
#include <set>

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace wxlens::settings::test
{

namespace
{
const QString kMulti   = QStringLiteral("panes.multi");
const QString kSync    = QStringLiteral("panes.sync_controls");
const QString kDrawing = QStringLiteral("tools.drawing");
const QString kMarkers = QStringLiteral("tools.markers");

// Counts emissions with a plain connection, like app_settings.test.cpp, so the suite does not need
// Qt6::Test.
template<typename Sender, typename Signal>
std::shared_ptr<int> Count(Sender* sender, Signal signal)
{
   auto count = std::make_shared<int>(0);
   QObject::connect(sender, signal, [count]() { ++*count; });
   return count;
}
} // namespace

TEST(ExperienceProfile, LevelsRevealCumulatively)
{
   // Every key is revealed from its lowest level upward - a higher level never hides something a
   // lower one shows. That is the "layer capability, never remove it" principle.
   for (const RevealKey& key : ExperienceProfile::Keys())
   {
      const QString id = QString::fromLatin1(key.id);
      bool          seen = false;
      for (int level = 0; level <= 2; ++level)
      {
         const bool revealed = ExperienceProfile::LevelDefault(static_cast<ExperienceLevel>(level), id);
         EXPECT_FALSE(seen && !revealed) << key.id;
         seen = seen || revealed;
      }
      EXPECT_TRUE(ExperienceProfile::LevelDefault(ExperienceLevel::Professional, id)) << key.id;
   }
}

TEST(ExperienceProfile, TableMatchesAdr0005)
{
   using L = ExperienceLevel;
   EXPECT_FALSE(ExperienceProfile::LevelDefault(L::Basic, kMulti));
   EXPECT_TRUE(ExperienceProfile::LevelDefault(L::Advanced, kMulti));
   // Advanced panes always move together; per-channel linking is Professional.
   EXPECT_FALSE(ExperienceProfile::LevelDefault(L::Advanced, kSync));
   EXPECT_TRUE(ExperienceProfile::LevelDefault(L::Professional, kSync));
   EXPECT_TRUE(ExperienceProfile::LevelDefault(L::Advanced, kMarkers));
   EXPECT_FALSE(ExperienceProfile::LevelDefault(L::Advanced, kDrawing));
}

TEST(ExperienceProfile, KeysAreUniqueAndPresentable)
{
   std::set<QString> ids;
   for (const RevealKey& key : ExperienceProfile::Keys())
   {
      EXPECT_TRUE(ids.insert(QString::fromLatin1(key.id)).second) << key.id;
      EXPECT_FALSE(QString::fromLatin1(key.label).isEmpty()) << key.id;
      EXPECT_FALSE(QString::fromLatin1(key.description).isEmpty()) << key.id;
   }
}

TEST(ExperienceProfile, UnknownKeysAreRevealedAndCannotBeOverridden)
{
   ExperienceProfile profile;
   EXPECT_TRUE(profile.IsRevealed(QStringLiteral("not.a.key")));
   EXPECT_FALSE(profile.SetRevealed(QStringLiteral("not.a.key"), false));
   EXPECT_FALSE(profile.HasOverrides());
}

TEST(ExperienceProfile, OverridesAreOnlyDifferencesFromTheLevel)
{
   ExperienceProfile profile;
   ASSERT_EQ(profile.Level(), ExperienceLevel::Basic);

   EXPECT_FALSE(profile.SetRevealed(kMulti, false)); // already the Basic default
   EXPECT_FALSE(profile.HasOverrides());

   EXPECT_TRUE(profile.SetRevealed(kMulti, true));
   EXPECT_TRUE(profile.IsRevealed(kMulti));
   EXPECT_TRUE(profile.HasOverrides());

   // Toggling back to the level default removes the override instead of storing a redundant one.
   EXPECT_TRUE(profile.SetRevealed(kMulti, false));
   EXPECT_FALSE(profile.HasOverrides());
}

TEST(ExperienceProfile, PickingALevelClearsOverrides)
{
   ExperienceProfile profile;
   profile.SetRevealed(kDrawing, true);
   EXPECT_TRUE(profile.SetLevel(ExperienceLevel::Advanced));
   EXPECT_FALSE(profile.HasOverrides());
   EXPECT_FALSE(profile.IsRevealed(kDrawing));

   // Re-picking the current level is "Reset to <level> defaults".
   profile.SetRevealed(kMulti, false);
   EXPECT_TRUE(profile.SetLevel(ExperienceLevel::Advanced));
   EXPECT_TRUE(profile.IsRevealed(kMulti));
   EXPECT_FALSE(profile.SetLevel(ExperienceLevel::Advanced));
}

class ExperienceSettingsTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(dir_.isValid());
      store_.SetConfigDirectory(dir_.path());
   }

   QTemporaryDir dir_;
   SettingsStore store_;
};

TEST_F(ExperienceSettingsTest, NewInstallStartsAtBasic)
{
   AppSettings settings {store_};
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Basic));
   EXPECT_FALSE(settings.experienceCustomized());
   EXPECT_FALSE(settings.isRevealed(kMulti));
   EXPECT_FALSE(settings.reveals()[kMulti].toBool());
   EXPECT_EQ(settings.experienceLevelNames(),
             (QStringList {"Basic", "Advanced", "Professional"}));
}

TEST_F(ExperienceSettingsTest, ExistingInstallStartsAtProfessionalAndKeepsLegacyLinking)
{
   // A config written by a pre-ADR-0005 build: a saved workspace, pane linking turned on.
   store_.SetInt(QStringLiteral("workspace"), QStringLiteral("width"), 2);
   store_.SetBool(QStringLiteral("appearance"), QStringLiteral("advanced_pane_linking"), true);
   ASSERT_TRUE(store_.Save());

   AppSettings settings {store_};
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Professional));
   EXPECT_TRUE(settings.advancedPaneLinking());
   EXPECT_FALSE(settings.experienceCustomized()); // on is Professional's own default
   EXPECT_TRUE(settings.isRevealed(kDrawing));
   // Migrated once: the legacy key is gone, so it cannot fight the new setting later.
   EXPECT_FALSE(store_.Contains(QStringLiteral("appearance"),
                                QStringLiteral("advanced_pane_linking")));
}

TEST_F(ExperienceSettingsTest, ExistingInstallWithLinkingOffBecomesAnOverride)
{
   store_.SetInt(QStringLiteral("workspace"), QStringLiteral("width"), 1);
   ASSERT_TRUE(store_.Save());

   AppSettings settings {store_};
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Professional));
   EXPECT_FALSE(settings.advancedPaneLinking());
   EXPECT_TRUE(settings.experienceCustomized());
   EXPECT_TRUE(settings.isRevealed(kMulti));
}

TEST_F(ExperienceSettingsTest, LevelAndOverridesPersist)
{
   {
      AppSettings settings {store_};
      settings.setExperienceLevel(static_cast<int>(ExperienceLevel::Advanced));
      settings.setRevealed(kDrawing, true);
   }
   SettingsStore reopened;
   reopened.SetConfigDirectory(dir_.path());
   AppSettings settings {reopened};
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Advanced));
   EXPECT_TRUE(settings.isRevealed(kDrawing));
   EXPECT_TRUE(settings.experienceCustomized());

   settings.resetExperienceOverrides();
   EXPECT_FALSE(settings.isRevealed(kDrawing));
   EXPECT_FALSE(reopened.Contains(QStringLiteral("experience"), QStringLiteral("reveal.tools.drawing")));
}

TEST_F(ExperienceSettingsTest, InvalidValuesAreRefused)
{
   AppSettings settings {store_};
   auto changed = Count(&settings, &AppSettings::experienceChanged);
   settings.setExperienceLevel(7);
   settings.setExperienceLevel(-1);
   settings.setRevealed(QStringLiteral("not.a.key"), false);
   EXPECT_EQ(*changed, 0);
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Basic));
}

TEST_F(ExperienceSettingsTest, SyncControlsKeyDrivesAdvancedPaneLinking)
{
   AppSettings settings {store_};
   auto linking = Count(&settings, &AppSettings::advancedPaneLinkingChanged);
   settings.setExperienceLevel(static_cast<int>(ExperienceLevel::Advanced));
   EXPECT_EQ(*linking, 0); // hidden at Basic and at Advanced
   settings.setExperienceLevel(static_cast<int>(ExperienceLevel::Professional));
   EXPECT_EQ(*linking, 1);
   EXPECT_TRUE(settings.advancedPaneLinking());
   settings.setAdvancedPaneLinking(false);
   EXPECT_EQ(*linking, 2);
   EXPECT_TRUE(settings.experienceCustomized());
}

TEST_F(ExperienceSettingsTest, GlobalResetKeepsTheLevelButDropsOverrides)
{
   AppSettings settings {store_};
   settings.setExperienceLevel(static_cast<int>(ExperienceLevel::Professional));
   settings.setRevealed(kDrawing, false);
   settings.resetToDefaults();
   EXPECT_EQ(settings.experienceLevel(), static_cast<int>(ExperienceLevel::Professional));
   EXPECT_FALSE(settings.experienceCustomized());
   EXPECT_TRUE(settings.isRevealed(kDrawing));
}

TEST_F(ExperienceSettingsTest, ExperienceSectionIsAddressable)
{
   AppSettings settings {store_};
   EXPECT_TRUE(settings.hasSection(QStringLiteral("experience")));
}

TEST(ExperiencePaneCap, HidingMultiplePanesKeepsTheStoredLayout)
{
   panes::PaneGridModel model;
   model.setDefaultSourceKey(QStringLiteral("KEAX"));
   model.setGridSize(2, 2);
   model.setActivePaneIndex(3);
   ASSERT_EQ(model.rowCount(), 4);

   auto size = Count(&model, &panes::PaneGridModel::gridSizeChanged);
   model.setMultiPaneEnabled(false);
   EXPECT_EQ(*size, 1);
   EXPECT_EQ(model.gridWidth(), 1);
   EXPECT_EQ(model.gridHeight(), 1);
   EXPECT_EQ(model.layoutWidth(), 2);
   EXPECT_EQ(model.layoutHeight(), 2);
   EXPECT_EQ(model.activePaneIndex(), 0);
   EXPECT_EQ(model.rowCount(), 4); // the hidden panes and their state are retained
   model.setActivePaneIndex(2);
   EXPECT_EQ(model.activePaneIndex(), 0); // only the visible pane can be active

   model.setMultiPaneEnabled(true);
   EXPECT_EQ(model.gridWidth(), 2);
   EXPECT_EQ(model.gridHeight(), 2);
}

} // namespace wxlens::settings::test
