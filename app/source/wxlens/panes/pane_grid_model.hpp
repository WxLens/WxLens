#pragma once

#include <wxlens/panes/sync_types.hpp>

#include <memory>

#include <QAbstractListModel>
#include <QVariantList>

namespace wxlens
{
namespace panes
{

class PaneController;

/**
 * The pane grid (docs/ROADMAP.md §4.6): a QAbstractListModel holding
 * `gridWidth * gridHeight` PaneControllers, 1x1 through 3x3 and beyond. A 1x1 grid is just the
 * degenerate case, not a special-cased single view - that's the whole point of building this
 * before sync (§4.1) rather than after.
 *
 * Panes persist across grid resizes: growing the grid keeps the existing panes and appends new
 * ones. Shrinking leaves trailing panes retained but inactive, so restoring a previous layout
 * preserves its state and QML map items are not destroyed without a current render context.
 */
class PaneGridModel : public QAbstractListModel
{
   Q_OBJECT

   Q_PROPERTY(int gridWidth READ gridWidth NOTIFY gridSizeChanged)
   Q_PROPERTY(int gridHeight READ gridHeight NOTIFY gridSizeChanged)

   // Bumped whenever any pane's group membership changes. Group state is queried through methods
   // (it is per-pane and per-channel, so it does not reduce to one property), and QML cannot tell
   // when a method's result goes stale - so a binding that calls cameraSyncGroup() references
   // this property as well to know when to re-evaluate.
   Q_PROPERTY(int syncRevision READ syncRevision NOTIFY syncRevisionChanged)
   Q_PROPERTY(int firstPaneId READ firstPaneId NOTIFY gridSizeChanged)
   Q_PROPERTY(QObject* activePane READ activePane NOTIFY activePaneChanged)
   Q_PROPERTY(int activePaneIndex READ activePaneIndex NOTIFY activePaneChanged)
   Q_PROPERTY(QVariantList radarSites READ radarSites CONSTANT)

public:
   enum Roles
   {
      PaneRole = Qt::UserRole + 1
   };

   explicit PaneGridModel(QObject* parent = nullptr);
   ~PaneGridModel() override;

   PaneGridModel(const PaneGridModel&)            = delete;
   PaneGridModel& operator=(const PaneGridModel&) = delete;
   PaneGridModel(PaneGridModel&&)                 = delete;
   PaneGridModel& operator=(PaneGridModel&&)      = delete;

   [[nodiscard]] int gridWidth() const;
   [[nodiscard]] int gridHeight() const;
   [[nodiscard]] int syncRevision() const;
   [[nodiscard]] int firstPaneId() const;
   [[nodiscard]] QObject* activePane() const;
   [[nodiscard]] int activePaneIndex() const;
   [[nodiscard]] QVariantList radarSites() const;

   /** Includes retained inactive panes; gridWidth * gridHeight is the visible pane count. */
   [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
   [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

   Q_INVOKABLE void setGridSize(int width, int height);
   Q_INVOKABLE void setActivePaneIndex(int index);

   /**
    * One-shot: copies one channel's current value from one pane to another, without creating any
    * ongoing group membership. This is the "match this pane's view to that one" action, kept
    * deliberately distinct from joining a group (docs/ROADMAP.md §4.1) - conflating them is
    * exactly what makes link behaviour feel unpredictable.
    */
   Q_INVOKABLE void copyChannel(int fromPaneId, int toPaneId, wxlens::panes::SyncChannel channel);
   Q_INVOKABLE void copyCamera(int fromPaneId, int toPaneId);

   /**
    * Convenience for pane chrome: puts a pane in (or removes it from) `groupId` on all camera
    * channels at once. The underlying model stays per-channel - this is UI sugar over it, not a
    * second mechanism.
    */
   Q_INVOKABLE void setCameraSyncGroup(int paneId, int groupId);

   /** Whether every camera channel of this pane is in `groupId`, for UI state. */
   Q_INVOKABLE int cameraSyncGroup(int paneId) const;

   /**
    * User-facing combinations from §4.5. Presets are deliberately named instead of exposing
    * enum ordinals to QML: "map", "map-site", "palette", "all", and "independent".
    */
   Q_INVOKABLE void setSyncPreset(int paneId, const QString& preset, int groupId);
   Q_INVOKABLE QString syncPreset(int paneId) const;
   Q_INVOKABLE int syncGroupForPreset(int paneId) const;

   /** The default radar site new panes are created with, until pane chrome can set it (§4.5). */
   void setDefaultSourceKey(const QString& sourceKey);

signals:
   void gridSizeChanged();
   void syncRevisionChanged();
   void activePaneChanged();

private:
   /**
    * The sync coordinator (docs/ROADMAP.md §4.1-4.2). Fans a channel change out to every other
    * pane sharing that channel's group. Lives here rather than in PaneController so that panes
    * never need to know about each other.
    */
   void PropagateChannel(PaneController* source, SyncChannel channel, ChangeOrigin origin);

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace panes
} // namespace wxlens
