#include <wxlens/overlays/overlay_manager.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QTemporaryDir>
#include <QUrl>
#include <gtest/gtest.h>

namespace wxlens { namespace overlays {
namespace
{
class OverlayManagerTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(directory_.isValid());
      settings_.SetConfigDirectory(directory_.path());
   }

   QTemporaryDir            directory_;
   settings::SettingsStore settings_;
};
} // namespace

TEST_F(OverlayManagerTest, LoadsWxdataPlacefileVectors)
{
   OverlayManager manager {settings_};
   const QString path = QStringLiteral(SCWX_TEST_DATA_DIR) +
                        QStringLiteral("/gr/placefiles/placefile-old-example.txt");
   manager.addPlacefile(QUrl::fromLocalFile(path));

   ASSERT_FALSE(manager.placefiles().empty());
   const QVariantMap record = manager.placefiles().front().toMap();
   EXPECT_TRUE(record.value("error").toString().isEmpty());
   EXPECT_GT(record.value("itemCount").toInt(), 0);
   EXPECT_FALSE(manager.placefileItems().empty());
}

TEST_F(OverlayManagerTest, VisibilityChannelsAreIndependent)
{
   OverlayManager manager {settings_};
   manager.setWarningsVisible(false);
   EXPECT_FALSE(manager.warningsVisible());
   EXPECT_TRUE(manager.placefilesVisible());
   manager.setPlacefilesVisible(false);
   EXPECT_FALSE(manager.placefilesVisible());
}

TEST_F(OverlayManagerTest, VisibilityAndPlacefilesPersistAcrossRestart)
{
   const QString path = QStringLiteral(SCWX_TEST_DATA_DIR) +
                        QStringLiteral("/gr/placefiles/placefile-old-example.txt");
   {
      OverlayManager manager {settings_};
      manager.setWarningsVisible(false);
      manager.addPlacefile(QUrl::fromLocalFile(path));
      ASSERT_EQ(manager.placefiles().size(), 1);
   }

   settings::SettingsStore reopened;
   reopened.SetConfigDirectory(directory_.path());
   OverlayManager restarted {reopened};
   EXPECT_FALSE(restarted.warningsVisible());
   EXPECT_TRUE(restarted.placefilesVisible());
   ASSERT_EQ(restarted.placefiles().size(), 1);
   const QVariantMap record = restarted.placefiles().front().toMap();
   EXPECT_EQ(record.value("source").toString(), QUrl::fromLocalFile(path).toString());
   EXPECT_TRUE(record.value("error").toString().isEmpty());
   EXPECT_GT(record.value("itemCount").toInt(), 0);
}

}} // namespace wxlens::overlays
