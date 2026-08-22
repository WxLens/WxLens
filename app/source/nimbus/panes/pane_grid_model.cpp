#include <nimbus/panes/pane_grid_model.hpp>
#include <nimbus/panes/pane_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/products/product_descriptor.hpp>

#include <algorithm>
#include <vector>

namespace nimbus
{
namespace panes
{

static const std::string logPrefix_ = "panes.pane_grid_model";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
// Matches §4.6's "1x1 through 3x3(+)" - the cap is a guard against a runaway resize, not a
// statement that larger grids are architecturally special. Raise it when the UI can drive it.
constexpr int kMaxGridDimension = 4;
} // namespace

class PaneGridModel::Impl
{
public:
   int gridWidth_ {1};
   int gridHeight_ {1};

   QString defaultSourceKey_ {};
   int     nextPaneId_ {0};

   std::vector<std::unique_ptr<PaneController>> panes_;
};

PaneGridModel::PaneGridModel(QObject* parent) :
    QAbstractListModel(parent), p {std::make_unique<Impl>()}
{
}

PaneGridModel::~PaneGridModel() = default;

int PaneGridModel::gridWidth() const
{
   return p->gridWidth_;
}

int PaneGridModel::gridHeight() const
{
   return p->gridHeight_;
}

int PaneGridModel::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid())
   {
      return 0;
   }
   return static_cast<int>(p->panes_.size());
}

QVariant PaneGridModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || index.row() < 0 ||
       index.row() >= static_cast<int>(p->panes_.size()) || role != PaneRole)
   {
      return {};
   }

   return QVariant::fromValue(p->panes_[static_cast<std::size_t>(index.row())].get());
}

QHash<int, QByteArray> PaneGridModel::roleNames() const
{
   return {{PaneRole, QByteArrayLiteral("pane")}};
}

void PaneGridModel::setDefaultSourceKey(const QString& sourceKey)
{
   p->defaultSourceKey_ = sourceKey;

   if (p->panes_.empty())
   {
      // Nothing has been created yet - materialize the initial grid now that new panes have a
      // source to bind to.
      setGridSize(p->gridWidth_, p->gridHeight_);
   }
}

void PaneGridModel::setGridSize(int width, int height)
{
   width  = std::clamp(width, 1, kMaxGridDimension);
   height = std::clamp(height, 1, kMaxGridDimension);

   const auto desired = static_cast<std::size_t>(width * height);
   const auto current = p->panes_.size();

   if (width != p->gridWidth_ || height != p->gridHeight_)
   {
      p->gridWidth_  = width;
      p->gridHeight_ = height;
      Q_EMIT gridSizeChanged();
   }

   if (desired == current)
   {
      return;
   }

   logger_->info("Grid resized to {}x{} ({} panes)", width, height, desired);

   if (desired > current)
   {
      beginInsertRows(QModelIndex(),
                      static_cast<int>(current),
                      static_cast<int>(desired - 1));
      for (std::size_t i = current; i < desired; ++i)
      {
         products::ProductDescriptor descriptor {};
         descriptor.sourceKey = p->defaultSourceKey_;

         // No QObject parent: the unique_ptr owns this. Setting a parent too would give Qt's
         // parent-child cleanup a second claim on the same object.
         p->panes_.push_back(std::make_unique<PaneController>(p->nextPaneId_++, descriptor));
      }
      endInsertRows();
   }
   else
   {
      beginRemoveRows(QModelIndex(),
                      static_cast<int>(desired),
                      static_cast<int>(current - 1));
      p->panes_.resize(desired);
      endRemoveRows();
   }
}

} // namespace panes
} // namespace nimbus
