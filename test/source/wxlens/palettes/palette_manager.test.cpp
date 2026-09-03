#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <gtest/gtest.h>
#include <QFile>
#include <QTemporaryDir>

namespace wxlens
{
namespace palettes
{
namespace
{
TEST(PaletteManager, ImportedPaletteCanBeReselected)
{
   QTemporaryDir directory;
   const QString path = directory.filePath("custom.pal");
   QFile file(path);
   ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("Product: BR\nColor: 0 1 2 3\nColor: 10 4 5 6\n");
   file.close();

   PaletteManager manager;
   ASSERT_TRUE(manager.openFile(QUrl::fromLocalFile(path)));
   const QString importedName = manager.activeName();
   const QString importedText = manager.activeText();
   ASSERT_TRUE(manager.select("DR"));
   ASSERT_TRUE(manager.select(importedName));
   EXPECT_EQ(manager.activeText(), importedText);
}

TEST(PaletteManager, ConfirmationDoesNotAllowPendingActionToBeReplaced)
{
   PaletteManager manager;
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(1, 2, 3)));
   bool closed {false};
   QObject::connect(&manager, &PaletteManager::closeRequested, [&closed]() { closed = true; });
   manager.requestClose();
   ASSERT_TRUE(manager.confirmationRequired());
   manager.requestResetAll();
   manager.resolveUnsavedChanges(PaletteManager::UnsavedDecision::Discard);
   EXPECT_TRUE(closed);
}

TEST(PaletteManager, EditsRemainDraftUntilApplied)
{
   PaletteManager manager;
   const QString original = manager.paletteText(QStringLiteral("DR"));
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(1, 2, 3)));
   EXPECT_EQ(manager.paletteText(QStringLiteral("DR")), original);

   int appliedCount {0};
   QObject::connect(&manager, &PaletteManager::paletteApplied,
                    [&appliedCount](const QString&) { ++appliedCount; });
   manager.applyActive();

   EXPECT_NE(manager.paletteText(QStringLiteral("DR")), original);
   EXPECT_EQ(appliedCount, 1);
}

TEST(PaletteManager, FamiliesGroupPresentationVariantsOfOneField)
{
   EXPECT_EQ(PaletteManager::FamilyOf(QStringLiteral("SRV")), QStringLiteral("DV"));
   EXPECT_EQ(PaletteManager::FamilyOf(QStringLiteral("KDP2")), QStringLiteral("KDP"));
   EXPECT_EQ(PaletteManager::FamilyOf(QStringLiteral("DR")), QStringLiteral("DR"));
   EXPECT_TRUE(PaletteManager::FamilyOf(QStringLiteral("not a palette")).isEmpty())
      << "imported palettes have no family until they declare a field";
   EXPECT_EQ(PaletteManager::FamilyMembers(QStringLiteral("DV")),
             (QStringList {QStringLiteral("DV"), QStringLiteral("SRV")}));
   EXPECT_EQ(PaletteManager::FamilyMembers(QStringLiteral("DR")), QStringList {QStringLiteral("DR")});
   EXPECT_TRUE(PaletteManager::FamilyMembers(QStringLiteral("SRV")).isEmpty())
      << "a variant is not itself a family id";
}

TEST(PaletteManager, ApplyingAPaletteMakesItItsFamilysDefault)
{
   PaletteManager manager;
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   EXPECT_TRUE(manager.isFamilyDefault(QStringLiteral("DV"))) << "bundled default until chosen";
   EXPECT_FALSE(manager.isFamilyDefault(QStringLiteral("SRV")));

   int familyChanges {0};
   QObject::connect(&manager, &PaletteManager::familyDefaultsChanged, [&]() { ++familyChanges; });
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   EXPECT_EQ(familyChanges, 0) << "looking at a palette must not change any default";
   manager.applyActive();

   EXPECT_EQ(manager.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));
   EXPECT_TRUE(manager.isFamilyDefault(QStringLiteral("SRV")));
   EXPECT_FALSE(manager.isFamilyDefault(QStringLiteral("DV")));
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DR")).isEmpty()) << "other families untouched";
   EXPECT_TRUE(manager.familyDefaultNames().contains(QStringLiteral("SRV")));
   EXPECT_FALSE(manager.familyDefaultNames().contains(QStringLiteral("DV")));
   EXPECT_EQ(familyChanges, 1);

   manager.applyActive();
   EXPECT_EQ(familyChanges, 1) << "re-applying the current default is not a default change";
}

TEST(PaletteManager, FamilyDefaultRejectsPalettesFromAnotherField)
{
   PaletteManager manager;
   manager.setFamilyDefault(QStringLiteral("DV"), QStringLiteral("DR"));
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   manager.setFamilyDefault(QStringLiteral("SRV"), QStringLiteral("SRV"));
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("SRV")).isEmpty()) << "not a family id";
   manager.setFamilyDefault(QStringLiteral("DV"), QStringLiteral("SRV"));
   EXPECT_EQ(manager.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));
   manager.setFamilyDefault(QStringLiteral("DV"), QString {});
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
}

TEST(PaletteManager, FamilyDefaultsPersistAndAreValidatedOnLoad)
{
   QTemporaryDir directory;
   ASSERT_TRUE(directory.isValid());
   settings::SettingsStore store;
   store.SetConfigDirectory(directory.path());

   {
      PaletteManager manager;
      manager.bindSettings(store);
      ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
      manager.applyActive();
   }

   settings::SettingsStore reopened;
   reopened.SetConfigDirectory(directory.path());
   PaletteManager restored;
   restored.bindSettings(reopened);
   EXPECT_EQ(restored.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));

   // A hand-edited file naming a reflectivity ramp as the velocity default is ignored, not obeyed.
   reopened.SetString(QStringLiteral("palettes"), QStringLiteral("family_default_DV"),
                      QStringLiteral("DR"));
   ASSERT_TRUE(reopened.Save());
   settings::SettingsStore tampered;
   tampered.SetConfigDirectory(directory.path());
   PaletteManager validated;
   validated.bindSettings(tampered);
   EXPECT_TRUE(validated.familyDefault(QStringLiteral("DV")).isEmpty());
}

TEST(PaletteManager, ResetAllRestoresAppliedTextsAndFamilyDefaults)
{
   PaletteManager manager;
   const QString factoryDr  = manager.paletteText(QStringLiteral("DR"));
   const QString factorySrv = manager.paletteText(QStringLiteral("SRV"));

   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(1, 2, 3)));
   manager.applyActive();
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(4, 5, 6)));
   manager.applyActive();
   ASSERT_NE(manager.paletteText(QStringLiteral("DR")), factoryDr);
   ASSERT_EQ(manager.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));

   manager.requestResetAll();
   if (manager.confirmationRequired())
      manager.resolveUnsavedChanges(PaletteManager::UnsavedDecision::Discard);

   EXPECT_EQ(manager.paletteText(QStringLiteral("DR")), factoryDr);
   EXPECT_EQ(manager.paletteText(QStringLiteral("SRV")), factorySrv);
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   EXPECT_EQ(manager.activeName(), QStringLiteral("SRV")) << "stays on what the user was viewing";
}
} // namespace
} // namespace palettes
} // namespace wxlens
