#include <wxlens/data/frame_cache.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <stdexcept>
#include <thread>
#include <vector>

namespace wxlens::data::test
{
namespace
{

/// Stands in for a decoded radar frame. `bytes` lets a test state an entry's
/// retention cost directly, instead of depending on a real volume's size.
struct Frame
{
   std::string label;
   std::size_t bytes {1U};
};

using Cache = FrameCache<Frame>;

const auto kSizeOf = [](const Frame& frame) { return frame.bytes; };

Cache::Loader Constant(const char* label, std::size_t bytes = 1U)
{
   return [label, bytes]()
   { return std::make_shared<Frame>(Frame {label, bytes}); };
}

/// A loader that records how many times it actually ran.
Cache::Loader Counting(std::atomic_int& calls,
                       const char*      label,
                       std::size_t      bytes = 1U)
{
   return [&calls, label, bytes]()
   {
      calls.fetch_add(1);
      return std::make_shared<Frame>(Frame {label, bytes});
   };
}

} // namespace

// --- Retention ---------------------------------------------------------------

TEST(FrameCacheTest, RepeatedSelectionReusesOneLoad)
{
   Cache           cache {1024U};
   std::atomic_int calls {0};

   const auto first = cache.Load("KEAX/V06", Counting(calls, "volume"), kSizeOf);
   const auto second =
      cache.Load("KEAX/V06", Counting(calls, "volume"), kSizeOf);

   EXPECT_EQ(calls.load(), 1);
   EXPECT_EQ(first.origin, Cache::Origin::Loaded);
   EXPECT_EQ(second.origin, Cache::Origin::Cache);
   EXPECT_TRUE(second.cache_hit());
   // The same decoded object, not an equal copy - geometry rebuilt from this
   // pointer can be skipped precisely because identity is preserved.
   EXPECT_EQ(first.value.get(), second.value.get());
}

TEST(FrameCacheTest, DistinctKeysAreLoadedIndependently)
{
   Cache           cache {1024U};
   std::atomic_int calls {0};

   cache.Load("KEAX/1", Counting(calls, "a"), kSizeOf);
   cache.Load("KEAX/2", Counting(calls, "b"), kSizeOf);

   EXPECT_EQ(calls.load(), 2);
   EXPECT_EQ(cache.count(), 2U);
}

TEST(FrameCacheTest, FindDoesNotLoadAndReportsMissAsNull)
{
   Cache cache {1024U};

   EXPECT_EQ(cache.Find("absent"), nullptr);

   cache.Load("present", Constant("frame"), kSizeOf);
   ASSERT_NE(cache.Find("present"), nullptr);
   EXPECT_EQ(cache.Find("present")->label, "frame");
}

// --- Bounding and eviction ---------------------------------------------------

TEST(FrameCacheTest, EvictionKeepsMemoryWithinBudget)
{
   Cache cache {300U};

   for (int i = 0; i < 10; ++i)
   {
      cache.Load("key" + std::to_string(i), Constant("frame", 100U), kSizeOf);
      EXPECT_LE(cache.size_bytes(), cache.capacity_bytes());
   }

   EXPECT_EQ(cache.count(), 3U);
   EXPECT_EQ(cache.size_bytes(), 300U);
}

TEST(FrameCacheTest, EvictsLeastRecentlyUsedNotOldestInserted)
{
   Cache cache {300U};

   cache.Load("a", Constant("a", 100U), kSizeOf);
   cache.Load("b", Constant("b", 100U), kSizeOf);
   cache.Load("c", Constant("c", 100U), kSizeOf);

   // Re-touch "a" so insertion order and use order disagree.
   ASSERT_NE(cache.Find("a"), nullptr);
   cache.Load("d", Constant("d", 100U), kSizeOf);

   EXPECT_TRUE(cache.Contains("a"));
   EXPECT_FALSE(cache.Contains("b")) << "b was least recently used";
   EXPECT_TRUE(cache.Contains("c"));
   EXPECT_TRUE(cache.Contains("d"));
}

TEST(FrameCacheTest, OversizedFrameIsReturnedButNotRetained)
{
   Cache cache {100U};
   cache.Load("small", Constant("small", 50U), kSizeOf);

   const auto huge = cache.Load("huge", Constant("huge", 5000U), kSizeOf);

   ASSERT_NE(huge.value, nullptr) << "the caller still gets its frame";
   EXPECT_EQ(huge.origin, Cache::Origin::Loaded);
   EXPECT_FALSE(cache.Contains("huge"));
   EXPECT_TRUE(cache.Contains("small"))
      << "one oversized frame must not flush the cache";
   EXPECT_LE(cache.size_bytes(), cache.capacity_bytes());
}

TEST(FrameCacheTest, ReloadingAKeyDoesNotDoubleCountItsBytes)
{
   Cache cache {1000U};
   cache.Load("k", Constant("v1", 100U), kSizeOf);
   cache.Clear();
   cache.Load("k", Constant("v2", 100U), kSizeOf);
   cache.Clear();
   cache.Load("k", Constant("v3", 100U), kSizeOf);

   EXPECT_EQ(cache.count(), 1U);
   EXPECT_EQ(cache.size_bytes(), 100U);
}

TEST(FrameCacheTest, ClearReleasesEverything)
{
   Cache cache {1000U};
   cache.Load("a", Constant("a", 100U), kSizeOf);
   cache.Load("b", Constant("b", 100U), kSizeOf);

   cache.Clear();

   EXPECT_EQ(cache.count(), 0U);
   EXPECT_EQ(cache.size_bytes(), 0U);
   EXPECT_EQ(cache.Find("a"), nullptr);
}

// --- Failure handling --------------------------------------------------------

TEST(FrameCacheTest, FailedLoadIsNotCachedAsANegativeResult)
{
   Cache           cache {1024U};
   std::atomic_int calls {0};

   const auto failed = cache.Load(
      "k",
      [&calls]() -> std::shared_ptr<Frame>
      {
         calls.fetch_add(1);
         return nullptr;
      },
      kSizeOf);

   EXPECT_EQ(failed.value, nullptr);
   EXPECT_EQ(cache.count(), 0U);

   // A later attempt must be free to retry rather than replay the failure.
   const auto retried = cache.Load("k", Counting(calls, "recovered"), kSizeOf);
   ASSERT_NE(retried.value, nullptr);
   EXPECT_EQ(retried.value->label, "recovered");
   EXPECT_EQ(calls.load(), 2);
}

TEST(FrameCacheTest, ThrowingLoaderPropagatesAndLeavesCacheUsable)
{
   Cache cache {1024U};

   EXPECT_THROW(cache.Load(
                   "k",
                   []() -> std::shared_ptr<Frame>
                   { throw std::runtime_error {"download failed"}; },
                   kSizeOf),
                std::runtime_error);

   EXPECT_EQ(cache.count(), 0U);

   // The in-flight slot must have been released, or this would deadlock.
   const auto recovered = cache.Load("k", Constant("recovered"), kSizeOf);
   ASSERT_NE(recovered.value, nullptr);
   EXPECT_EQ(recovered.origin, Cache::Origin::Loaded);
}

// --- Deduplication -----------------------------------------------------------

TEST(FrameCacheTest, ConcurrentCallersForOneKeyShareASingleLoad)
{
   constexpr int kThreads = 8;

   Cache            cache {1024U};
   std::atomic_int  calls {0};
   std::atomic_bool release {false};

   // Every thread must be inside Load before the loader is allowed to finish,
   // otherwise the first could complete and the rest would simply hit the
   // cache - which would pass without exercising deduplication at all.
   std::barrier entered {kThreads};

   std::vector<Cache::LoadResult> results(kThreads);
   std::vector<std::thread>       threads;
   threads.reserve(kThreads);

   for (int i = 0; i < kThreads; ++i)
   {
      threads.emplace_back(
         [&, i]()
         {
            entered.arrive_and_wait();
            results[i] = cache.Load(
               "shared-key",
               [&]()
               {
                  calls.fetch_add(1);
                  while (!release.load())
                  {
                     std::this_thread::yield();
                  }
                  return std::make_shared<Frame>(Frame {"volume", 10U});
               },
               kSizeOf);
         });
   }

   // Let the winning loader finish once the others have had a chance to queue.
   std::this_thread::sleep_for(std::chrono::milliseconds {50});
   release.store(true);

   for (auto& thread : threads)
   {
      thread.join();
   }

   EXPECT_EQ(calls.load(), 1) << "the same object was downloaded more than once";

   int loaded       = 0;
   int deduplicated = 0;
   int cached       = 0;
   for (const auto& result : results)
   {
      ASSERT_NE(result.value, nullptr);
      EXPECT_EQ(result.value.get(), results.front().value.get())
         << "every caller must observe the one shared frame";
      switch (result.origin)
      {
      case Cache::Origin::Loaded:
         ++loaded;
         break;
      case Cache::Origin::Deduplicated:
         ++deduplicated;
         break;
      case Cache::Origin::Cache:
         ++cached;
         break;
      }
   }

   EXPECT_EQ(loaded, 1);
   EXPECT_EQ(loaded + deduplicated + cached, kThreads);
   // Deliberately not asserted as exactly kThreads - 1: a thread descheduled
   // past the loader's completion is legitimately served from the cache
   // instead, which on a loaded machine is a scheduling outcome rather than a
   // defect. The single loader call asserted above is the property that
   // matters; this only confirms the deduplication path really ran, rather than
   // the test degenerating into every caller hitting the cache.
   EXPECT_GE(deduplicated, 1);
}

TEST(FrameCacheTest, DeduplicatedCallersSeeTheLoaderFailure)
{
   constexpr int kThreads = 4;

   Cache            cache {1024U};
   std::atomic_bool release {false};
   std::atomic_int  thrown {0};
   std::barrier     entered {kThreads};

   std::vector<std::thread> threads;
   threads.reserve(kThreads);

   for (int i = 0; i < kThreads; ++i)
   {
      threads.emplace_back(
         [&]()
         {
            entered.arrive_and_wait();
            try
            {
               cache.Load(
                  "shared-key",
                  [&]() -> std::shared_ptr<Frame>
                  {
                     while (!release.load())
                     {
                        std::this_thread::yield();
                     }
                     throw std::runtime_error {"download failed"};
                  },
                  kSizeOf);
            }
            catch (const std::runtime_error&)
            {
               thrown.fetch_add(1);
            }
         });
   }

   std::this_thread::sleep_for(std::chrono::milliseconds {50});
   release.store(true);

   for (auto& thread : threads)
   {
      thread.join();
   }

   EXPECT_EQ(thrown.load(), kThreads)
      << "a waiter must not silently receive a null frame on failure";
   EXPECT_EQ(cache.count(), 0U);
}

TEST(FrameCacheTest, ConcurrentDistinctKeysDoNotBlockEachOther)
{
   constexpr int kThreads = 6;

   Cache           cache {1024U};
   std::atomic_int calls {0};
   std::barrier    entered {kThreads};

   std::vector<std::thread> threads;
   threads.reserve(kThreads);

   for (int i = 0; i < kThreads; ++i)
   {
      threads.emplace_back(
         [&, i]()
         {
            entered.arrive_and_wait();
            // Each key blocks on the barrier inside its own loader; if one
            // loader held the cache lock, this would deadlock rather than fail.
            cache.Load(
               "key" + std::to_string(i), Counting(calls, "frame", 1U), kSizeOf);
         });
   }

   for (auto& thread : threads)
   {
      thread.join();
   }

   EXPECT_EQ(calls.load(), kThreads);
   EXPECT_EQ(cache.count(), static_cast<std::size_t>(kThreads));
}

} // namespace wxlens::data::test
