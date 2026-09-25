#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QTemporaryDir>
#include <QFile>
#include <gtest/gtest.h>

namespace wxlens
{
namespace panes
{
namespace
{
PaneController* Pane(PaneGridModel& model, int index)
{
   return qvariant_cast<PaneController*>(model.data(model.index(index, 0), PaneGridModel::PaneRole));
}

TEST(Workspace, RestoresIndependentSelectionsAndCameraButStartsLive)
{
   QTemporaryDir directory;
   settings::SettingsStore store;
   store.SetConfigDirectory(directory.path());
   {
      PaneGridModel model;
      model.setAdvancedPaneLinking(true);
      model.restoreWorkspace(store, {});
      EXPECT_EQ(model.rowCount(), 1);
      model.setGridSize(2, 1);
      Pane(model, 1)->setProductName("Velocity");
      Pane(model, 1)->setSelectedElevation(1.5);
      Pane(model, 1)->setCenter(42, -93);
      Pane(model, 1)->setZoom(9);
      Pane(model, 1)->setSyncGroup(SyncChannel::Time, 4);
      Pane(model, 1)->selectArchiveTime("2020-01-01 12:00");
      model.setGridSize(1, 1);
      ASSERT_TRUE(model.saveWorkspace());
   }
   store.Reload();
   PaneGridModel restored;
   restored.setAdvancedPaneLinking(true);
   restored.restoreWorkspace(store, {});
   EXPECT_EQ(restored.gridWidth(), 1);
   EXPECT_EQ(restored.rowCount(), 2);
   EXPECT_EQ(Pane(restored, 0)->productName(), "Reflectivity");
   EXPECT_EQ(Pane(restored, 1)->productName(), "Velocity");
   EXPECT_EQ(Pane(restored, 1)->selectedElevation(), 1.5);
   EXPECT_EQ(Pane(restored, 1)->centerLatitude(), 42);
   EXPECT_EQ(Pane(restored, 1)->zoom(), 9);
   EXPECT_EQ(Pane(restored, 1)->syncGroup(SyncChannel::Time), 4);
   EXPECT_TRUE(Pane(restored, 1)->liveMode());
}

TEST(Workspace, InvalidValuesFallBackAndMalformedFileIsPreserved)
{
   QTemporaryDir directory;
   settings::SettingsStore store;
   store.SetConfigDirectory(directory.path());
   store.SetInt("workspace", "width", 999);
   store.SetDouble("workspace", "pane_0_latitude", 999);
   store.SetString("workspace", "pane_0_identity", "invalid");
   ASSERT_TRUE(store.Save());
   {
      PaneGridModel model;
      model.restoreWorkspace(store, {});
      EXPECT_EQ(model.gridWidth(), 1);
      EXPECT_EQ(Pane(model, 0)->productName(), "Reflectivity");
      EXPECT_LE(Pane(model, 0)->centerLatitude(), 90);
   }
   QFile file(store.FilePath("workspace"));
   ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("invalid = [");
   file.close();
   store.Reload();
   PaneGridModel model;
   model.restoreWorkspace(store, {});
   EXPECT_FALSE(model.saveWorkspace());
   ASSERT_TRUE(file.open(QIODevice::ReadOnly));
   EXPECT_EQ(file.readAll(), "invalid = [");
}
} // namespace
} // namespace panes
} // namespace wxlens
