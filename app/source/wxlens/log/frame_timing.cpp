#include <wxlens/log/frame_timing.hpp>
#include <wxlens/log/logger.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QQuickWindow>
#include <QTimer>

#include <chrono>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace wxlens
{
namespace log
{
namespace
{
using Clock = std::chrono::steady_clock;
struct Sample
{
   double elapsedMs;
   double renderWallMs;
   double swapIntervalMs;
};

struct Capture
{
   QFile               file;
   std::mutex          mutex;
   std::vector<Sample> samples;
   Clock::time_point   start {Clock::now()};
   Clock::time_point   renderStart {};
   Clock::time_point   previousSwap {};
   double              renderWallMs {-1};
   std::size_t         dropped {0};

   static double Milliseconds(Clock::duration duration)
   { return std::chrono::duration<double, std::milli>(duration).count(); }

   // Runs on the GUI thread. Rendering callbacks only append bounded samples;
   // they never perform file I/O or log one message per frame.
   void Flush()
   {
      std::vector<Sample> batch;
      std::size_t         lost;
      {
         std::lock_guard lock {mutex};
         batch.swap(samples);
         lost = std::exchange(dropped, 0);
      }
      QByteArray bytes;
      for (const auto& sample : batch)
      {
         bytes += QByteArray::number(sample.elapsedMs, 'f', 3) + ',' +
                  QByteArray::number(sample.renderWallMs, 'f', 3) + ',' +
                  QByteArray::number(sample.swapIntervalMs, 'f', 3) + '\n';
      }
      if (!bytes.isEmpty() && file.isOpen())
      {
         if (file.write(bytes) != bytes.size() || !file.flush())
         {
            Create("log.frame_timing")->error("Frame capture write failed");
            file.close();
         }
      }
      if (lost)
         Create("log.frame_timing")
            ->warn("Frame capture dropped {} samples", lost);
   }
};
} // namespace

void AttachFrameTiming(QQuickWindow* window)
{
   const auto path = qEnvironmentVariable("WXLENS_FRAME_TIMINGS");
   if (path.isEmpty())
      return;
   auto capture = std::make_shared<Capture>();
   capture->file.setFileName(path);
   if (!capture->file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
   {
      Create("log.frame_timing")
         ->error("Cannot create frame capture: {}",
                 capture->file.errorString().toStdString());
      return;
   }
   capture->file.write("elapsed_ms,render_wall_ms,swap_interval_ms\n");
   capture->file.flush();
   Create("log.frame_timing")
      ->info("Frame capture started, UTC epoch ms {}",
             QDateTime::currentMSecsSinceEpoch());
   QObject::connect(
      window,
      &QQuickWindow::beforeRendering,
      window,
      [capture]()
      {
         std::lock_guard lock {capture->mutex};
         capture->renderStart = Clock::now();
      },
      Qt::DirectConnection);
   QObject::connect(
      window,
      &QQuickWindow::afterRendering,
      window,
      [capture]()
      {
         const auto      now = Clock::now();
         std::lock_guard lock {capture->mutex};
         if (capture->renderStart != Clock::time_point {})
            capture->renderWallMs =
               Capture::Milliseconds(now - capture->renderStart);
      },
      Qt::DirectConnection);
   QObject::connect(
      window,
      &QQuickWindow::frameSwapped,
      window,
      [capture]()
      {
         const auto      now = Clock::now();
         std::lock_guard lock {capture->mutex};
         const double    interval =
            capture->previousSwap == Clock::time_point {} ?
               -1 :
               Capture::Milliseconds(now - capture->previousSwap);
         capture->previousSwap = now;
         if (capture->samples.size() < 20000)
            capture->samples.push_back(
               {Capture::Milliseconds(now - capture->start),
                capture->renderWallMs,
                interval});
         else
            ++capture->dropped;
         capture->renderStart  = {};
         capture->renderWallMs = -1;
      },
      Qt::DirectConnection);
   auto* timer = new QTimer(window);
   QObject::connect(
      timer, &QTimer::timeout, window, [capture]() { capture->Flush(); });
   QObject::connect(QCoreApplication::instance(),
                    &QCoreApplication::aboutToQuit,
                    window,
                    [capture]() { capture->Flush(); });
   timer->start(1000);
}
} // namespace log
} // namespace wxlens
