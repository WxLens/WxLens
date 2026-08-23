#include <nimbus/objects/object_tool_controller.hpp>
#include <nimbus/log/logger.hpp>
#include <nimbus/objects/map_object_store.hpp>
#include <nimbus/panes/pane_controller.hpp>

#include <QString>

namespace nimbus
{
namespace objects
{

static const std::string logPrefix_ = "objects.object_tool_controller";
static const auto        logger_    = nimbus::log::Create(logPrefix_);

namespace
{
// A sensible default ring for radar work: 50 km is a legible distance reference at typical
// single-site zoom levels, without implying anything about the radar's actual range.
constexpr double kDefaultRingRadiusMeters = 50000.0;
} // namespace

class ObjectToolController::Impl
{
public:
   Tool   activeTool_ {Tool::None};
   int    scopeKind_ {static_cast<int>(MapObjectScopeKind::CurrentPaneOnly)};
   double ringRadiusMeters_ {kDefaultRingRadiusMeters};
   int    markerCount_ {0};
};

ObjectToolController::ObjectToolController(QObject* parent) :
    QObject(parent), p {std::make_unique<Impl>()}
{
}

ObjectToolController::~ObjectToolController() = default;

int ObjectToolController::activeTool() const
{
   return static_cast<int>(p->activeTool_);
}

int ObjectToolController::scopeKind() const
{
   return p->scopeKind_;
}

double ObjectToolController::ringRadiusMeters() const
{
   return p->ringRadiusMeters_;
}

void ObjectToolController::setActiveTool(int tool)
{
   const auto requested = static_cast<Tool>(tool);
   if (p->activeTool_ == requested)
   {
      return;
   }
   p->activeTool_ = requested;
   Q_EMIT activeToolChanged();
}

void ObjectToolController::setScopeKind(int scopeKind)
{
   if (p->scopeKind_ == scopeKind)
   {
      return;
   }
   p->scopeKind_ = scopeKind;
   Q_EMIT scopeKindChanged();
}

void ObjectToolController::setRingRadiusMeters(double radiusMeters)
{
   if (p->ringRadiusMeters_ == radiusMeters || radiusMeters <= 0.0)
   {
      return;
   }
   p->ringRadiusMeters_ = radiusMeters;
   Q_EMIT ringRadiusMetersChanged();
}

int ObjectToolController::placeAt(double latitude, double longitude, panes::PaneController* pane)
{
   if (p->activeTool_ == Tool::None || pane == nullptr)
   {
      return -1;
   }

   auto& store = MapObjectStore::Instance();
   int   id    = -1;

   switch (p->activeTool_)
   {
   case Tool::Marker:
      id = store.addMarker(latitude,
                           longitude,
                           QStringLiteral("M%1").arg(++p->markerCount_),
                           pane,
                           p->scopeKind_);
      break;

   case Tool::RangeRing:
      id = store.addRangeRing(
         latitude, longitude, p->ringRadiusMeters_, QString {}, pane, p->scopeKind_);
      break;

   case Tool::None:
   default:
      return -1;
   }

   if (id >= 0)
   {
      logger_->info("Placed object {} from pane {} at {:.5f},{:.5f}",
                    id,
                    pane->paneId(),
                    latitude,
                    longitude);
      Q_EMIT objectPlaced(id);
   }

   return id;
}

} // namespace objects
} // namespace nimbus
