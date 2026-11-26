#pragma once

#include <QObject>
#include <memory>

namespace wxlens
{
namespace panes
{
class PaneController;
class PlaybackController;
/** Connects the shared playhead to the active View and its source's availability. */
class PlaybackBinding : public QObject
{
   Q_OBJECT
public:
   explicit PlaybackBinding(PlaybackController& player, QObject* parent = nullptr);
   ~PlaybackBinding() override;
   void setPane(PaneController* pane);
   void refresh();
   void live();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};
} // namespace panes
} // namespace wxlens
