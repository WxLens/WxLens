#include <wxlens/objects/object_tool_controller.hpp>
#include <wxlens/log/logger.hpp>
#include <wxlens/objects/map_object_store.hpp>
#include <wxlens/panes/pane_controller.hpp>

#include <QString>

namespace wxlens
{
namespace objects
{

static const std::string logPrefix_ = "objects.object_tool_controller";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

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
   QVariantList drawingLatitudes_ {};
   QVariantList drawingLongitudes_ {};
   int drawingCount_ {0};
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

bool ObjectToolController::drawingActive() const { return !p->drawingLatitudes_.isEmpty(); }
QVariantList ObjectToolController::drawingLatitudes() const { return p->drawingLatitudes_; }
QVariantList ObjectToolController::drawingLongitudes() const { return p->drawingLongitudes_; }

void ObjectToolController::setActiveTool(int tool)
{
   const auto requested = static_cast<Tool>(tool);
   if (p->activeTool_ == requested)
   {
      return;
   }
   p->activeTool_ = requested;
   if (requested != Tool::Drawing)
   {
      cancelDrawing();
   }
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

   case Tool::Drawing:
      return -1;

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

void ObjectToolController::beginDrawing(double latitude, double longitude)
{
   p->drawingLatitudes_  = {latitude};
   p->drawingLongitudes_ = {longitude};
   Q_EMIT drawingChanged();
}

void ObjectToolController::appendDrawingPoint(double latitude, double longitude)
{
   if (p->drawingLatitudes_.isEmpty())
   {
      beginDrawing(latitude, longitude);
      return;
   }
   p->drawingLatitudes_.append(latitude);
   p->drawingLongitudes_.append(longitude);
   Q_EMIT drawingChanged();
}

int ObjectToolController::commitDrawing(panes::PaneController* pane)
{
   const int id = MapObjectStore::Instance().addLine(p->drawingLatitudes_,
                                                     p->drawingLongitudes_,
                                                     QStringLiteral("D%1").arg(++p->drawingCount_),
                                                     pane,
                                                     p->scopeKind_);
   cancelDrawing();
   if (id >= 0)
   {
      Q_EMIT objectPlaced(id);
   }
   return id;
}

void ObjectToolController::cancelDrawing()
{
   if (p->drawingLatitudes_.isEmpty()) return;
   p->drawingLatitudes_.clear();
   p->drawingLongitudes_.clear();
   Q_EMIT drawingChanged();
}

} // namespace objects
} // namespace wxlens
