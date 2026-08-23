#pragma once

#include <nimbus/panes/sync_types.hpp>

#include <QObject>
#include <QString>
#include <QVector>

namespace nimbus
{
namespace objects
{

Q_NAMESPACE

/**
 * One unified object family for markers, drawings, range rings and (from slice 7) measurements -
 * docs/ROADMAP.md §4.3. The legacy app had two parallel systems (globally-scoped markers, and
 * per-map annotations); this deliberately replaces both rather than reproducing the split.
 */
enum class MapObjectType
{
   Marker,         ///< a single point of interest
   Line,           ///< open polyline (freehand drawing or straight segments)
   Polygon,        ///< closed area
   RangeRing,      ///< circle of a fixed ground radius about a centre
   TextAnnotation, ///< a label anchored to a point
   Measurement     ///< a pinned measurement path; geometry is its vertices in order
};
Q_ENUM_NS(MapObjectType)

/**
 * Three-tier lifecycle (§4.3). The point is that probing the map constantly must not litter it
 * with permanent objects.
 *
 * `Temporary` objects never enter the store at all - they are UI state belonging to whatever tool
 * is active, and vanish when the interaction ends. Only Pinned and Saved are stored.
 */
enum class MapObjectLifecycle
{
   Temporary, ///< live interaction state; never stored (see MapObjectStore::Add)
   Pinned,    ///< explicitly committed by the user; lives for the session
   Saved      ///< persisted across restarts
};
Q_ENUM_NS(MapObjectLifecycle)

/**
 * Which panes an object appears in (§4.3). This is the field that makes objects first-class
 * across a multi-pane workspace rather than stuck to whichever pane drew them.
 */
enum class MapObjectScopeKind
{
   CurrentPaneOnly, ///< only the pane it was created in
   SyncGroup,       ///< every pane sharing the origin pane's group on a given channel
   SameLocation,    ///< every pane currently centred on roughly the same place
   AllPanes         ///< everywhere
};
Q_ENUM_NS(MapObjectScopeKind)

/**
 * How far apart two pane centres may be and still count as "the same location" for
 * MapObjectScopeKind::SameLocation. Degrees, compared per-axis - deliberately coarse, since the
 * intent is "looking at the same storm", not "identical camera".
 */
inline constexpr double kSameLocationToleranceDegrees = 0.5;

struct MapObjectScope
{
   MapObjectScopeKind kind {MapObjectScopeKind::CurrentPaneOnly};

   /// Which pane created the object. Used by CurrentPaneOnly, SyncGroup and SameLocation.
   int originPaneId {-1};

   /// For SyncGroup: which channel's grouping decides visibility. Location is the natural default
   /// for what a user would call "linked panes".
   panes::SyncChannel channel {panes::SyncChannel::Location};

   /// For SyncGroup: the group captured when the object was created. Stored rather than looked up
   /// from the origin pane at draw time, so the object belongs to *that group* - if the pane that
   /// drew it later leaves, the object stays with the group it was shared into, and does not
   /// silently follow the author around.
   panes::SyncGroupId originGroupId {panes::kNoSyncGroup};
};

/**
 * A geographically-anchored object. Geometry is always in degrees, never screen space, so objects
 * stay put as panes pan, zoom and rotate independently of one another.
 *
 * `points` means different things per type: one point for Marker/TextAnnotation/RangeRing (its
 * centre), an ordered path for Line, and a closed ring for Polygon.
 */
struct MapObject
{
   int           id {-1};
   MapObjectType type {MapObjectType::Marker};

   QVector<double> latitudes {};
   QVector<double> longitudes {};

   /// RangeRing only: ground radius in metres. A real ground distance, not a pixel radius, so the
   /// ring stays geographically correct at any zoom.
   double radiusMeters {0.0};

   QString label {};
   QString color {QStringLiteral("#ffb300")};

   MapObjectScope     scope {};
   MapObjectLifecycle lifecycle {MapObjectLifecycle::Pinned};
};

} // namespace objects
} // namespace nimbus
