#pragma once

#include <QObject>
#include <QVariantList>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace wxlens
{
namespace settings
{
class SettingsStore;
}
namespace overlays {

/**
 * Weather Overlays (warnings/watches + GR placefiles), docs/ROADMAP.md §7 Phase 1 slice 12.
 *
 * The visibility toggles and the placefile source list are user configuration, not session
 * state - they persist through the structured config store (§3.2) exactly like SavedPlaceManager,
 * so adding a placefile or hiding warnings survives a restart instead of resetting every launch.
 * Only the *source* (URL/path) is persisted; fetched content (titles, item geometry) is always
 * re-derived by re-fetching on startup, the same as a manual refresh.
 */
class OverlayManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QVariantList warningPolygons READ warningPolygons NOTIFY warningsChanged)
   Q_PROPERTY(QVariantList placefileItems READ placefileItems NOTIFY placefilesChanged)
   Q_PROPERTY(QVariantList placefiles READ placefiles NOTIFY placefilesChanged)
   Q_PROPERTY(bool warningsVisible READ warningsVisible WRITE setWarningsVisible NOTIFY warningsVisibleChanged)
   Q_PROPERTY(bool placefilesVisible READ placefilesVisible WRITE setPlacefilesVisible NOTIFY placefilesVisibleChanged)
   Q_PROPERTY(bool refreshingWarnings READ refreshingWarnings NOTIFY refreshingWarningsChanged)
   Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
   explicit OverlayManager(settings::SettingsStore& settings, QObject* parent = nullptr);
   ~OverlayManager() override;

   QVariantList warningPolygons() const;
   QVariantList placefileItems() const;
   QVariantList placefiles() const;
   bool warningsVisible() const;
   bool placefilesVisible() const;
   bool refreshingWarnings() const;
   QString statusText() const;

   void setWarningsVisible(bool visible);
   void setPlacefilesVisible(bool visible);

   Q_INVOKABLE void refreshWarnings();
   Q_INVOKABLE bool importWarningFile(const QUrl& url);
   Q_INVOKABLE void addPlacefile(const QUrl& url);
   Q_INVOKABLE void removePlacefile(int index);
   Q_INVOKABLE void refreshPlacefile(int index);

signals:
   void warningsChanged();
   void placefilesChanged();
   void warningsVisibleChanged();
   void placefilesVisibleChanged();
   void refreshingWarningsChanged();
   void statusTextChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

}} // namespace wxlens::overlays
