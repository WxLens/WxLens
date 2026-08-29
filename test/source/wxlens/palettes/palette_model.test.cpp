#include <wxlens/palettes/palette_model.hpp>

#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>

namespace wxlens
{
namespace palettes
{
namespace
{
const QString kPalette =
   "; untouched heading\n"
   "Product: BR\n"
   "Units: DBZ\n"
   "Color: 0 0 0 0 ; keep this comment\n"
   "Color: 10 255 0 0 0 255 0\n"
   "RF: 1 2 3 4\n";

TEST(PaletteModel, ParsesStopsAndUsesRealColorTableForPreview)
{
   PaletteModel model;
   model.load("test", kPalette);
   ASSERT_TRUE(model.valid());
   ASSERT_EQ(model.rowCount(), 2);
   EXPECT_EQ(model.data(model.index(1), PaletteModel::HasSecondColorRole).toBool(), true);
   ASSERT_EQ(model.previewStops().size(), 64);
   EXPECT_EQ(model.previewStops().first().toMap().value("color").value<QColor>(), QColor(0, 0, 0));
}

TEST(PaletteModel, EditsOnlyColorLineAndSaveAsPreservesEverythingElse)
{
   PaletteModel model;
   model.load("test", kPalette);
   model.setStopColor(0, QColor(12, 34, 56));
   EXPECT_TRUE(model.text().contains("; untouched heading"));
   EXPECT_TRUE(model.text().contains("Color: 0 12 34 56 ; keep this comment"));
   EXPECT_TRUE(model.text().contains("RF: 1 2 3 4"));

   QTemporaryDir directory;
   const QString path = directory.filePath("copy.pal");
   ASSERT_TRUE(model.saveAs(QUrl::fromLocalFile(path)));
   QFile file(path);
   ASSERT_TRUE(file.open(QIODevice::ReadOnly));
   EXPECT_EQ(QString::fromUtf8(file.readAll()), model.text());
}

TEST(PaletteModel, InvalidPaletteIsReported)
{
   PaletteModel model;
   model.load("bad", "Product: BR\n; no colors\n");
   EXPECT_FALSE(model.valid());
   EXPECT_TRUE(model.previewStops().isEmpty());
}

TEST(PaletteModel, DirtyStateCanBeDiscardedOrClearedBySaveAs)
{
   PaletteModel model;
   model.load("test", kPalette);
   EXPECT_FALSE(model.dirty());
   model.setStopColor(0, QColor(12, 34, 56));
   EXPECT_TRUE(model.dirty());
   model.revertChanges();
   EXPECT_FALSE(model.dirty());
   EXPECT_EQ(model.data(model.index(0), PaletteModel::ColorRole).value<QColor>(), QColor(0, 0, 0));
}

TEST(PaletteModel, RejectsMalformedChannelsAndDuplicateValues)
{
   PaletteModel malformed;
   malformed.load("bad", "Product: BR\nColor: 0 nope 2 3\n");
   EXPECT_FALSE(malformed.valid());

   PaletteModel duplicate;
   duplicate.load("bad", "Product: BR\nColor: 0 1 2 3\nColor: 0 4 5 6\n");
   EXPECT_FALSE(duplicate.valid());
}

TEST(PaletteModel, RejectsDuplicateEditAndEditsSecondColor)
{
   PaletteModel model;
   model.load("test", kPalette);
   EXPECT_FALSE(model.setStopValue(1, 0.0));
   EXPECT_DOUBLE_EQ(model.data(model.index(1), PaletteModel::ValueRole).toDouble(), 10.0);
   EXPECT_TRUE(model.setStopColor(1, QColor(7, 8, 9), true));
   EXPECT_EQ(model.data(model.index(1), PaletteModel::SecondColorRole).value<QColor>(),
             QColor(7, 8, 9));
   EXPECT_EQ(model.data(model.index(1), PaletteModel::ColorRole).value<QColor>(),
             QColor(255, 0, 0));
}
} // namespace
} // namespace palettes
} // namespace wxlens
