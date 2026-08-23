// Tests for the unified map-object store: scope resolution and the three-tier lifecycle
// (docs/ROADMAP.md §4.3).
//
// Scope resolution is the part worth testing hardest - it decides whether an object drawn in one
// pane shows up in another, which is invisible in code review and obvious to a user the moment it
// is wrong.

#include <nimbus/objects/map_object.hpp>
#include <nimbus/objects/map_object_store.hpp>
#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/panes/pane_grid_model.hpp>

#include <gtest/gtest.h>

namespace nimbus::objects::test
{

namespace
{

class MapObjectScopeTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      model_.setDefaultSourceKey(QString {});
      model_.setGridSize(2, 2);
      store_.Clear();
   }

   void TearDown() override { store_.Clear(); }

   [[nodiscard]] panes::PaneController* Pane(int row) const
   {
      const QModelIndex index = model_.index(row, 0);
      return qvariant_cast<panes::PaneController*>(
         model_.data(index, panes::PaneGridModel::PaneRole));
   }

   [[nodiscard]] MapObject MakeMarker(panes::PaneController* origin,
                                      MapObjectScopeKind     kind,
                                      double                 latitude  = 35.0,
                                      double                 longitude = -97.0) const
   {
      MapObject object;
      object.type                = MapObjectType::Marker;
      object.latitudes           = {latitude};
      object.longitudes          = {longitude};
      object.lifecycle           = MapObjectLifecycle::Pinned;
      object.scope.kind          = kind;
      object.scope.originPaneId  = origin->paneId();
      object.scope.originGroupId = origin->syncGroup(panes::SyncChannel::Location);
      return object;
   }

   panes::PaneGridModel model_;
   MapObjectStore&      store_ {MapObjectStore::Instance()};
};

} // namespace

TEST_F(MapObjectScopeTest, TemporaryObjectsAreNeverStored)
{
   // Tier 1 is tool-local state: storing it is what would clutter the map on every probe.
   MapObject object    = MakeMarker(Pane(0), MapObjectScopeKind::AllPanes);
   object.lifecycle    = MapObjectLifecycle::Temporary;

   EXPECT_EQ(store_.Add(object), -1);
   EXPECT_EQ(store_.rowCount(), 0);
}

TEST_F(MapObjectScopeTest, PinnedObjectsAreStored)
{
   const int id = store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::CurrentPaneOnly));

   EXPECT_GT(id, 0);
   EXPECT_EQ(store_.rowCount(), 1);
   ASSERT_NE(store_.Find(id), nullptr);
   EXPECT_EQ(store_.Find(id)->lifecycle, MapObjectLifecycle::Pinned);
}

TEST_F(MapObjectScopeTest, GeometryIsValidated)
{
   MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::AllPanes);
   object.latitudes.clear();

   EXPECT_EQ(store_.Add(object), -1) << "empty geometry must be rejected";

   MapObject mismatched = MakeMarker(Pane(0), MapObjectScopeKind::AllPanes);
   mismatched.longitudes.append(1.0);
   EXPECT_EQ(store_.Add(mismatched), -1) << "mismatched lat/lon counts must be rejected";
}

TEST_F(MapObjectScopeTest, CurrentPaneOnlyIsVisibleOnlyInItsOwnPane)
{
   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::CurrentPaneOnly);

   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(0)));
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, Pane(1)));
}

TEST_F(MapObjectScopeTest, AllPanesIsVisibleEverywhere)
{
   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::AllPanes);

   for (int i = 0; i < 4; ++i)
   {
      EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(i))) << "pane " << i;
   }
}

TEST_F(MapObjectScopeTest, SyncGroupScopeFollowsTheGroupNotThePane)
{
   // This is the payoff of slice 5: "show this on my linked panes" resolves against the sync
   // groups rather than needing its own sharing mechanism.
   Pane(0)->setSyncGroup(panes::SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(panes::SyncChannel::Location, 1);
   Pane(2)->setSyncGroup(panes::SyncChannel::Location, 2);

   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::SyncGroup);

   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(0))) << "its own pane";
   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(1))) << "same group";
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, Pane(2))) << "different group";
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, Pane(3))) << "ungrouped, not the author";
}

