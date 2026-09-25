#include <wxlens/panes/playback_binding.hpp>
#include <wxlens/panes/playback_controller.hpp>
#include <wxlens/panes/pane_controller.hpp>
#include <wxlens/data/radar_site_data_service.hpp>

#include <QPointer>
#include <QTimer>

namespace wxlens
{
namespace panes
{
class PlaybackBinding::Impl
{
public:
   explicit Impl(PlaybackController& controller) : player(controller) {}
   PlaybackController& player;
   QPointer<PaneController> pane;
   std::shared_ptr<data::RadarSiteDataService> service;
   QMetaObject::Connection framesConnection;
   QMetaObject::Connection productConnection;
   QMetaObject::Connection timeConnection;
   quint64 request {0};
   QTimer watchdog;
};

PlaybackBinding::PlaybackBinding(PlaybackController& player, QObject* parent) :
    QObject(parent), p(std::make_unique<Impl>(player))
{
   p->watchdog.setSingleShot(true);
   p->watchdog.setInterval(60000);
   connect(&p->watchdog, &QTimer::timeout, this, [this]()
   { p->player.completeSeek(p->request, QStringLiteral("Scan request timed out")); });
   connect(&player, &PlaybackController::seekRequested, this, [this](qint64 time, quint64 request)
   {
      p->request = request;
      if (!p->pane || p->pane->level3Product())
      {
         p->player.completeSeek(request, QStringLiteral("Playback is available for Level 2 products"));
         return;
      }
      p->watchdog.start();
      p->pane->applyChannelValue(SyncChannel::Time, QDateTime::fromMSecsSinceEpoch(time, Qt::UTC),
                                ChangeOrigin::DataDriven);
   });
}
PlaybackBinding::~PlaybackBinding() = default;

void PlaybackBinding::setPane(PaneController* pane)
{
   QObject::disconnect(p->framesConnection);
   QObject::disconnect(p->productConnection);
   QObject::disconnect(p->timeConnection);
   p->watchdog.stop();
   p->player.reset();
   p->pane = pane;
   p->service.reset();
   if (!pane) return;
   p->productConnection = connect(pane, &PaneController::productChanged, this,
                                  [this]() { setPane(p->pane); });
   p->timeConnection = connect(pane, &PaneController::timeChanged, this, [this]()
   {
      if (!p->pane) return;
      if (!p->pane->timeLoading())
      {
         p->watchdog.stop();
         p->player.completeSeek(p->request, p->pane->timeError());
      }
   });
   if (pane->level3Product())
   {
      p->player.setFrames({}, QStringLiteral("Playback is available for Level 2 products"));
      return;
   }
   if (pane->sourceKey().isEmpty()) return;
   p->service = data::RadarSiteDataService::Instance(pane->sourceKey().toStdString());
   p->framesConnection = connect(p->service.get(), &data::RadarSiteDataService::RecentFramesChanged,
                                 this, [this](QList<qint64> frames, QString error)
   { p->player.setFrames(std::move(frames), error); });
   p->player.setFrames(p->service->recentFrames());
   // Defer history until the initial foreground volume has finished loading.
   p->service->RequestRecentHistory();
}
void PlaybackBinding::refresh()
{
   if (p->service) p->service->RequestRecentHistory();
}
void PlaybackBinding::live()
{
   p->player.reset();
   p->watchdog.stop();
   if (p->pane) p->pane->selectLive();
   if (p->service) p->player.setFrames(p->service->recentFrames());
}
} // namespace panes
} // namespace wxlens
