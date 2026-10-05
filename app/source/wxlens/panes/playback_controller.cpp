#include <wxlens/panes/playback_controller.hpp>

#include <QTimer>
#include <algorithm>
#include <optional>

namespace wxlens
{
namespace panes
{
class PlaybackController::Impl
{
public:
   QList<qint64> times;
   qint64 selected {0};
   std::optional<qint64> pending;
   quint64 generation {0};
   bool busy {false};
   bool playing {false};
   /// Whether the playhead tracks the newest scan. Live is the state the app starts and returns
   /// to; a seek or a play leaves it, because a user parked on a frame does not want the next
   /// volume to move them off it.
   bool following {true};
   QString error;
   QTimer debounce;
   QTimer animation;
};

PlaybackController::PlaybackController(QObject* parent) : QObject(parent), p(std::make_unique<Impl>())
{
   p->debounce.setSingleShot(true);
   p->debounce.setInterval(80);
   p->animation.setSingleShot(true);
   p->animation.setInterval(600);
   connect(&p->debounce, &QTimer::timeout, this, &PlaybackController::Dispatch);
   connect(&p->animation, &QTimer::timeout, this, [this]()
   {
      if (!p->playing || p->times.isEmpty()) return;
      const int next = (selectedIndex() + 1) % frameCount();
      p->pending = p->times[next];
      Dispatch();
   });
}
PlaybackController::~PlaybackController() = default;
int PlaybackController::frameCount() const { return static_cast<int>(p->times.size()); }
int PlaybackController::selectedIndex() const
{
   if (p->times.isEmpty()) return -1;
   const auto it = std::lower_bound(p->times.begin(), p->times.end(), p->selected);
   return it == p->times.end() ? frameCount() - 1 : static_cast<int>(it - p->times.begin());
}
bool PlaybackController::playing() const { return p->playing; }
bool PlaybackController::busy() const { return p->busy; }
QString PlaybackController::status() const
{
   if (!p->error.isEmpty()) return p->error;
   if (p->times.isEmpty()) return QStringLiteral("No recent scans available");
   return p->busy ? QStringLiteral("Loading scan…") : QStringLiteral("%1 scans").arg(frameCount());
}
void PlaybackController::setFrames(QList<qint64> times, const QString& error)
{
   std::sort(times.begin(), times.end());
   times.erase(std::unique(times.begin(), times.end()), times.end());
   p->times = std::move(times);
   p->error = error;
   if (p->following && !p->times.isEmpty()) p->selected = p->times.back();
   if (p->pending && !p->times.contains(*p->pending)) p->pending.reset();
   if (p->times.size() < 2) pause();
   Q_EMIT changed();
}
void PlaybackController::reset()
{
   pause();
   p->following = true;
   p->debounce.stop();
   p->pending.reset();
   ++p->generation;
   p->busy = false;
   p->times.clear();
   p->selected = 0;
   p->error.clear();
   Q_EMIT changed();
}
void PlaybackController::seek(int index)
{
   if (index < 0 || index >= frameCount()) return;
   pause();
   // Seeking to the newest scan is how a user asks to follow live again.
   p->following = index == frameCount() - 1;
   p->pending = p->times[index];
   p->selected = *p->pending;
   p->debounce.start();
   Q_EMIT changed();
}
void PlaybackController::step(int delta)
{
   if (frameCount() > 0) seek(std::clamp(selectedIndex() + delta, 0, frameCount() - 1));
}
void PlaybackController::togglePlaying()
{
   if (p->playing) { pause(); return; }
   if (frameCount() < 2) return;
   p->playing = true;
   p->following = false;
   if (!p->busy) p->animation.start();
   Q_EMIT changed();
}
void PlaybackController::pause()
{
   p->playing = false;
   p->animation.stop();
   Q_EMIT changed();
}
void PlaybackController::returnToLive() { Q_EMIT liveRequested(); }

void PlaybackController::Dispatch()
{
   if (p->busy || !p->pending) return;
   p->selected = *p->pending;
   p->pending.reset();
   p->busy = true;
   p->error.clear();
   const auto request = ++p->generation;
   Q_EMIT changed();
   Q_EMIT seekRequested(p->selected, request);
}
void PlaybackController::completeSeek(quint64 request, const QString& error)
{
   if (request != p->generation || !p->busy) return;
   p->busy = false;
   p->error = error;
   if (!error.isEmpty()) pause();
   if (p->pending) p->debounce.start();
   else if (p->playing) p->animation.start();
   Q_EMIT changed();
}
} // namespace panes
} // namespace wxlens
