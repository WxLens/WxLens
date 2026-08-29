#include <wxlens/palettes/palette_manager.hpp>

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
} // namespace
} // namespace palettes
} // namespace wxlens
