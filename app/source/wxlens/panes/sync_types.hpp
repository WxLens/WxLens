#pragma once

#include <QObject>

namespace wxlens
{
namespace panes
{

Q_NAMESPACE

/**
 * The properties panes can be synchronized on, independently of one another (docs/ROADMAP.md
 * §4.1). Two panes are synchronized on a channel exactly when they hold the same non-null group
 * id for that channel - there is deliberately no global "linked" flag anywhere in this design.
 *
 * The full channel set is declared up front because the primitive is what makes the user-facing
 * modes fall out for free: "same location, independent zoom" is just Location grouped and Zoom
 * not; "full camera link" is Location+Zoom+Bearing+Pitch in one group. Channels whose underlying
 * state does not exist yet (Time, Animation, Cursor, SelectedStorm, Palette) are declared but
 * carry no value - see PaneController::channelValue. Declaring them costs nothing and keeps the
 * enum from being renumbered later, when persisted layouts would care.
 */
enum class SyncChannel
{
   Location,      ///< centre latitude/longitude, as a pair
   Zoom,          ///< zoom level
   Bearing,       ///< map rotation
   Pitch,         ///< map tilt
   Time,          ///< timeline cursor / archive time (no state yet)
   Animation,     ///< play/pause/speed (no state yet)
   Cursor,        ///< hover/crosshair position, for synchronized probing (no state yet)
   SelectedStorm, ///< selected storm cell (no state yet; needs Level 3 STI)
   RadarSite,     ///< the pane's data source
   Product,       ///< the product shown from that source
   Palette        ///< colour table (no state yet)
};
Q_ENUM_NS(SyncChannel)

/**
 * Where a change came from (docs/ROADMAP.md §4.2). This is the reentrancy guard: a change only
 * fans out to grouped panes when it originated from `UserInput` (or an explicit one-shot apply),
 * never when it was itself the result of an incoming sync. Without this, two grouped panes would
 * bounce a value back and forth forever.
 *
 * The distinction has to hold per channel independently, so that e.g. an incoming Location sync
 * can never cascade into a Zoom change.
 */
enum class ChangeOrigin
{
   UserInput,         ///< a person interacted with this pane - the only origin that fans out
   ProgrammaticSync,  ///< applied by the sync coordinator; must not fan out again
   DataDriven,        ///< e.g. an animation timer advancing Time
   FollowPropagation  ///< reserved for directional "follow" semantics layered on grouping
};
Q_ENUM_NS(ChangeOrigin)

/**
 * A pane's group membership on one channel. Panes sharing a non-null id on the same channel are
 * synchronized on it. `kNoSyncGroup` means "independent on this channel", which is every pane's
 * default and is always reachable again - leaving a group is a first-class action, never an
 * all-or-nothing global unlink.
 */
using SyncGroupId = int;

inline constexpr SyncGroupId kNoSyncGroup = 0;

} // namespace panes
} // namespace wxlens
