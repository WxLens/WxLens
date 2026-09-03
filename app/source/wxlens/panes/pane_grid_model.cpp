#include <wxlens/panes/pane_grid_model.hpp>
#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/data/radar_site_database.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/products/product_descriptor.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace wxlens
{
namespace panes
{

static const std::string logPrefix_ = "panes.pane_grid_model";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

namespace
{
// Matches §4.6's "1x1 through 3x3(+)" - the cap is a guard against a runaway resize, not a
// statement that larger grids are architecturally special. Raise it when the UI can drive it.
constexpr int kMaxGridDimension = 4;

// What "link the camera" means in the pane chrome. Bearing and Pitch are included so a future
// rotation control is grouped correctly the day it exists, without revisiting this.
constexpr std::array<SyncChannel, 4> kCameraChannels {
   SyncChannel::Location, SyncChannel::Zoom, SyncChannel::Bearing, SyncChannel::Pitch};

constexpr std::array<SyncChannel, 10> kUserLinkChannels {
   SyncChannel::Location, SyncChannel::Zoom, SyncChannel::Bearing, SyncChannel::Pitch,
   SyncChannel::RadarSite, SyncChannel::Product, SyncChannel::Palette, SyncChannel::Time,
   SyncChannel::Animation, SyncChannel::SelectedStorm};

QString RegionName(const std::string& country, const std::string& region)
{
   static const std::map<std::string, QString> kUsRegions {
      {"AK", QStringLiteral("Alaska")}, {"AL", QStringLiteral("Alabama")},
      {"AR", QStringLiteral("Arkansas")}, {"AZ", QStringLiteral("Arizona")},
      {"CA", QStringLiteral("California")}, {"CO", QStringLiteral("Colorado")},
      {"DE", QStringLiteral("Delaware")}, {"FL", QStringLiteral("Florida")},
      {"GA", QStringLiteral("Georgia")}, {"GU", QStringLiteral("Guam")},
      {"HI", QStringLiteral("Hawaii")}, {"IA", QStringLiteral("Iowa")},
      {"ID", QStringLiteral("Idaho")}, {"IL", QStringLiteral("Illinois")},
      {"IN", QStringLiteral("Indiana")}, {"KS", QStringLiteral("Kansas")},
      {"KY", QStringLiteral("Kentucky")}, {"LA", QStringLiteral("Louisiana")},
      {"MA", QStringLiteral("Massachusetts")}, {"MD", QStringLiteral("Maryland")},
      {"ME", QStringLiteral("Maine")}, {"MI", QStringLiteral("Michigan")},
      {"MN", QStringLiteral("Minnesota")}, {"MO", QStringLiteral("Missouri")},
      {"MS", QStringLiteral("Mississippi")}, {"MT", QStringLiteral("Montana")},
      {"NC", QStringLiteral("North Carolina")}, {"ND", QStringLiteral("North Dakota")},
      {"NE", QStringLiteral("Nebraska")}, {"NJ", QStringLiteral("New Jersey")},
      {"NM", QStringLiteral("New Mexico")}, {"NV", QStringLiteral("Nevada")},
      {"NY", QStringLiteral("New York")}, {"OH", QStringLiteral("Ohio")},
      {"OK", QStringLiteral("Oklahoma")}, {"OR", QStringLiteral("Oregon")},
      {"PA", QStringLiteral("Pennsylvania")}, {"PR", QStringLiteral("Puerto Rico")},
      {"SC", QStringLiteral("South Carolina")}, {"SD", QStringLiteral("South Dakota")},
      {"TN", QStringLiteral("Tennessee")}, {"TX", QStringLiteral("Texas")},
      {"UT", QStringLiteral("Utah")}, {"VA", QStringLiteral("Virginia")},
      {"VT", QStringLiteral("Vermont")}, {"WA", QStringLiteral("Washington")},
      {"WI", QStringLiteral("Wisconsin")}, {"WV", QStringLiteral("West Virginia")},
      {"WY", QStringLiteral("Wyoming")}};
   if (country != "USA") return QString::fromStdString(region);
   const auto it = kUsRegions.find(region);
   return it == kUsRegions.end() ? QString::fromStdString(region) : it->second;
}

QString CountryName(const std::string& country)
{
   if (country == "USA") return QStringLiteral("United States America");
   if (country == "JPN") return QStringLiteral("Japan");
   if (country == "KOR") return QStringLiteral("South Korea Republic of Korea");
   return QString::fromStdString(country);
}
} // namespace

class PaneGridModel::Impl
{
public:
   int gridWidth_ {1};
   int gridHeight_ {1};

   QString defaultSourceKey_ {};
   int     nextPaneId_ {0};
   int     syncRevision_ {0};
   int     activePaneIndex_ {0};

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

int PaneGridModel::syncRevision() const
{
   return p->syncRevision_;
}

int PaneGridModel::firstPaneId() const
{
   return p->panes_.empty() ? 0 : p->panes_.front()->paneId();
}

QObject* PaneGridModel::activePane() const
{
   return p->panes_.empty()
      ? nullptr : p->panes_[static_cast<std::size_t>(p->activePaneIndex_)].get();
}

int PaneGridModel::activePaneIndex() const { return p->activePaneIndex_; }

QVariantList PaneGridModel::radarSites() const
{
   QVariantList result;
   for (const data::RadarSiteInfo& site : data::RadarSites())
   {
      QVariantMap item;
      item[QStringLiteral("id")] = QString::fromStdString(site.id);
      item[QStringLiteral("name")] = QString::fromStdString(site.place);
      item[QStringLiteral("region")] = QString::fromStdString(site.state);
      item[QStringLiteral("country")] = QString::fromStdString(site.country);
      const QString regionName = RegionName(site.country, site.state);
      item[QStringLiteral("regionName")] = regionName;
      item[QStringLiteral("searchText")] =
         QStringLiteral("%1 %2 %3 %4 %5 %6")
            .arg(QString::fromStdString(site.id),
                 QString::fromStdString(site.place),
                 QString::fromStdString(site.state),
                 regionName,
                 QString::fromStdString(site.country),
                 CountryName(site.country))
            .toLower();
      item[QStringLiteral("latitude")] = site.latitude;
      item[QStringLiteral("longitude")] = site.longitude;
      result.append(item);
   }
   return result;
}

void PaneGridModel::setActivePaneIndex(int index)
{
   const int visiblePaneCount = p->gridWidth_ * p->gridHeight_;
   if (index < 0 || index >= visiblePaneCount ||
       index == p->activePaneIndex_)
   {
      return;
   }
   p->activePaneIndex_ = index;
   Q_EMIT activePaneChanged();
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

   logger_->info("Grid resized to {}x{} ({} visible panes)", width, height, desired);

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

         PaneController* pane = p->panes_.back().get();
         connect(pane,
                 &PaneController::channelChanged,
                 this,
                 [this, pane](SyncChannel channel, ChangeOrigin origin)
                 { PropagateChannel(pane, channel, origin); });
      }
      endInsertRows();
   }

   // Do not remove trailing pane controllers when a layout shrinks. Their PaneHost delegates own
   // MapLibre quick items, whose renderer teardown requires a current GL context; destroying them
   // from the model-change event can crash inside QOpenGLContext::functions(). Retaining and
   // hiding them also gives the 1x1/last-layout toggle genuine pane-state memory.
   if (p->activePaneIndex_ >= static_cast<int>(desired))
   {
      p->activePaneIndex_ = std::max(0, static_cast<int>(desired) - 1);
      Q_EMIT activePaneChanged();
   }

   if (current == 0 && desired > 0)
   {
      Q_EMIT activePaneChanged();
   }
}

void PaneGridModel::PropagateChannel(PaneController* source,
                                     SyncChannel     channel,
                                     ChangeOrigin    origin)
{
   // §4.2: only a genuine user interaction fans out. An incoming sync re-emits with
   // ProgrammaticSync, which lands here and stops - that is what prevents two grouped panes from
   // bouncing a value between them forever.
   if (origin != ChangeOrigin::UserInput)
   {
      return;
   }

   const SyncGroupId group = source->syncGroup(channel);
   if (group == kNoSyncGroup)
   {
      return;
   }

   const QVariant value = source->channelValue(channel);
   if (!value.isValid())
   {
      return;
   }

   for (const auto& pane : p->panes_)
   {
      if (pane.get() == source || pane->syncGroup(channel) != group)
      {
         continue;
      }

      pane->applyChannelValue(channel, value, ChangeOrigin::ProgrammaticSync);
   }
}

void PaneGridModel::copyChannel(int fromPaneId, int toPaneId, SyncChannel channel)
{
   PaneController* from = nullptr;
   PaneController* to   = nullptr;

   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() == fromPaneId)
      {
         from = pane.get();
      }
      if (pane->paneId() == toPaneId)
      {
         to = pane.get();
      }
   }

   if (from == nullptr || to == nullptr || from == to)
   {
      return;
   }

   // ProgrammaticSync, so a one-shot copy never cascades into the target's own groups - the user
   // asked to match one pane to another, not to re-broadcast from the target.
   to->applyChannelValue(channel, from->channelValue(channel), ChangeOrigin::ProgrammaticSync);
}

