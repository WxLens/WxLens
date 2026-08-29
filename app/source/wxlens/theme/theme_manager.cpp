#include <wxlens/theme/theme_manager.hpp>

#include <wxlens/log/logger.hpp>
#include <wxlens/settings/settings_store.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>

#include <toml++/toml.hpp>

namespace wxlens
{
namespace theme
{
namespace
{
const QString kBuiltInDark  = QStringLiteral(":/qt/qml/WxLens/App/res/themes/operational-dark.toml");
const QString kBuiltInLight = QStringLiteral(":/qt/qml/WxLens/App/res/themes/daylight.toml");
const auto logger_ = wxlens::log::Create("theme.theme_manager");

struct Theme
{
   QString name;
   bool dark {};
   QColor background, surface, elevatedSurface, control, controlHover, controlActive;
   QColor primary, accent, danger, warning, success;
   QColor textPrimary, textSecondary, textMuted, border, radarAccent, measurementAccent;
   int cornerRadius {}, spacingUnit {};
   QByteArray source;
};

QString LocalPath(QString path)
{
   if (path.startsWith(QStringLiteral("file:///"))) path.remove(0, 8);
   else if (path.startsWith(QStringLiteral("file://"))) path.remove(0, 7);
   return QDir::fromNativeSeparators(path);
}

bool ParseTheme(const QByteArray& bytes, Theme& out, QString& error)
{
   try
   {
      const auto table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
      const auto version = table["theme"]["version"].value<int64_t>();
      const auto name = table["theme"]["name"].value<std::string>();
      const auto dark = table["theme"]["dark"].value<bool>();
      if (!version || *version != 1 || !name || name->empty() || !dark)
      {
         error = QStringLiteral("theme.version must be 1 and name/dark are required");
         return false;
      }
      auto color = [&](const char* key, QColor& target)
      {
         const auto value = table["colors"][key].value<std::string>();
         if (!value) return false;
         target = QColor(QString::fromStdString(*value));
         return target.isValid();
      };
      out.name = QString::fromStdString(*name); out.dark = *dark;
      const bool colorsOk =
         color("background", out.background) && color("surface", out.surface) &&
         color("elevated_surface", out.elevatedSurface) && color("control", out.control) &&
         color("control_hover", out.controlHover) && color("control_active", out.controlActive) &&
         color("primary", out.primary) && color("accent", out.accent) &&
         color("danger", out.danger) && color("warning", out.warning) &&
         color("success", out.success) && color("text_primary", out.textPrimary) &&
         color("text_secondary", out.textSecondary) && color("text_muted", out.textMuted) &&
         color("border", out.border) && color("radar_accent", out.radarAccent) &&
         color("measurement_accent", out.measurementAccent);
      const auto radius = table["metrics"]["corner_radius"].value<int64_t>();
      const auto spacing = table["metrics"]["spacing_unit"].value<int64_t>();
      if (!colorsOk || !radius || !spacing || *radius < 0 || *radius > 24 || *spacing < 1 || *spacing > 16)
      {
         error = QStringLiteral("all color roles and valid metrics are required");
         return false;
      }
      out.cornerRadius = static_cast<int>(*radius); out.spacingUnit = static_cast<int>(*spacing);
      out.source = bytes;
      return true;
   }
   catch (const toml::parse_error& ex)
   {
      error = QString::fromUtf8(ex.what()); return false;
   }
}

bool ReadTheme(const QString& path, Theme& theme)
{
   QFile file(path);
   if (!file.open(QIODevice::ReadOnly)) return false;
   QString error;
   if (!ParseTheme(file.readAll(), theme, error))
   {
      logger_->warn("Rejected theme {}: {}", path.toStdString(), error.toStdString());
      return false;
   }
   return true;
}
} // namespace

class ThemeManager::Impl
{
public:
   explicit Impl(settings::SettingsStore& store) : store_ {store} {}
   void LoadAll()
   {
      Theme theme;
      for (const auto& path : {kBuiltInDark, kBuiltInLight}) if (ReadTheme(path, theme)) themes_[theme.name] = theme;
      QDir dir(ThemesDirectory());
      dir.mkpath(QStringLiteral("."));
      for (const auto& file : dir.entryList({QStringLiteral("*.toml")}, QDir::Files))
         if (ReadTheme(dir.filePath(file), theme)) themes_[theme.name] = theme;
      const QString wanted = store_.GetString(QStringLiteral("appearance"), QStringLiteral("theme"), QStringLiteral("Operational Dark"));
      active_ = themes_.contains(wanted) ? wanted : QStringLiteral("Operational Dark");
   }
   QString ThemesDirectory() const { return QDir(store_.ConfigDirectory()).filePath(QStringLiteral("themes")); }
   settings::SettingsStore& store_;
   QMap<QString, Theme> themes_;
   QString active_;
};

ThemeManager::ThemeManager(settings::SettingsStore& store, QObject* parent) : QObject(parent), p {std::make_unique<Impl>(store)} { p->LoadAll(); }
ThemeManager::~ThemeManager() = default;
#define THEME_GETTER(type, method, field) type ThemeManager::method() const { return p->themes_[p->active_].field; }
THEME_GETTER(QString, activeTheme, name)
QStringList ThemeManager::availableThemes() const { return p->themes_.keys(); }
THEME_GETTER(bool, dark, dark)
THEME_GETTER(QColor, background, background)
THEME_GETTER(QColor, surface, surface)
THEME_GETTER(QColor, elevatedSurface, elevatedSurface)
THEME_GETTER(QColor, control, control)
THEME_GETTER(QColor, controlHover, controlHover)
THEME_GETTER(QColor, controlActive, controlActive)
THEME_GETTER(QColor, primary, primary)
THEME_GETTER(QColor, accent, accent)
THEME_GETTER(QColor, danger, danger)
THEME_GETTER(QColor, warning, warning)
THEME_GETTER(QColor, success, success)
THEME_GETTER(QColor, textPrimary, textPrimary)
THEME_GETTER(QColor, textSecondary, textSecondary)
THEME_GETTER(QColor, textMuted, textMuted)
THEME_GETTER(QColor, border, border)
THEME_GETTER(QColor, radarAccent, radarAccent)
THEME_GETTER(QColor, measurementAccent, measurementAccent)
THEME_GETTER(int, cornerRadius, cornerRadius)
THEME_GETTER(int, spacingUnit, spacingUnit)
#undef THEME_GETTER

bool ThemeManager::setActiveTheme(const QString& name)
{
   if (!p->themes_.contains(name)) return false;
   if (p->active_ == name) return true;
   p->active_ = name;
   p->store_.SetString(QStringLiteral("appearance"), QStringLiteral("theme"), name);
   p->store_.Save();
   Q_EMIT themeChanged();
   return true;
}

bool ThemeManager::importTheme(const QString& filePath)
{
   Theme theme;
   const QString path = LocalPath(filePath);
   if (!ReadTheme(path, theme)) return false;
   QDir dir(p->ThemesDirectory()); dir.mkpath(QStringLiteral("."));
   QString safe = theme.name; safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
   const QString destination = dir.filePath(safe + QStringLiteral(".toml"));
   if (QFileInfo(path).absoluteFilePath() != QFileInfo(destination).absoluteFilePath())
   {
      QFile::remove(destination);
      if (!QFile::copy(path, destination)) return false;
   }
   const bool added = !p->themes_.contains(theme.name);
   p->themes_[theme.name] = theme;
   if (added) Q_EMIT availableThemesChanged();
   return setActiveTheme(theme.name);
}

bool ThemeManager::exportActiveTheme(const QString& filePath) const
{
   QSaveFile file(LocalPath(filePath));
   if (!file.open(QIODevice::WriteOnly) || file.write(p->themes_[p->active_].source) < 0) return false;
   return file.commit();
}

QString ThemeManager::themesDirectory() const { return p->ThemesDirectory(); }
} // namespace theme
} // namespace wxlens
