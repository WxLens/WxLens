#include <wxlens/settings/settings_store.hpp>
#include <wxlens/theme/theme_manager.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace wxlens::theme::test
{
TEST(ThemeManager, BundlesDarkAndLightAndLiveSwitches)
{
   QTemporaryDir dir;
   settings::SettingsStore store;
   store.SetConfigDirectory(dir.path());
   ThemeManager manager {store};
   EXPECT_TRUE(manager.availableThemes().contains(QStringLiteral("Operational Dark")));
   EXPECT_TRUE(manager.availableThemes().contains(QStringLiteral("Daylight")));
   EXPECT_TRUE(manager.dark());
   const QColor darkBackground = manager.background();
   int changes = 0;
   QObject::connect(&manager, &ThemeManager::themeChanged, &manager, [&changes]() { ++changes; });
   EXPECT_TRUE(manager.setActiveTheme(QStringLiteral("Daylight")));
   EXPECT_FALSE(manager.dark());
   EXPECT_NE(manager.background(), darkBackground);
   EXPECT_EQ(changes, 1);
}

TEST(ThemeManager, SelectionPersistsAndUnknownNameIsRejected)
{
   QTemporaryDir dir;
   settings::SettingsStore store;
   store.SetConfigDirectory(dir.path());
   { ThemeManager manager {store}; EXPECT_TRUE(manager.setActiveTheme(QStringLiteral("Daylight"))); }
   settings::SettingsStore reopened;
   reopened.SetConfigDirectory(dir.path());
   ThemeManager manager {reopened};
   EXPECT_EQ(manager.activeTheme(), QStringLiteral("Daylight"));
   EXPECT_FALSE(manager.setActiveTheme(QStringLiteral("Missing")));
   EXPECT_EQ(manager.activeTheme(), QStringLiteral("Daylight"));
}

TEST(ThemeManager, ShareableThemeRoundTripsAndMalformedThemeIsSafe)
{
   QTemporaryDir dir;
   settings::SettingsStore store;
   store.SetConfigDirectory(dir.path());
   ThemeManager manager {store};
   const QString exported = dir.filePath(QStringLiteral("exported.toml"));
   EXPECT_TRUE(manager.exportActiveTheme(exported));
   EXPECT_TRUE(manager.importTheme(exported));

   const QString broken = dir.filePath(QStringLiteral("broken.toml"));
   QFile file(broken); ASSERT_TRUE(file.open(QIODevice::WriteOnly));
   file.write("[theme]\nversion = 99\nname = \"Broken\"\n"); file.close();
   const QString before = manager.activeTheme();
   EXPECT_FALSE(manager.importTheme(broken));
   EXPECT_EQ(manager.activeTheme(), before);
}
} // namespace wxlens::theme::test
