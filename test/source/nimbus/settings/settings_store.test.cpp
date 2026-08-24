// Tests for the structured config store (docs/ROADMAP.md §3.2, docs/adr/0003).
//
// The store's whole job is to be safe in the presence of a file a human edited by hand, so that is
// what these test hardest: a malformed file, a wrong-typed value and an out-of-range number must
// each fall back to the default without taking neighbouring settings down with them, and a file
// the store could not parse must survive a save untouched rather than being replaced by defaults.

#include <nimbus/settings/settings_store.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <gtest/gtest.h>

namespace nimbus::settings::test
{

namespace
{

class SettingsStoreTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ASSERT_TRUE(tempDir_.isValid());
      store_ = std::make_unique<SettingsStore>();
      store_->SetConfigDirectory(tempDir_.path());
   }

   void WriteFile(const QString& category, const QString& contents) const
   {
      QFile file {QDir(tempDir_.path()).filePath(category + QStringLiteral(".toml"))};
      ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
      file.write(contents.toUtf8());
      file.close();
   }

   [[nodiscard]] QString ReadFile(const QString& category) const
   {
      QFile file {QDir(tempDir_.path()).filePath(category + QStringLiteral(".toml"))};
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      {
         return {};
      }
      return QString::fromUtf8(file.readAll());
   }

   QTemporaryDir                  tempDir_;
   std::unique_ptr<SettingsStore> store_;
};

} // namespace

TEST_F(SettingsStoreTest, MissingFileYieldsDefaults)
{
   // A fresh install has no config at all. That is the normal case, not an error.
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 7, 0, 10), 7);
   EXPECT_TRUE(store_->GetBool(QStringLiteral("radar"), QStringLiteral("show_terrain"), true));
   EXPECT_EQ(store_->GetString(QStringLiteral("units"), QStringLiteral("x"), QStringLiteral("m")),
             QStringLiteral("m"));
   EXPECT_FALSE(store_->CategoryFailedToParse(QStringLiteral("measurement")));
}

TEST_F(SettingsStoreTest, RoundTripsThroughDisk)
{
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 2);
   store_->SetBool(QStringLiteral("radar"), QStringLiteral("show_terrain"), false);
   store_->SetDouble(QStringLiteral("objects"), QStringLiteral("ring_radius"), 50000.0);
   store_->SetString(QStringLiteral("units"), QStringLiteral("distance"), QStringLiteral("metric"));
   ASSERT_TRUE(store_->Save());

   // Re-read from disk rather than from memory - persisting is the entire point, and an in-memory
   // read would pass even if nothing was ever written.
   store_->Reload();

   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 10), 2);
   EXPECT_FALSE(store_->GetBool(QStringLiteral("radar"), QStringLiteral("show_terrain"), true));
   EXPECT_DOUBLE_EQ(
      store_->GetDouble(QStringLiteral("objects"), QStringLiteral("ring_radius"), 0.0, 0.0, 1e9),
      50000.0);
   EXPECT_EQ(
      store_->GetString(QStringLiteral("units"), QStringLiteral("distance"), QString {}),
      QStringLiteral("metric"));
}

TEST_F(SettingsStoreTest, OneFilePerCategory)
{
   // ADR 0003: the on-disk layout mirrors the typed-accessor split rather than one blob.
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 1);
   store_->SetInt(QStringLiteral("units"), QStringLiteral("distance"), 1);
   ASSERT_TRUE(store_->Save());

   EXPECT_TRUE(QFile::exists(store_->FilePath(QStringLiteral("measurement"))));
   EXPECT_TRUE(QFile::exists(store_->FilePath(QStringLiteral("units"))));
   EXPECT_TRUE(store_->FilePath(QStringLiteral("measurement")).endsWith(QStringLiteral(".toml")));
}

TEST_F(SettingsStoreTest, SavedFileIsHandEditableAndReadsBack)
{
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 1);
   ASSERT_TRUE(store_->Save());

   // Comments are the reason ADR 0003 chose TOML over JSON, so the written file must actually
   // carry the explanatory header - a user is expected to open this.
   const QString written = ReadFile(QStringLiteral("measurement"));
   EXPECT_TRUE(written.contains(QStringLiteral("# Nimbus settings")));
   EXPECT_TRUE(written.contains(QStringLiteral("gesture")));

   // And what a human types back in must be picked up.
   WriteFile(QStringLiteral("measurement"),
             QStringLiteral("# my notes\ngesture = 2\n"));
   store_->Reload();
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 10), 2);
}

