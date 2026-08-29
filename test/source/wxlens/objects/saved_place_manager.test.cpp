#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/objects/saved_place_manager.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <gtest/gtest.h>

namespace
{
class SavedPlaceTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(directory_.isValid());
      settings_.SetConfigDirectory(directory_.path());
      store_.Clear();
   }
   void TearDown() override { store_.Clear(); }

   QTemporaryDir directory_;
   wxlens::settings::SettingsStore settings_;
   wxlens::objects::MapObjectStore store_;
};

TEST_F(SavedPlaceTest, PlaceUsesUnifiedStoreAndSavedDefaults)
{
   wxlens::objects::SavedPlaceManager manager {store_, settings_};
   const QString group = manager.addGroup(QStringLiteral("Family"), QStringLiteral("#ff3366"));
   const int id = manager.addPlace(QStringLiteral("Home"), 39.1, -94.5, group);
   ASSERT_GT(id, 0);
   const auto* object = store_.Find(id);
   ASSERT_NE(object, nullptr);
   EXPECT_EQ(object->type, wxlens::objects::MapObjectType::Marker);
   EXPECT_EQ(object->lifecycle, wxlens::objects::MapObjectLifecycle::Saved);
   EXPECT_EQ(object->scope.kind, wxlens::objects::MapObjectScopeKind::AllPanes);
   EXPECT_EQ(object->color, QStringLiteral("#ff3366"));
}

TEST_F(SavedPlaceTest, GroupColourVisibilityAndOverridePropagate)
{
   wxlens::objects::SavedPlaceManager manager {store_, settings_};
   const QString group = manager.addGroup(QStringLiteral("Work"), QStringLiteral("#112233"));
   const int inherited = manager.addPlace(QStringLiteral("Office"), 1.0, 2.0, group);
   const int overridden = manager.addPlace(QStringLiteral("Depot"), 3.0, 4.0, group,
                                           QStringLiteral("#abcdef"));
   ASSERT_TRUE(manager.editGroup(group, QStringLiteral("Work"), QStringLiteral("#445566")));
   EXPECT_EQ(store_.Find(inherited)->color, QStringLiteral("#445566"));
   EXPECT_EQ(store_.Find(overridden)->color, QStringLiteral("#abcdef"));
   ASSERT_TRUE(manager.setGroupVisible(group, false));
   EXPECT_FALSE(store_.Find(inherited)->visible);
   EXPECT_FALSE(store_.Find(overridden)->visible);
}

TEST_F(SavedPlaceTest, PlaceAndGroupCanBeEditedAndRenamed)
{
   wxlens::objects::SavedPlaceManager manager {store_, settings_};
   const QString family = manager.addGroup(QStringLiteral("Family"), QStringLiteral("#112233"));
   const QString work = manager.addGroup(QStringLiteral("Work"), QStringLiteral("#445566"));
   const int id = manager.addPlace(QStringLiteral("Old name"), 10.0, 20.0, family);

   ASSERT_TRUE(manager.editGroup(work, QStringLiteral("Colleagues"), QStringLiteral("#778899")));
   ASSERT_TRUE(manager.editPlace(id, QStringLiteral("New name"), 30.0, 40.0, work,
                                 QStringLiteral("#abcdef")));
   const auto* object = store_.Find(id);
   ASSERT_NE(object, nullptr);
   EXPECT_EQ(object->label, QStringLiteral("New name"));
   EXPECT_DOUBLE_EQ(object->latitudes.front(), 30.0);
   EXPECT_DOUBLE_EQ(object->longitudes.front(), 40.0);
   EXPECT_EQ(object->savedPlaceGroupId, work);
   EXPECT_EQ(object->color, QStringLiteral("#abcdef"));
   EXPECT_EQ(manager.groups().at(1).toMap().value("name").toString(),
             QStringLiteral("Colleagues"));
}

TEST_F(SavedPlaceTest, PersistsAndImportsPortableJson)
{
   {
      wxlens::objects::SavedPlaceManager manager {store_, settings_};
      const QString group = manager.addGroup(QStringLiteral("Friends"), QStringLiteral("#123456"));
      ASSERT_GT(manager.addPlace(QStringLiteral("Alex"), 35.0, -97.0, group), 0);
      ASSERT_TRUE(manager.exportFile(directory_.filePath(QStringLiteral("copy.json"))));
   }
   store_.Clear();
   wxlens::objects::SavedPlaceManager reloaded {store_, settings_};
   ASSERT_EQ(reloaded.places().size(), 1);
   EXPECT_EQ(reloaded.places().front().toMap().value("name").toString(), QStringLiteral("Alex"));

   store_.Clear();
   wxlens::objects::SavedPlaceManager imported {store_, settings_};
   ASSERT_TRUE(imported.importFile(directory_.filePath(QStringLiteral("copy.json"))));
   EXPECT_GE(imported.search(QStringLiteral("ale")).size(), 1);
}

TEST_F(SavedPlaceTest, RejectsInvalidCoordinatesAndMalformedImport)
{
   wxlens::objects::SavedPlaceManager manager {store_, settings_};
   const QString group = manager.addGroup(QStringLiteral("Family"), QStringLiteral("#123456"));
   EXPECT_EQ(manager.addPlace(QStringLiteral("Bad"), 91.0, 0.0, group), -1);
   QFile file {directory_.filePath(QStringLiteral("bad.json"))};
   ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("not json"); file.close();
   EXPECT_FALSE(manager.importFile(file.fileName()));
   EXPECT_EQ(store_.rowCount(), 0);
}

TEST_F(SavedPlaceTest, MalformedPersistentFileIsNotOverwrittenAtStartup)
{
   const QString path = directory_.filePath(QStringLiteral("saved-places.json"));
   QFile file {path};
   ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("unfinished user edit");
   file.close();

   wxlens::objects::SavedPlaceManager manager {store_, settings_};
   ASSERT_TRUE(file.open(QIODevice::ReadOnly));
   EXPECT_EQ(file.readAll(), QByteArrayLiteral("unfinished user edit"));
}
} // namespace