TEST_F(MapObjectScopeTest, SyncGroupObjectStaysWithTheGroupWhenItsAuthorLeaves)
{
   Pane(0)->setSyncGroup(panes::SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(panes::SyncChannel::Location, 1);

   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::SyncGroup);

   // The author leaves the group it shared into. The object belongs to the group, so pane 1 must
   // keep seeing it - the alternative (following the author around) would make shared objects
   // disappear from other people's panes for no visible reason.
   Pane(0)->setSyncGroup(panes::SyncChannel::Location, panes::kNoSyncGroup);

   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(1)));
   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(0)))
      << "the author still sees its own object";
}

TEST_F(MapObjectScopeTest, UngroupedPaneStillSeesItsOwnSyncGroupObject)
{
   // Drawing a group-scoped object in an unlinked pane must not make it vanish immediately.
   const MapObject object = MakeMarker(Pane(3), MapObjectScopeKind::SyncGroup);

   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(3)));
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, Pane(0)));
}

TEST_F(MapObjectScopeTest, SameLocationTracksThePaneCentre)
{
   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::SameLocation, 35.0, -97.0);

   Pane(1)->setCenter(35.0, -97.0);
   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(1))) << "centred on the object";

   Pane(1)->setCenter(45.0, -80.0);
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, Pane(1))) << "looking elsewhere";

   // Just inside the tolerance: "same storm", not "identical camera".
   Pane(1)->setCenter(35.0 + (kSameLocationToleranceDegrees / 2.0), -97.0);
   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(object, Pane(1)));
}

TEST_F(MapObjectScopeTest, NullPaneIsNeverVisible)
{
   const MapObject object = MakeMarker(Pane(0), MapObjectScopeKind::AllPanes);
   EXPECT_FALSE(MapObjectStore::IsVisibleInPane(object, nullptr));
}

TEST_F(MapObjectScopeTest, ObjectsForPaneFiltersByScope)
{
   Pane(0)->setSyncGroup(panes::SyncChannel::Location, 1);
   Pane(1)->setSyncGroup(panes::SyncChannel::Location, 1);

   store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::CurrentPaneOnly));
   store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::SyncGroup));
   store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::AllPanes));

   EXPECT_EQ(store_.objectsForPane(Pane(0)).size(), 3) << "author sees all three";
   EXPECT_EQ(store_.objectsForPane(Pane(1)).size(), 2) << "group + global, not pane-only";
   EXPECT_EQ(store_.objectsForPane(Pane(2)).size(), 1) << "global only";
}

TEST_F(MapObjectScopeTest, ScopeCanBeChangedAfterCreation)
{
   const int id = store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::CurrentPaneOnly));
   ASSERT_GT(id, 0);
   ASSERT_FALSE(MapObjectStore::IsVisibleInPane(*store_.Find(id), Pane(1)));

   EXPECT_TRUE(store_.setObjectScope(id, static_cast<int>(MapObjectScopeKind::AllPanes)));
   EXPECT_TRUE(MapObjectStore::IsVisibleInPane(*store_.Find(id), Pane(1)));
}

TEST_F(MapObjectScopeTest, RemoveAndClearWork)
{
   const int first  = store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::AllPanes));
   const int second = store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::AllPanes));
   ASSERT_EQ(store_.rowCount(), 2);

   EXPECT_TRUE(store_.Remove(first));
   EXPECT_FALSE(store_.Remove(first)) << "removing twice is not an error, just false";
   EXPECT_EQ(store_.rowCount(), 1);
   EXPECT_NE(store_.Find(second), nullptr);

   store_.Clear();
   EXPECT_EQ(store_.rowCount(), 0);
}

TEST_F(MapObjectScopeTest, RevisionAdvancesOnEveryMutation)
{
   // QML bindings re-evaluate off this, so a mutation that fails to bump it would leave stale
   // objects on screen.
   const int initial = store_.revision();

   const int id = store_.Add(MakeMarker(Pane(0), MapObjectScopeKind::AllPanes));
   const int afterAdd = store_.revision();
   EXPECT_GT(afterAdd, initial);

   store_.setObjectScope(id, static_cast<int>(MapObjectScopeKind::CurrentPaneOnly));
   const int afterScope = store_.revision();
   EXPECT_GT(afterScope, afterAdd);

   store_.Remove(id);
   EXPECT_GT(store_.revision(), afterScope);
}

} // namespace nimbus::objects::test
