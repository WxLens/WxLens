#pragma once

#include <memory>
#include <string>

#include <QObject>
#include <QString>

namespace wxlens
{
namespace products
{

/**
 * Phase 1 slice 2: minimal, temporary bridge exposing one hardcoded radar site's load status to
 * QML while proving the Data Source -> Data Product data path (docs/ROADMAP.md §4.6). This is
 * NOT the real Data Product layer - that arrives as typed product bindings a PaneController can
 * bind to (slice 4+, once PaneGridModel exists). Superseded then, not extended.
 */
class RadarProductStatus : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString siteId READ siteId CONSTANT)
   Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
   explicit RadarProductStatus(const std::string& radarSite, QObject* parent = nullptr);
   ~RadarProductStatus() override;

   RadarProductStatus(const RadarProductStatus&)            = delete;
   RadarProductStatus& operator=(const RadarProductStatus&) = delete;
   RadarProductStatus(RadarProductStatus&&)                 = delete;
   RadarProductStatus& operator=(RadarProductStatus&&)      = delete;

   [[nodiscard]] QString siteId() const;
   [[nodiscard]] QString statusText() const;

signals:
   void statusTextChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace products
} // namespace wxlens
