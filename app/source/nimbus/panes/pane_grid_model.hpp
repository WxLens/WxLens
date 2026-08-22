#pragma once

#include <memory>

#include <QAbstractListModel>

namespace nimbus
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
 * ones, so a user adding a pane doesn't lose the sites/cameras they already set up. Shrinking
 * drops the trailing panes.
 */
class PaneGridModel : public QAbstractListModel
{
   Q_OBJECT

   Q_PROPERTY(int gridWidth READ gridWidth NOTIFY gridSizeChanged)
   Q_PROPERTY(int gridHeight READ gridHeight NOTIFY gridSizeChanged)

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

   [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
   [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

   Q_INVOKABLE void setGridSize(int width, int height);

   /** The default radar site new panes are created with, until pane chrome can set it (§4.5). */
   void setDefaultSourceKey(const QString& sourceKey);

signals:
   void gridSizeChanged();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace panes
} // namespace nimbus
