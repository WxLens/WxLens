#include <wxlens/data/radar_task_queue.hpp>
#include <wxlens/log/logger.hpp>

#include <scwx/util/threads.hpp>

#include <algorithm>
#include <deque>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace wxlens
{
namespace data
{

static const std::string logPrefix_ = "data.radar_task_queue";
static const auto        logger_    = wxlens::log::Create(logPrefix_);

namespace
{

/**
 * Default bound on concurrent provider work.
 *
 * Not `hardware_concurrency()`: these tasks are dominated by one S3 transfer each, so the useful
 * limit is how many transfers the connection sustains without each one slowing the others, not
 * how many cores exist. Four matches the io_context runner count main() now starts, and keeps a
 * 3x3 workspace from opening nine simultaneous downloads the moment it is created.
 */
constexpr std::size_t kDefaultMaxConcurrency = 4;

} // namespace

class RadarTaskQueue::Impl
{
public:
   struct Entry
   {
      std::string           serialKey;
      std::function<void()> task;
      /// Set by TakeDispatchableLocked so Finish can decrement the right counter.
      bool background {false};
   };

   mutable std::mutex mutex_;
   std::deque<Entry>  foreground_;
   std::deque<Entry>  background_;

   /// Serial keys with a task currently executing. An entry whose key is in here is not
   /// dispatchable, however long it has waited.
   std::unordered_map<std::string, std::size_t> activeKeys_;

   std::condition_variable idle_;
   bool                    shuttingDown_ {false};

   std::size_t running_ {0};
   std::size_t runningBackground_ {0};
   std::size_t maxConcurrency_ {kDefaultMaxConcurrency};

   /**
    * Background work never fills the last slot.
    *
    * Priority ordering alone is not enough: if every slot is already occupied by history
    * warming, a foreground site switch still waits for one of those ~6 s downloads to finish
    * before it can even start. Reserving a slot bounds a foreground task's wait at "until a
    * slot frees", not "until a background download completes".
    */
   [[nodiscard]] std::size_t background_bound() const
   {
      return (maxConcurrency_ > 1) ? maxConcurrency_ - 1 : 1;
   }

   /// Pops the highest-priority dispatchable entry, or nullopt when the bound is reached or
   /// every waiting entry is blocked on a busy serial key. Caller holds mutex_.
   std::optional<Entry> TakeDispatchableLocked()
   {
      if (shuttingDown_ || running_ >= maxConcurrency_)
      {
         return std::nullopt;
      }

      const bool backgroundAllowed = runningBackground_ < background_bound();

      for (const bool isBackground : {false, true})
      {
         if (isBackground && !backgroundAllowed)
         {
            break;
         }

         auto& queue = isBackground ? background_ : foreground_;
         for (auto it = queue.begin(); it != queue.end(); ++it)
         {
            // An empty key carries no serialization constraint, so it is always dispatchable.
            if (!it->serialKey.empty() && activeKeys_.contains(it->serialKey))
            {
               continue;
            }

            Entry entry = std::move(*it);
            queue.erase(it);
            if (!entry.serialKey.empty())
            {
               ++activeKeys_[entry.serialKey];
            }
            ++running_;
            if (isBackground)
            {
               ++runningBackground_;
            }
            entry.background = isBackground;
            return entry;
         }
      }

      return std::nullopt;
   }

   void Finish(const std::string& serialKey, bool background)
   {
      std::lock_guard lock {mutex_};
      if (!serialKey.empty())
      {
         if (auto it = activeKeys_.find(serialKey); it != activeKeys_.end())
         {
            if (--it->second == 0)
            {
               activeKeys_.erase(it);
            }
         }
      }
      --running_;
      if (background)
      {
         --runningBackground_;
      }
      if (running_ == 0)
      {
         idle_.notify_all();
      }
   }

   /**
    * Dispatches as many entries as the bound and the serial keys allow.
    *
    * Never called with mutex_ held by the caller, and never runs a task inside the lock: a task
    * that posts its own continuation would otherwise deadlock on a non-recursive mutex.
    */
   /**
    * Dispatches as many entries as the bound and the serial keys allow.
    *
    * `self` is the Impl's own shared_ptr, carried into every dispatched lambda. Without it a
    * queue destroyed while a task was mid-flight would leave that task writing to freed state
    * on its way out - which is exactly what the first version did, and what the full suite
    * caught as an access violation. It matters for any queue that is not the process-wide
    * singleton, which includes every test's.
    *
    * Never runs a task inside the lock: a task that posts its own continuation (history warming
    * advances frame by frame that way) would otherwise deadlock on a non-recursive mutex.
    */
   static void Pump(const std::shared_ptr<Impl>& self)
   {
      while (true)
      {
         std::optional<Entry> entry;
         {
            std::lock_guard lock {self->mutex_};
            entry = self->TakeDispatchableLocked();
         }
         if (!entry.has_value())
         {
            return;
         }

         const std::string serialKey  = entry->serialKey;
         const bool        background = entry->background;
         scwx::util::async(
            [self, serialKey, background, task = std::move(entry->task)]()
            {
               try
               {
                  task();
               }
               catch (const std::exception& ex)
               {
                  logger_->error("Task threw: {}", ex.what());
               }
               catch (...)
               {
                  logger_->error("Task threw an unknown exception");
               }

               // Finish before pumping, so the key this task held is released and a task
               // serialized behind it becomes dispatchable on this very pump.
               self->Finish(serialKey, background);
               Pump(self);
            });
      }
   }

   /**
    * Stops dispatching and waits for what is already running.
    *
    * A queue's owner is entitled to assume that once it is gone, none of its tasks can still be
    * touching anything the owner had lent them - test latches and gates captured by reference,
    * most obviously. Blocking here is bounded because the tasks themselves are finite; the
    * queue simply refuses to start any more.
    */
   void Drain()
   {
      std::unique_lock lock {mutex_};
      shuttingDown_ = true;
      foreground_.clear();
      background_.clear();

      // Bounded, never indefinite. A task is only counted as running once it has been handed to
      // the io_context, and io_context::stop() discards handlers it has not started yet - so a
      // queue destroyed after the context has been stopped would otherwise wait on a completion
      // that can never arrive. The shipping application never reaches here (main() ends in
      // std::_Exit, so no static destructor runs), but a test binary returns from main normally
      // and an unbounded wait there is a hang, not a failure.
      constexpr auto kDrainTimeout = std::chrono::seconds {5};
      if (!idle_.wait_for(lock, kDrainTimeout, [this]() { return running_ == 0; }))
      {
         logger_->warn("Drain timed out with {} task(s) still counted as running", running_);
      }
   }
};

RadarTaskQueue::RadarTaskQueue() : p {std::make_shared<Impl>()} {}

RadarTaskQueue::~RadarTaskQueue()
{
   p->Drain();
}

RadarTaskQueue& RadarTaskQueue::Instance()
{
   static RadarTaskQueue instance;
   return instance;
}

void RadarTaskQueue::Post(Priority              priority,
                          std::string           serialKey,
                          std::function<void()> task)
{
   if (task == nullptr)
   {
      return;
   }

   {
      std::lock_guard lock {p->mutex_};
      auto&           queue =
         (priority == Priority::Foreground) ? p->foreground_ : p->background_;
      queue.push_back(Impl::Entry {std::move(serialKey), std::move(task), false});
   }

   Impl::Pump(p);
}

std::size_t RadarTaskQueue::DropQueued(const std::string& serialKeyPrefix)
{
   std::lock_guard lock {p->mutex_};

   std::size_t dropped = 0;
   for (auto* queue : {&p->foreground_, &p->background_})
   {
      const auto removed = std::erase_if(
         *queue,
         [&serialKeyPrefix](const Impl::Entry& entry)
         { return entry.serialKey.starts_with(serialKeyPrefix); });
      dropped += removed;
   }

   return dropped;
}

void RadarTaskQueue::SetMaxConcurrency(std::size_t maximum)
{
   {
      std::lock_guard lock {p->mutex_};
      p->maxConcurrency_ = std::max<std::size_t>(maximum, 1);
   }

   // Raising the bound can make already-queued work dispatchable immediately; lowering it only
   // takes effect as running tasks finish, which is what the bound means.
   Impl::Pump(p);
}

std::size_t RadarTaskQueue::max_concurrency() const
{
   std::lock_guard lock {p->mutex_};
   return p->maxConcurrency_;
}

std::size_t RadarTaskQueue::queued_count() const
{
   std::lock_guard lock {p->mutex_};
   return p->foreground_.size() + p->background_.size();
}

std::size_t RadarTaskQueue::running_count() const
{
   std::lock_guard lock {p->mutex_};
   return p->running_;
}

} // namespace data
} // namespace wxlens
