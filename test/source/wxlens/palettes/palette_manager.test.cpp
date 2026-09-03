#include <wxlens/palettes/palette_manager.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <gtest/gtest.h>
#include <QFile>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

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

TEST(PaletteManager, UnitsAreParsedAndCanonicalised)
{
   EXPECT_EQ(PaletteManager::CanonicalUnits(QStringLiteral(" kt ")), QStringLiteral("KT"));
   EXPECT_EQ(PaletteManager::CanonicalUnits(QStringLiteral("KTS")), QStringLiteral("KT"));
   // KDP declares DEG/KM and KDP2 declares DEG for the same field, so they must compare equal.
   EXPECT_EQ(PaletteManager::CanonicalUnits(QStringLiteral("DEG/KM")),
             PaletteManager::CanonicalUnits(QStringLiteral("deg")));
   EXPECT_TRUE(PaletteManager::CanonicalUnits(QString {}).isEmpty());

   EXPECT_EQ(PaletteManager::UnitsOfText(QStringLiteral("; comment\nUnits: dBZ\nColor: 0 1 2 3\n")),
             QStringLiteral("DBZ"));
   EXPECT_TRUE(PaletteManager::UnitsOfText(QStringLiteral("Color: 0 1 2 3\n")).isEmpty())
      << "HC and Default16 ship without units; that is not an error";

   PaletteManager manager;
   EXPECT_EQ(manager.unitsOf(QStringLiteral("DR")), QStringLiteral("DBZ"));
   EXPECT_EQ(manager.unitsOf(QStringLiteral("DV")), QStringLiteral("KT"));
}

TEST(PaletteManager, ImportIsUnlinkedUntilTheUserPicksAField)
{
   QTemporaryDir directory;
   const QString path = directory.filePath("test01.pal");
   QFile file(path);
   ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("Units: KT\nColor: -40 0 0 255\nColor: 40 255 0 0\n");
   file.close();

   PaletteManager manager;
   const QVariantMap preview = manager.inspectFile(QUrl::fromLocalFile(path));
   ASSERT_TRUE(preview.value(QStringLiteral("valid")).toBool());
   EXPECT_EQ(preview.value(QStringLiteral("name")).toString(), QStringLiteral("test01"));
   EXPECT_EQ(preview.value(QStringLiteral("units")).toString(), QStringLiteral("KT"));
   EXPECT_EQ(preview.value(QStringLiteral("stopCount")).toInt(), 2);
   const QVariantList candidates = preview.value(QStringLiteral("compatibleFamilies")).toList();
   QStringList candidateIds;
   for (const QVariant& value : candidates)
      candidateIds.append(value.toMap().value(QStringLiteral("id")).toString());
   EXPECT_TRUE(candidateIds.contains(QStringLiteral("DV")));
   EXPECT_FALSE(candidateIds.contains(QStringLiteral("DR"))) << "dBZ is not knots";

   ASSERT_TRUE(manager.openFile(QUrl::fromLocalFile(path)));
   const QString imported = manager.activeName();
   EXPECT_TRUE(manager.familyOf(imported).isEmpty()) << "an import starts unlinked";
   manager.setFamilyDefault(QStringLiteral("DV"), imported);
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty())
      << "an unlinked import cannot become a field's default";

   ASSERT_TRUE(manager.setPaletteFamily(imported, QStringLiteral("DV")));
   manager.setFamilyDefault(QStringLiteral("DV"), imported);
   EXPECT_EQ(manager.familyDefault(QStringLiteral("DV")), imported);

   // Unlinking must not leave it as a default for a field it no longer belongs to.
   ASSERT_TRUE(manager.setPaletteFamily(imported, QString {}));
   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   EXPECT_FALSE(manager.setPaletteFamily(QStringLiteral("DR"), QStringLiteral("DV")))
      << "a bundled palette's family is a meteorological fact, not a preference";
}

TEST(PaletteManager, ImportCanBeLinkedAtImportTimeAndFiltersByField)
{
   QTemporaryDir directory;
   const QString velocityPath = directory.filePath("test02.pal");
   QFile velocity(velocityPath);
   ASSERT_TRUE(velocity.open(QIODevice::WriteOnly));
   velocity.write("Units: KT\nColor: -40 0 0 255\nColor: 40 255 0 0\n");
   velocity.close();

   const QString unitlessPath = directory.filePath("test03.pal");
   QFile unitless(unitlessPath);
   ASSERT_TRUE(unitless.open(QIODevice::WriteOnly));
   unitless.write("Color: 0 1 2 3\nColor: 10 4 5 6\n");
   unitless.close();

   PaletteManager manager;
   ASSERT_TRUE(manager.openFile(QUrl::fromLocalFile(velocityPath), QStringLiteral("DV")));
   const QString linked = manager.activeName();
   EXPECT_EQ(manager.familyOf(linked), QStringLiteral("DV"));

   ASSERT_TRUE(manager.openFile(QUrl::fromLocalFile(unitlessPath)));
   const QString unlinked = manager.activeName();

   const QStringList velocityList = manager.palettesForFamily(QStringLiteral("DV"));
   EXPECT_TRUE(velocityList.contains(QStringLiteral("DV")));
   EXPECT_TRUE(velocityList.contains(QStringLiteral("SRV")));
   EXPECT_TRUE(velocityList.contains(linked));
   EXPECT_FALSE(velocityList.contains(QStringLiteral("DR")))
      << "another field's bundled ramp must never be offered here";
   EXPECT_FALSE(velocityList.contains(unlinked))
      << "an import with no units cannot be assumed to fit this field";

   const QStringList reflectivityList = manager.palettesForFamily(QStringLiteral("DR"));
   EXPECT_TRUE(reflectivityList.contains(QStringLiteral("DR")));
   EXPECT_FALSE(reflectivityList.contains(linked)) << "linked to velocity, so not offered here";

   EXPECT_EQ(manager.palettesForFamily(QString {}), manager.paletteNames())
      << "no filter means the whole catalog";
}

TEST(PaletteManager, DraftAppliedStateDistinguishesTheTwoCloseWarnings)
{
   PaletteManager manager;
   EXPECT_FALSE(manager.activeDraftApplied()) << "nothing edited yet";

   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(1, 2, 3)));
   EXPECT_FALSE(manager.activeDraftApplied()) << "edited but not applied";

   manager.applyActive();
   EXPECT_TRUE(manager.activeDraftApplied())
      << "applied, so the close prompt must warn about the file, not about losing the change";

   ASSERT_TRUE(manager.editor()->setStopColor(0, QColor(4, 5, 6)));
   EXPECT_FALSE(manager.activeDraftApplied()) << "edited again past what was applied";
}

TEST(PaletteManager, ResetFamilyDefaultsRestoresEveryBundledPalette)
{
   PaletteManager manager;
   ASSERT_TRUE(manager.select(QStringLiteral("SRV")));
   manager.applyActive();
   ASSERT_EQ(manager.familyDefault(QStringLiteral("DV")), QStringLiteral("SRV"));

   int familyChanges {0};
   QObject::connect(&manager, &PaletteManager::familyDefaultsChanged, [&]() { ++familyChanges; });
   manager.resetFamilyDefaults();

   EXPECT_TRUE(manager.familyDefault(QStringLiteral("DV")).isEmpty());
   EXPECT_TRUE(manager.isFamilyDefault(QStringLiteral("DV")));
   EXPECT_EQ(familyChanges, 1);

   manager.resetFamilyDefaults();
   EXPECT_EQ(familyChanges, 1) << "already at the bundled defaults, so nothing changed";
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
