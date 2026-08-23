// Tests for the per-channel pane synchronization model (docs/ROADMAP.md §4.1-4.2).
//
// These exercise the coordinator directly rather than through QML, which is the point: the sync
// rules are the part most likely to break subtly (a value propagating one channel too far, or a
// feedback loop), and driving them through the UI makes failures hard to attribute.

#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/panes/pane_grid_model.hpp>
#include <nimbus/panes/sync_types.hpp>

#include <gtest/gtest.h>

namespace nimbus::panes::test
{

namespace
{

constexpr double kTolerance = 1e-9;

class PaneSyncTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // An empty source key keeps PaneController from binding a real radar product, so these
      // tests never touch the network or the site database - they are about sync only.
      model_.setDefaultSourceKey(QString {});
      model_.setGridSize(2, 2);
   }

   [[nodiscard]] PaneController* Pane(int row) const
   {
      const QModelIndex index = model_.index(row, 0);
      return qvariant_cast<PaneController*>(model_.data(index, PaneGridModel::PaneRole));
   }

   PaneGridModel model_;
};

} // namespace

TEST_F(PaneSyncTest, GridProvidesIndependentPanes)
{
   ASSERT_EQ(model_.rowCount(), 4);
   for (int i = 0; i < 4; ++i)
   {
      ASSERT_NE(Pane(i), nullptr);
      EXPECT_EQ(Pane(i)->syncGroup(SyncChannel::Location), kNoSyncGroup);
   }
}

TEST_F(PaneSyncTest, UngroupedPanesDoNotAffectEachOther)
{
   Pane(0)->setCenter(35.0, -97.0);

   EXPECT_NEAR(Pane(1)->centerLatitude(), Pane(1)->homeLatitude(), kTolerance);
   EXPECT_NEAR(Pane(1)->centerLongitude(), Pane(1)->homeLongitude(), kTolerance);
}

TEST_F(PaneSyncTest, PanesSharingALocationGroupFollowUserInput)
{
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);

   Pane(0)->setCenter(35.0, -97.0);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);
   EXPECT_NEAR(Pane(1)->centerLongitude(), -97.0, kTolerance);

   // Pane 2 is in no group and must be untouched.
   EXPECT_NEAR(Pane(2)->centerLatitude(), Pane(2)->homeLatitude(), kTolerance);
}

TEST_F(PaneSyncTest, ChannelsAreIndependent)
{
   // The whole point of the model: shared location, independent zoom.
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);

   const double originalZoom = Pane(1)->zoom();

   Pane(0)->setCenter(35.0, -97.0);
   Pane(0)->setZoom(originalZoom + 3.0);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance) << "Location was grouped";
   EXPECT_NEAR(Pane(1)->zoom(), originalZoom, kTolerance) << "Zoom was not grouped";
}

TEST_F(PaneSyncTest, SeparateGroupsDoNotCrossTalk)
{
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);
   Pane(2)->setSyncGroup(SyncChannel::Location, 2);
   Pane(3)->setSyncGroup(SyncChannel::Location, 2);

   Pane(0)->setCenter(35.0, -97.0);
   Pane(2)->setCenter(41.0, -104.0);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);
   EXPECT_NEAR(Pane(3)->centerLatitude(), 41.0, kTolerance);
}

TEST_F(PaneSyncTest, LeavingAGroupStopsPropagation)
{
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);

   Pane(0)->setCenter(35.0, -97.0);
   ASSERT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);

   // Leaving a group is always available and must take effect immediately.
   Pane(1)->setSyncGroup(SyncChannel::Location, kNoSyncGroup);
   Pane(0)->setCenter(30.0, -90.0);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance)
      << "pane 1 left the group and must keep its own view";
}

TEST_F(PaneSyncTest, IncomingSyncDoesNotEchoBack)
{
   // §4.2's reentrancy guard: an applied sync re-emits with ProgrammaticSync, which must not fan
   // out again. Without the guard the two panes would bounce the value between them.
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);

   int userInputCount = 0;
   int syncedCount    = 0;
   QObject::connect(Pane(1),
                    &PaneController::channelChanged,
                    [&](SyncChannel channel, ChangeOrigin origin)
                    {
                       if (channel != SyncChannel::Location)
                       {
                          return;
                       }
                       if (origin == ChangeOrigin::UserInput)
                       {
                          ++userInputCount;
                       }
                       else
                       {
                          ++syncedCount;
                       }
                    });

   Pane(0)->setCenter(35.0, -97.0);

   EXPECT_EQ(syncedCount, 1) << "applied exactly once";
   EXPECT_EQ(userInputCount, 0)
      << "an applied sync must never be re-reported as user input, or it would fan out again";
}

