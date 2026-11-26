#pragma once

#include <QObject>
#include <QList>
#include <memory>

namespace wxlens
{
namespace panes
{
/** A single playhead over real source timestamps. Scheduling is independent of radar/QML. */
class PlaybackController : public QObject
{
   Q_OBJECT
   Q_PROPERTY(int frameCount READ frameCount NOTIFY changed)
   Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
   Q_PROPERTY(bool playing READ playing NOTIFY changed)
   Q_PROPERTY(bool busy READ busy NOTIFY changed)
   Q_PROPERTY(QString status READ status NOTIFY changed)

public:
   explicit PlaybackController(QObject* parent = nullptr);
   ~PlaybackController() override;
   int frameCount() const;
   int selectedIndex() const;
   bool playing() const;
   bool busy() const;
   QString status() const;
   void setFrames(QList<qint64> times, const QString& error = {});
   void reset();
   void completeSeek(quint64 request, const QString& error = {});
   Q_INVOKABLE void seek(int index);
   Q_INVOKABLE void step(int delta);
   Q_INVOKABLE void togglePlaying();
   Q_INVOKABLE void pause();
   Q_INVOKABLE void returnToLive();

signals:
   void changed();
   void liveRequested();
   void seekRequested(qint64 timeMs, quint64 request);

private:
   void Dispatch();
   class Impl;
   std::unique_ptr<Impl> p;
};
} // namespace panes
} // namespace wxlens