TEST_F(SettingsStoreTest, WrongTypeFallsBackWithoutAffectingNeighbours)
{
   WriteFile(QStringLiteral("measurement"),
             QStringLiteral("gesture = \"drag\"\nother = 5\n"));

   // The typo'd key falls back...
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 10), 0);
   // ...and the valid key beside it is untouched. A parser that gave up on the whole file here
   // would turn one typo into a full settings reset.
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("other"), 0, 0, 10), 5);
}

TEST_F(SettingsStoreTest, OutOfRangeFallsBackRatherThanClamping)
{
   WriteFile(QStringLiteral("measurement"), QStringLiteral("gesture = 99\n"));

   // Falling back, not clamping: clamping would silently turn a typo into a different valid-looking
   // setting the user never chose, and they would have no way to tell.
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 2), 0);

   WriteFile(QStringLiteral("objects"), QStringLiteral("ring_radius = -10.0\n"));
   EXPECT_DOUBLE_EQ(
      store_->GetDouble(QStringLiteral("objects"), QStringLiteral("ring_radius"), 500.0, 0.0, 1e6),
      500.0);
}

TEST_F(SettingsStoreTest, IntegerIsAcceptedForADoubleSetting)
{
   // "radius = 50" is a perfectly reasonable way to hand-write a double; rejecting the tidier
   // spelling would be a gotcha with no upside.
   WriteFile(QStringLiteral("objects"), QStringLiteral("ring_radius = 50\n"));
   EXPECT_DOUBLE_EQ(
      store_->GetDouble(QStringLiteral("objects"), QStringLiteral("ring_radius"), 0.0, 0.0, 1e6),
      50.0);
}

TEST_F(SettingsStoreTest, MalformedFileFallsBackAndIsNotOverwritten)
{
   const QString handWritten = QStringLiteral("gesture = = 3\nthis is not toml\n");
   WriteFile(QStringLiteral("measurement"), handWritten);

   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 10), 0);
   EXPECT_TRUE(store_->CategoryFailedToParse(QStringLiteral("measurement")));

   // Writing our view of a file we could not read would discard whatever the user was in the
   // middle of editing. Save reports failure instead, and the file on disk is byte-for-byte intact.
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 1);
   EXPECT_FALSE(store_->Save());
   EXPECT_EQ(ReadFile(QStringLiteral("measurement")), handWritten);
}

TEST_F(SettingsStoreTest, MalformedCategoryDoesNotBlockOthers)
{
   WriteFile(QStringLiteral("measurement"), QStringLiteral("garbage = = =\n"));
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 1);
   store_->SetInt(QStringLiteral("units"), QStringLiteral("distance"), 2);

   // Save returns false because one category could not be written, but the healthy category must
   // still make it to disk - one bad file should not freeze every other preference.
   EXPECT_FALSE(store_->Save());
   store_->Reload();
   EXPECT_EQ(store_->GetInt(QStringLiteral("units"), QStringLiteral("distance"), 0, 0, 10), 2);
}

TEST_F(SettingsStoreTest, UnknownKeysInAFileAreLeftAlone)
{
   // Forward compatibility both ways: a key written by a newer build, or a note a user added,
   // must survive this build saving the file.
   WriteFile(QStringLiteral("measurement"),
             QStringLiteral("gesture = 1\nsomething_from_a_newer_build = 42\n"));
   store_->SetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 2);
   ASSERT_TRUE(store_->Save());

   store_->Reload();
   EXPECT_EQ(store_->GetInt(
                QStringLiteral("measurement"), QStringLiteral("something_from_a_newer_build"), 0, 0, 100),
             42);
   EXPECT_EQ(store_->GetInt(QStringLiteral("measurement"), QStringLiteral("gesture"), 0, 0, 10), 2);
}

} // namespace nimbus::settings::test
