#pragma once

#include <QObject>
#include <QVariantList>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace wxlens { namespace overlays {

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
   explicit OverlayManager(QObject* parent = nullptr);
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
