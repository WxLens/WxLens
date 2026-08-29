#pragma once

#include <memory>

#include <QColor>
#include <QObject>
#include <QStringList>

namespace wxlens
{
namespace settings
{
class SettingsStore;
}
namespace theme
{

class ThemeManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString activeTheme READ activeTheme WRITE setActiveTheme NOTIFY themeChanged)
   Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged)
   Q_PROPERTY(bool dark READ dark NOTIFY themeChanged)
   Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
   Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
   Q_PROPERTY(QColor elevatedSurface READ elevatedSurface NOTIFY themeChanged)
   Q_PROPERTY(QColor control READ control NOTIFY themeChanged)
   Q_PROPERTY(QColor controlHover READ controlHover NOTIFY themeChanged)
   Q_PROPERTY(QColor controlActive READ controlActive NOTIFY themeChanged)
   Q_PROPERTY(QColor primary READ primary NOTIFY themeChanged)
   Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
   Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)
   Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
   Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
   Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
   Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
   Q_PROPERTY(QColor textMuted READ textMuted NOTIFY themeChanged)
   Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
   Q_PROPERTY(QColor radarAccent READ radarAccent NOTIFY themeChanged)
   Q_PROPERTY(QColor measurementAccent READ measurementAccent NOTIFY themeChanged)
   Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY themeChanged)
   Q_PROPERTY(int spacingUnit READ spacingUnit NOTIFY themeChanged)

public:
   explicit ThemeManager(settings::SettingsStore& store, QObject* parent = nullptr);
   ~ThemeManager() override;

   ThemeManager(const ThemeManager&)            = delete;
   ThemeManager& operator=(const ThemeManager&) = delete;

   [[nodiscard]] QString activeTheme() const;
   [[nodiscard]] QStringList availableThemes() const;
   [[nodiscard]] bool dark() const;
   [[nodiscard]] QColor background() const;
   [[nodiscard]] QColor surface() const;
   [[nodiscard]] QColor elevatedSurface() const;
   [[nodiscard]] QColor control() const;
   [[nodiscard]] QColor controlHover() const;
   [[nodiscard]] QColor controlActive() const;
   [[nodiscard]] QColor primary() const;
   [[nodiscard]] QColor accent() const;
   [[nodiscard]] QColor danger() const;
   [[nodiscard]] QColor warning() const;
   [[nodiscard]] QColor success() const;
   [[nodiscard]] QColor textPrimary() const;
   [[nodiscard]] QColor textSecondary() const;
   [[nodiscard]] QColor textMuted() const;
   [[nodiscard]] QColor border() const;
   [[nodiscard]] QColor radarAccent() const;
   [[nodiscard]] QColor measurementAccent() const;
   [[nodiscard]] int cornerRadius() const;
   [[nodiscard]] int spacingUnit() const;

   Q_INVOKABLE bool setActiveTheme(const QString& name);
   Q_INVOKABLE bool importTheme(const QString& filePath);
   Q_INVOKABLE bool exportActiveTheme(const QString& filePath) const;
   Q_INVOKABLE QString themesDirectory() const;

signals:
   void themeChanged();
   void availableThemesChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace theme
} // namespace wxlens
