#include <wxlens/data/radar_task_queue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wxlens::data::test
{
namespace
{

using Priority = RadarTaskQueue::Priority;

/// Blocks a task until the test lets it through, so a test can hold slots occupied and observe
/// what the queue does with everything else while they are.
class Gate
{
public:
   void Wait()
   {
      std::unique_lock lock {mutex_};
      condition_.wait(lock, [this]() { return open_; });
   }

   void Open()
   {
      {
         std::lock_guard lock {mutex_};
         open_ = true;
      }
      condition_.notify_all();
   }

private:
   std::mutex              mutex_;
   std::condition_variable condition_;
   bool                    open_ {false};
};

/// Counts down as tasks complete. Returns false on timeout rather than hanging the suite, so a
/// scheduling regression fails the test instead of stalling CI.
class Latch
{
public:
   explicit Latch(int count) : remaining_ {count} {}

   void CountDown()
   {
      {
         std::lock_guard lock {mutex_};
         --remaining_;
      }
      condition_.notify_all();
   }

   [[nodiscard]] bool Wait(std::chrono::milliseconds timeout = std::chrono::seconds {5})
   {
      std::unique_lock lock {mutex_};
      return condition_.wait_for(lock, timeout, [this]() { return remaining_ <= 0; });
   }

private:
   std::mutex              mutex_;
   std::condition_variable condition_;
   int                     remaining_;
};

/// Spins until `predicate` holds, so a test never asserts on a state the queue has not reached
/// yet. Returns false on timeout.
bool WaitFor(const std::function<bool()>&    predicate,
             std::chrono::milliseconds timeout = std::chrono::seconds {5})
{
   const auto deadline = std::chrono::steady_clock::now() + timeout;
   while (std::chrono::steady_clock::now() < deadline)
   {
      if (predicate())
      {
         return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds {1});
   }
   return predicate();
}

} // namespace

TEST(RadarTaskQueue, RunsAPostedTask)
{
   RadarTaskQueue   queue;
   std::atomic_bool ran {false};
   Latch            done {1};

   queue.Post(Priority::Foreground, "", [&]() { ran = true; done.CountDown(); });

   ASSERT_TRUE(done.Wait());
   EXPECT_TRUE(ran.load());
}

/// The reason the queue exists: speculative history warming must never delay a site switch.
TEST(RadarTaskQueue, ForegroundRunsBeforeAlreadyQueuedBackground)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(1);

   Gate  hold;
   Latch occupied {1};
   Latch finished {3};

   std::mutex               orderMutex;
   std::vector<std::string> order;
   const auto               record = [&](std::string label)
   {
      std::lock_guard lock {orderMutex};
      order.push_back(std::move(label));
   };

   // Occupy the single slot so everything below is genuinely queued rather than racing.
   queue.Post(Priority::Foreground,
              "",
              [&]()
              {
                 occupied.CountDown();
                 hold.Wait();
                 record("blocker");
                 finished.CountDown();
              });
   ASSERT_TRUE(occupied.Wait());

   queue.Post(Priority::Background, "", [&]() { record("background"); finished.CountDown(); });
   // Posted *after* the background task, and must still run before it.
   queue.Post(Priority::Foreground, "", [&]() { record("foreground"); finished.CountDown(); });

   hold.Open();
   ASSERT_TRUE(finished.Wait());

   std::lock_guard lock {orderMutex};
   ASSERT_EQ(order.size(), 3U);
   EXPECT_EQ(order[0], "blocker");
   EXPECT_EQ(order[1], "foreground");
   EXPECT_EQ(order[2], "background");
}

/// Priority ordering alone is not enough - if background work can fill every slot, a foreground
/// task still waits for a ~6 s download to finish before it can start.
TEST(RadarTaskQueue, BackgroundNeverOccupiesTheLastSlot)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(3);

   Gate  hold;
   Latch backgroundRunning {2};
   Latch finished {3};

   for (int i = 0; i < 5; ++i)
   {
      queue.Post(Priority::Background,
                 "",
                 [&]()
                 {
                    backgroundRunning.CountDown();
                    hold.Wait();
                    finished.CountDown();
                 });
   }

   ASSERT_TRUE(backgroundRunning.Wait());
   // Two background tasks hold slots; the bound is 3, so exactly one slot stays reserved.
   EXPECT_TRUE(WaitFor([&]() { return queue.running_count() == 2U; }));

   std::atomic_bool foregroundRan {false};
   Latch            foregroundDone {1};
   queue.Post(Priority::Foreground,
              "",
              [&]()
              {
                 foregroundRan = true;
                 foregroundDone.CountDown();
              });

   // Runs immediately, without waiting for any background task to release its slot.
   EXPECT_TRUE(foregroundDone.Wait());
   EXPECT_TRUE(foregroundRan.load());

   hold.Open();
   EXPECT_TRUE(finished.Wait());
}