void PaneGridModel::copyCamera(int fromPaneId, int toPaneId)
{
   for (const SyncChannel channel : kCameraChannels)
   {
      copyChannel(fromPaneId, toPaneId, channel);
   }
}

void PaneGridModel::setCameraSyncGroup(int paneId, int groupId)
{
   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() != paneId)
      {
         continue;
      }

      for (const SyncChannel channel : kCameraChannels)
      {
         pane->setSyncGroup(channel, groupId);
      }

      logger_->info("Pane {} camera sync group set to {}", paneId, groupId);

      ++p->syncRevision_;
      Q_EMIT syncRevisionChanged();

      // Joining a group does not retroactively move anything (§4.1) - panes converge on the next
      // change. Adopting the group's current view immediately is the friendlier behaviour here,
      // so a newly linked pane matches what it just joined instead of staying put until the user
      // happens to pan. Done as an explicit one-shot copy from an existing member, which is
      // exactly the distinction §4.1 asks to keep visible.
      if (groupId != kNoSyncGroup)
      {
         for (const auto& other : p->panes_)
         {
            if (other.get() == pane.get() ||
                other->syncGroup(SyncChannel::Location) != groupId)
            {
               continue;
            }

            for (const SyncChannel channel : kCameraChannels)
            {
               pane->applyChannelValue(
                  channel, other->channelValue(channel), ChangeOrigin::ProgrammaticSync);
            }
            break;
         }
      }

      return;
   }
}