TEST_F(PaneSyncTest, GroupOfThreeAllConverge)
{
   for (int i = 0; i < 3; ++i)
   {
      Pane(i)->setSyncGroup(SyncChannel::Location, 7);
   }

   Pane(2)->setCenter(38.5, -90.5);

   EXPECT_NEAR(Pane(0)->centerLatitude(), 38.5, kTolerance);
   EXPECT_NEAR(Pane(1)->centerLatitude(), 38.5, kTolerance);
   EXPECT_NEAR(Pane(3)->centerLatitude(), Pane(3)->homeLatitude(), kTolerance);
}

TEST_F(PaneSyncTest, CopyChannelIsOneShotNotAPersistentLink)
{
   // §4.1 insists these stay distinct concepts. Copying must move the value once and create no
   // ongoing relationship.
   Pane(0)->setCenter(35.0, -97.0);
   model_.copyChannel(Pane(0)->paneId(), Pane(1)->paneId(), SyncChannel::Location);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance) << "value copied once";
   EXPECT_EQ(Pane(1)->syncGroup(SyncChannel::Location), kNoSyncGroup)
      << "a one-shot copy must not create group membership";

   Pane(0)->setCenter(30.0, -90.0);
   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance)
      << "no ongoing link, so later changes must not follow";
}

TEST_F(PaneSyncTest, CameraGroupHelperGroupsEveryCameraChannel)
{
   model_.setCameraSyncGroup(Pane(0)->paneId(), 1);
   model_.setCameraSyncGroup(Pane(1)->paneId(), 1);

   for (const SyncChannel channel :
        {SyncChannel::Location, SyncChannel::Zoom, SyncChannel::Bearing, SyncChannel::Pitch})
   {
      EXPECT_EQ(Pane(0)->syncGroup(channel), 1);
      EXPECT_EQ(Pane(1)->syncGroup(channel), 1);
   }
   EXPECT_EQ(model_.cameraSyncGroup(Pane(0)->paneId()), 1);

   const double zoom = Pane(0)->zoom() + 2.0;
   Pane(0)->setCenter(35.0, -97.0);
   Pane(0)->setZoom(zoom);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);
   EXPECT_NEAR(Pane(1)->zoom(), zoom, kTolerance) << "zoom is part of the camera group";
}

TEST_F(PaneSyncTest, JoiningACameraGroupAdoptsItsCurrentView)
{
   model_.setCameraSyncGroup(Pane(0)->paneId(), 1);
   Pane(0)->setCenter(35.0, -97.0);

   // Pane 1 joins after the fact and should land on the group's view rather than staying put
   // until the next pan.
   model_.setCameraSyncGroup(Pane(1)->paneId(), 1);

   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);
   EXPECT_NEAR(Pane(1)->centerLongitude(), -97.0, kTolerance);
}

TEST_F(PaneSyncTest, PanesKeepGroupsAcrossGridResize)
{
   Pane(0)->setSyncGroup(SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(SyncChannel::Location, 1);

   model_.setGridSize(3, 3);

   ASSERT_EQ(model_.rowCount(), 9);
   EXPECT_EQ(Pane(0)->syncGroup(SyncChannel::Location), 1);
   EXPECT_EQ(Pane(1)->syncGroup(SyncChannel::Location), 1);

   Pane(0)->setCenter(35.0, -97.0);
   EXPECT_NEAR(Pane(1)->centerLatitude(), 35.0, kTolerance);
   EXPECT_NEAR(Pane(8)->centerLatitude(), Pane(8)->homeLatitude(), kTolerance)
      << "a pane added by the resize is independent";
}

TEST_F(PaneSyncTest, ChannelsWithoutStateArePropagationNoOps)
{
   // Declared-but-unbacked channels must not propagate a meaningless value.
   Pane(0)->setSyncGroup(SyncChannel::Time, 1);
   Pane(1)->setSyncGroup(SyncChannel::Time, 1);

   EXPECT_FALSE(Pane(0)->channelValue(SyncChannel::Time).isValid());

   // Applying an invalid value must be inert rather than corrupting anything.
   Pane(1)->applyChannelValue(SyncChannel::Time, QVariant {}, ChangeOrigin::ProgrammaticSync);
   SUCCEED();
}

} // namespace nimbus::panes::test
