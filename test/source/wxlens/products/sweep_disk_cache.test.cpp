#include <wxlens/products/sweep_disk_cache.hpp>

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace wxlens
{
namespace products
{
namespace
{

std::filesystem::path Root(const QTemporaryDir& dir)
{
   return std::filesystem::path {dir.path().toStdString()};
}

SweepData SampleSweep(float scale = 1.0f)
{
   SweepData sweep;
   sweep.vertices        = {1.0f, 2.0f, 3.0f, 4.0f};
   sweep.dataMoments8    = {10, 20, 30};
   sweep.dataMomentOffset = 5.0f;
   sweep.dataMomentScale  = scale;
   sweep.dataMomentUnits  = "M/S";
   return sweep;
}

const std::vector<float> kSampleCuts {0.5f, 0.9f, 1.5f, 2.4f};

const auto kObservationTime =
   std::chrono::system_clock::time_point {std::chrono::hours {500000}};

} // namespace

TEST(SweepDiskCache, RoundTripsGeometryElevationAngleAndCuts)
{
   QTemporaryDir dir;
   SweepDiskCache cache {Root(dir), 1024U * 1024U};

   const auto sweep = SampleSweep();
   cache.Store("KEAX:Reflectivity:req0.500000:12345", kObservationTime, sweep, 0.5f, kSampleCuts);

   const auto found = cache.Find("KEAX:Reflectivity:req0.500000:12345");
   ASSERT_NE(found.sweep, nullptr);
   EXPECT_EQ(found.sweep->vertices, sweep.vertices);
   EXPECT_EQ(found.sweep->dataMoments8, sweep.dataMoments8);
   EXPECT_FLOAT_EQ(found.sweep->dataMomentOffset, sweep.dataMomentOffset);
   EXPECT_FLOAT_EQ(found.sweep->dataMomentScale, sweep.dataMomentScale);
   EXPECT_EQ(found.sweep->dataMomentUnits, sweep.dataMomentUnits);
   EXPECT_FLOAT_EQ(found.elevationAngleDegrees, 0.5f);
   EXPECT_EQ(found.elevationCuts, kSampleCuts)
      << "the tilt picker's available-cuts list must survive a disk-cache hit too, not just the "
         "geometry";
}

TEST(SweepDiskCache, MissReturnsNullSweepWithoutTouchingDisk)
{
   QTemporaryDir dir;
   SweepDiskCache cache {Root(dir), 1024U * 1024U};

   EXPECT_EQ(cache.Find("absent").sweep, nullptr);
   EXPECT_EQ(cache.count(), 0U);
}

// The actual property the checklist item asks for: a second cache instance pointed at the same
// directory - standing in for the next process launch - finds what the first one stored.
TEST(SweepDiskCache, SurvivesAFreshInstanceOverTheSameDirectory)
{
   QTemporaryDir dir;
   const auto     sweep = SampleSweep(2.0f);
   {
      SweepDiskCache first {Root(dir), 1024U * 1024U};
      first.Store("KEAX:Velocity:req1.500000:99", kObservationTime, sweep, 1.5f, kSampleCuts);
   }

   SweepDiskCache reopened {Root(dir), 1024U * 1024U};
   const auto      found = reopened.Find("KEAX:Velocity:req1.500000:99");
   ASSERT_NE(found.sweep, nullptr);
   EXPECT_EQ(found.sweep->vertices, sweep.vertices);
   EXPECT_FLOAT_EQ(found.elevationAngleDegrees, 1.5f);
   EXPECT_EQ(found.elevationCuts, kSampleCuts);
}

TEST(SweepDiskCache, CorruptEntryIsRejectedAndRemovedRatherThanCrashing)
{
   QTemporaryDir dir;
   SweepDiskCache cache {Root(dir), 1024U * 1024U};
   cache.Store("k", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
   ASSERT_EQ(cache.count(), 1U);

   // Corrupt the one entry file in place - stands in for a crash mid-write or bit rot, not
   // something SweepDiskCache's own API can provoke directly.
   for (const auto& entry : std::filesystem::directory_iterator {Root(dir)})
   {
      std::ofstream file(entry.path(), std::ios::binary | std::ios::trunc);
      file << "not a valid cache entry";
   }

   EXPECT_EQ(cache.Find("k").sweep, nullptr);
   EXPECT_EQ(cache.count(), 0U) << "the corrupt file must be removed, not left behind";
}

TEST(SweepDiskCache, TruncatedTempFileFromAnInterruptedStoreIsCleanedUpAtNextConstruction)
{
   QTemporaryDir dir;

   {
      SweepDiskCache cache {Root(dir), 1024U * 1024U};
      cache.Store("k", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);

      // Simulate a Store that was killed before the rename that commits it. A live Store()
      // no longer sweeps this away unconditionally - EnforceCapacityLocked only runs when the
      // (O(1)) running byte estimate says the budget might actually be exceeded, which a couple
      // of tiny entries well under a 1 MB budget never will. That trade is the whole point: the
      // first version scanned the whole directory on every single Store, which was a real,
      // measured, worsening-with-cache-size delay between computing a sweep and putting it on
      // screen. A stray temp file is inert (never read as a valid entry) in the meantime.
      cache.Store("other", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
      std::ofstream leftover(Root(dir) / "stray.wxrd.tmp", std::ios::binary);
      leftover << "partial write";
      leftover.close();
      EXPECT_TRUE(std::filesystem::exists(Root(dir) / "stray.wxrd.tmp"));
   }

   // The next construction (standing in for the next launch) always does one full scan, which is
   // where an orphaned temp file actually gets swept.
   SweepDiskCache reopened {Root(dir), 1024U * 1024U};
   (void) reopened;
   for (const auto& entry : std::filesystem::directory_iterator {Root(dir)})
   {
      EXPECT_NE(entry.path().extension(), ".tmp");
   }
}

TEST(SweepDiskCache, ClearRemovesEveryEntry)
{
   QTemporaryDir dir;
   SweepDiskCache cache {Root(dir), 1024U * 1024U};
   cache.Store("a", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
   cache.Store("b", kObservationTime, SampleSweep(), 1.0f, kSampleCuts);
   ASSERT_EQ(cache.count(), 2U);

   cache.Clear();

   EXPECT_EQ(cache.count(), 0U);
   EXPECT_EQ(cache.size_bytes(), 0U);
   EXPECT_EQ(cache.Find("a").sweep, nullptr);
}

TEST(SweepDiskCache, EvictsLeastRecentlyReadEntryOnceOverBudget)
{
   QTemporaryDir dir;
   // Same key length as "a"/"b"/"c" below, so the measured size matches theirs exactly - a
   // differently-sized probe key would make this budget too loose to ever trigger eviction.
   SweepDiskCache probe {Root(dir), 1024U * 1024U};
   probe.Store("z", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
   const auto perEntryBytes = probe.size_bytes();
   probe.Clear();

   SweepDiskCache cache {Root(dir), perEntryBytes * 2};
   cache.Store("a", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
   cache.Store("b", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);
   ASSERT_EQ(cache.count(), 2U);

   // Touch "a" so read-order and insertion-order disagree, then force eviction with a third entry.
   ASSERT_NE(cache.Find("a").sweep, nullptr);
   cache.Store("c", kObservationTime, SampleSweep(), 0.5f, kSampleCuts);

   EXPECT_LE(cache.size_bytes(), cache.capacity_bytes());
   EXPECT_NE(cache.Find("a").sweep, nullptr) << "a was read most recently";
   EXPECT_EQ(cache.Find("b").sweep, nullptr) << "b was least recently read";
   EXPECT_NE(cache.Find("c").sweep, nullptr);
}

} // namespace products
} // namespace wxlens