int PaneGridModel::cameraSyncGroup(int paneId) const
{
   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() == paneId)
      {
         // Location is the representative channel: setCameraSyncGroup always moves the whole set
         // together, so any one of them answers for the group as a whole.
         return pane->syncGroup(SyncChannel::Location);
      }
   }
   return kNoSyncGroup;
}

void PaneGridModel::setSyncPreset(int paneId, const QString& preset, int groupId)
{
   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() != paneId)
      {
         continue;
      }

      const int targetGroup = preset == QStringLiteral("independent") ? kNoSyncGroup : groupId;
      for (const SyncChannel channel : kUserLinkChannels)
      {
         bool selected = preset == QStringLiteral("all");
         selected = selected ||
                    ((preset == QStringLiteral("map") || preset == QStringLiteral("map-site")) &&
                     std::find(kCameraChannels.begin(), kCameraChannels.end(), channel) !=
                        kCameraChannels.end());
         selected = selected || (preset == QStringLiteral("map-site") &&
                                 channel == SyncChannel::RadarSite);
         selected = selected ||
                    (preset == QStringLiteral("palette") && channel == SyncChannel::Palette);
         pane->setSyncGroup(channel, selected ? targetGroup : kNoSyncGroup);
      }

      // A preset joins an existing group with that group's current values, matching the
      // established camera-link behaviour. Palette-only therefore repaints immediately and does
      // not have to wait for the next edit before the panes visibly agree.
      if (targetGroup != kNoSyncGroup)
      {
         for (const auto& other : p->panes_)
         {
            if (other.get() == pane.get())
               continue;
            bool copiedAny = false;
            for (const SyncChannel channel : kUserLinkChannels)
            {
               if (pane->syncGroup(channel) == targetGroup &&
                   other->syncGroup(channel) == targetGroup)
               {
                  pane->applyChannelValue(
                     channel, other->channelValue(channel), ChangeOrigin::ProgrammaticSync);
                  copiedAny = true;
               }
            }
            if (copiedAny)
               break;
         }
      }

      ++p->syncRevision_;
      Q_EMIT syncRevisionChanged();
      logger_->info("Pane {} sync preset '{}' set to group {}",
                    paneId,
                    preset.toStdString(),
                    targetGroup);
      return;
   }
}

QString PaneGridModel::syncPreset(int paneId) const
{
   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() != paneId)
      {
         continue;
      }

      const int group = syncGroupForPreset(paneId);
      if (group == kNoSyncGroup)
      {
         return QStringLiteral("independent");
      }
      const auto linked = [&](SyncChannel channel) { return pane->syncGroup(channel) == group; };
      const bool camera = std::all_of(kCameraChannels.begin(), kCameraChannels.end(), linked);
      const bool site = linked(SyncChannel::RadarSite);
      const bool product = linked(SyncChannel::Product);
      const bool palette = linked(SyncChannel::Palette);
      const bool time = linked(SyncChannel::Time);
      const bool animation = linked(SyncChannel::Animation);
      const bool selectedStorm = linked(SyncChannel::SelectedStorm);

      if (camera && site && product && palette && time && animation && selectedStorm)
         return QStringLiteral("all");
      if (camera && site && !product && !palette && !time && !animation && !selectedStorm)
         return QStringLiteral("map-site");
      if (camera && !site && !product && !palette && !time && !animation && !selectedStorm)
         return QStringLiteral("map");
      if (!camera && !site && !product && palette && !time && !animation && !selectedStorm)
         return QStringLiteral("palette");
      return QStringLiteral("custom");
   }
   return QStringLiteral("independent");
}

int PaneGridModel::syncGroupForPreset(int paneId) const
{
   for (const auto& pane : p->panes_)
   {
      if (pane->paneId() != paneId)
         continue;
      for (const SyncChannel channel : kUserLinkChannels)
      {
         const int group = pane->syncGroup(channel);
         if (group != kNoSyncGroup)
            return group;
      }
   }
   return kNoSyncGroup;
}

} // namespace panes
} // namespace wxlens
