#include <wxlens/overlays/overlay_manager.hpp>

#include <QUrl>
#include <gtest/gtest.h>

namespace wxlens { namespace overlays {

TEST(OverlayManagerTest, LoadsWxdataPlacefileVectors)
{
   OverlayManager manager;
   const QString path = QStringLiteral(SCWX_TEST_DATA_DIR) +
                        QStringLiteral("/gr/placefiles/placefile-old-example.txt");
   manager.addPlacefile(QUrl::fromLocalFile(path));

   ASSERT_FALSE(manager.placefiles().empty());
   const QVariantMap record = manager.placefiles().front().toMap();
   EXPECT_TRUE(record.value("error").toString().isEmpty());
   EXPECT_GT(record.value("itemCount").toInt(), 0);
   EXPECT_FALSE(manager.placefileItems().empty());
}

TEST(OverlayManagerTest, VisibilityChannelsAreIndependent)
{
   OverlayManager manager;
   manager.setWarningsVisible(false);
   EXPECT_FALSE(manager.warningsVisible());
   EXPECT_TRUE(manager.placefilesVisible());
   manager.setPlacefilesVisible(false);
   EXPECT_FALSE(manager.placefilesVisible());
}

}} // namespace wxlens::overlays