/// Two listing calls on one provider race inside wxdata's UpdateMetadata. The serial key is what
/// keeps that unreachable while still allowing different sites to list concurrently.
TEST(RadarTaskQueue, SameSerialKeyNeverRunsConcurrently)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(4);

   std::atomic_int concurrent {0};
   std::atomic_int peak {0};
   Latch           done {8};

   for (int i = 0; i < 8; ++i)
   {
      queue.Post(Priority::Foreground,
                 "KEAX/list",
                 [&]()
                 {
                    const int now = ++concurrent;
                    int       observed = peak.load();
                    while (now > observed && !peak.compare_exchange_weak(observed, now))
                    {
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds {2});
                    --concurrent;
                    done.CountDown();
                 });
   }

   ASSERT_TRUE(done.Wait());
   EXPECT_EQ(peak.load(), 1);
}

TEST(RadarTaskQueue, DifferentSerialKeysRunConcurrently)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(4);

   Gate  hold;
   Latch bothRunning {2};
   Latch done {2};

   for (const char* site : {"KEAX/list", "KDDC/list"})
   {
      queue.Post(Priority::Foreground,
                 site,
                 [&]()
                 {
                    bothRunning.CountDown();
                    hold.Wait();
                    done.CountDown();
                 });
   }

   // The whole point of raising concurrency: one site's listing must not block another's.
   EXPECT_TRUE(bothRunning.Wait());
   hold.Open();
   EXPECT_TRUE(done.Wait());
}

/// How a deactivated radar site stops costing anything. Its running task unwinds on its own
/// cancellation flag; everything still queued must never start at all.
TEST(RadarTaskQueue, DropQueuedRemovesNotYetStartedTasksByPrefix)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(1);

   Gate  hold;
   Latch occupied {1};

   queue.Post(Priority::Foreground,
              "KTWX/list",
              [&]()
              {
                 occupied.CountDown();
                 hold.Wait();
              });
   ASSERT_TRUE(occupied.Wait());

   std::atomic_int abandonedRuns {0};
   std::atomic_int keptRuns {0};

   for (int i = 0; i < 3; ++i)
   {
      queue.Post(Priority::Background, "KEAX/warm", [&]() { ++abandonedRuns; });
   }
   queue.Post(Priority::Background, "KEAX/list", [&]() { ++abandonedRuns; });
   queue.Post(Priority::Background, "KDDC/warm", [&]() { ++keptRuns; });

   EXPECT_EQ(queue.DropQueued("KEAX/"), 4U);

   hold.Open();
   EXPECT_TRUE(WaitFor([&]() { return keptRuns.load() == 1; }));
   EXPECT_EQ(abandonedRuns.load(), 0);
}

TEST(RadarTaskQueue, DropQueuedLeavesARunningTaskAlone)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(1);

   Gate             hold;
   Latch            occupied {1};
   Latch            finished {1};
   std::atomic_bool completed {false};

   queue.Post(Priority::Background,
              "KEAX/warm",
              [&]()
              {
                 occupied.CountDown();
                 hold.Wait();
                 completed = true;
                 finished.CountDown();
              });
   ASSERT_TRUE(occupied.Wait());

   // Already started, so there is nothing queued to drop - cancelling it is the service's job,
   // through its own flag, not the queue's.
   EXPECT_EQ(queue.DropQueued("KEAX/"), 0U);

   hold.Open();
   ASSERT_TRUE(finished.Wait());
   EXPECT_TRUE(completed.load());
}

TEST(RadarTaskQueue, ConcurrencyBoundIsAtLeastOne)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(0);
   EXPECT_EQ(queue.max_concurrency(), 1U);

   Latch done {1};
   queue.Post(Priority::Foreground, "", [&]() { done.CountDown(); });
   EXPECT_TRUE(done.Wait());
}

/// A task posting its own continuation is how history warming advances frame by frame. Doing
/// that from inside the queue's own dispatch must not deadlock.
TEST(RadarTaskQueue, TaskMayPostItsOwnContinuation)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(2);

   Latch           done {1};
   std::atomic_int steps {0};

   auto step = std::make_shared<std::function<void()>>();
   *step     = [&queue, &done, &steps, step]()
   {
      if (++steps >= 5)
      {
         done.CountDown();
         return;
      }
      queue.Post(Priority::Background, "KEAX/warm", *step);
   };

   queue.Post(Priority::Background, "KEAX/warm", *step);

   ASSERT_TRUE(done.Wait());
   EXPECT_EQ(steps.load(), 5);
}

TEST(RadarTaskQueue, ExceptionFromATaskDoesNotStopTheQueue)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(1);

   Latch done {1};
   queue.Post(Priority::Foreground, "KEAX/list", []() { throw std::runtime_error {"boom"}; });
   queue.Post(Priority::Foreground, "KEAX/list", [&]() { done.CountDown(); });

   // The second task shares the first's serial key, so it also proves the key was released.
   EXPECT_TRUE(done.Wait());
}

} // namespace wxlens::data::test
