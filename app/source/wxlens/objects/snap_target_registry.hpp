#pragma once

#include <QObject>
#include <QVariantMap>

namespace wxlens
{
namespace panes
{
class PaneController;
}

namespace objects
{

/**
 * Screen-space magnetic-target resolver (ROADMAP §4.4). The registry is intentionally
 * independent of the measurement controller: saved places and storm-track points can become
 * providers later without teaching the measurement geometry about either domain.
 */
class SnapTargetRegistry : public QObject
{
   Q_OBJECT

public:
   explicit SnapTargetRegistry(QObject* parent = nullptr);

   /**
    * Returns {snapped, latitude, longitude, pixelX, pixelY, kind, label}. Tolerance is pixels,
    * evaluated after projecting every target through this pane's current camera.
    */
   Q_INVOKABLE QVariantMap resolve(wxlens::panes::PaneController* pane,
                                   double                         x,
                                   double                         y,
                                   double                         tolerancePixels,
                                   bool                           suppressed) const;
};

} // namespace objects
} // namespace wxlens
