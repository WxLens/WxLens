#include <wxlens/panes/playback_controller.hpp>
#include <QEventLoop>
#include <QTimer>
#include <gtest/gtest.h>

namespace wxlens
{
namespace panes
{
namespace
{
void Events(int milliseconds)
{
   QEventLoop loop;
   QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
   loop.exec();
}

TEST(Playback, UsesOnlyRealTimesAndCoalescesSeeks)
{
   PlaybackController player;
   player.setFrames({1000, 2000, 7000, 2000});
   EXPECT_EQ(player.frameCount(), 3);
   QList<qint64> requested;
   quint64 request = 0;
   QObject::connect(&player, &PlaybackController::seekRequested,
      [&](qint64 time, quint64 id) { requested.append(time); request = id; });
   player.seek(0);
   player.seek(1);
   Events(150);
   ASSERT_EQ(requested, QList<qint64>({2000}));
   player.seek(0);
   player.seek(2);
   Events(150);
   EXPECT_EQ(requested.size(), 1); // Never starts a second load while the first is pending.
   player.completeSeek(request);
   Events(150);
   EXPECT_EQ(requested, QList<qint64>({2000, 7000}));
   player.completeSeek(request, "Missing scan");
   EXPECT_EQ(player.status(), "Missing scan");
   EXPECT_FALSE(player.playing());
}

TEST(Playback, FollowsTheNewestScanUntilTheUserLeavesLive)
{
   PlaybackController player;
   player.setFrames({1000, 2000});
   EXPECT_EQ(player.selectedIndex(), 1);
   player.setFrames({1000, 2000, 3000});
   EXPECT_EQ(player.selectedIndex(), 2); // A new scan moves an untouched playhead forward.

   player.seek(0);
   Events(150);
   player.setFrames({1000, 2000, 3000, 4000});
   EXPECT_EQ(player.selectedIndex(), 0); // A parked playhead stays where the user put it.

   player.seek(3);
   Events(150);
   player.setFrames({1000, 2000, 3000, 4000, 5000});
   EXPECT_EQ(player.selectedIndex(), 4); // Seeking to the newest scan rejoins live.

   player.seek(1);
   Events(150);
   player.reset();
   player.setFrames({7000, 8000});
   EXPECT_EQ(player.selectedIndex(), 1); // Return-to-live resets to the newest again.
}

TEST(Playback, OldSourceCompletionCannotCompleteNewSeek)
{
   PlaybackController player;
   quint64 request = 0;
   QObject::connect(&player, &PlaybackController::seekRequested,
      [&](qint64, quint64 id) { request = id; });
   player.setFrames({1000, 5000});
   player.seek(0);
   Events(150);
   const auto old = request;
   player.reset();
   player.setFrames({2000, 9000});
   player.seek(1);
   Events(150);
   player.completeSeek(old);
   EXPECT_TRUE(player.busy());
   player.completeSeek(request);
   EXPECT_FALSE(player.busy());
}

TEST(Playback, AnimationWaitsForLoadAndWraps)
{
   PlaybackController player;
   QList<qint64> requested;
   quint64 request = 0;
   QObject::connect(&player, &PlaybackController::seekRequested,
      [&](qint64 time, quint64 id) { requested.append(time); request = id; });
   player.setFrames({1000, 5000});
   player.togglePlaying();
   Events(750);
   ASSERT_EQ(requested, QList<qint64>({1000}));
   Events(750);
   EXPECT_EQ(requested.size(), 1);
   player.completeSeek(request);
   Events(750);
   EXPECT_EQ(requested, QList<qint64>({1000, 5000}));
   player.completeSeek(request, "Network unavailable");
   EXPECT_FALSE(player.playing());
}
} // namespace
} // namespace panes
} // namespace wxlens
