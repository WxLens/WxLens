#include <wxlens/data/radar_site_data_service.hpp>
#include <wxlens/data/radar_task_queue.hpp>
#include <wxlens/products/radar_sweep_product.hpp>

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <chrono>
#include <memory>
#include <thread>

/**
 * Lifecycle cover for the defect measured on 2026-09-26: every radar site ever visited stayed
 * alive forever, kept polling, kept its decoded history allocated, and re-downloaded that
 * history every 30 minutes - five abandoned sites holding ~1.56 GB and 624 s of download time
 * in a 90-minute session, all competing with a newly selected site for the same worker.
 *
 * These tests deliberately make no network request. Constructing a service builds an AWS S3
 * client but issues nothing; the load paths are not exercised here, because what needs
 * asserting is ownership and teardown, not transfer behaviour.
 */
namespace wxlens::data::test
{
namespace
{

/// Nothing here spins a Qt event loop, so timers never fire during a test; draining is only to
/// let queued GUI-thread publications land before a test inspects state.
void DrainGuiEvents()
{
   if (QCoreApplication::instance() != nullptr)
   {
      QCoreApplication::processEvents();
   }
}

constexpr const char* kSiteA = "KEAX";
constexpr const char* kSiteB = "KDDC";

} // namespace

TEST(RadarSiteDataService, SharesOneInstancePerSite)
{
   auto first  = RadarSiteDataService::Instance(kSiteA);
   auto second = RadarSiteDataService::Instance(kSiteA);

   ASSERT_NE(first, nullptr);
   EXPECT_EQ(first.get(), second.get());
   EXPECT_EQ(first->radar_site(), kSiteA);
}

TEST(RadarSiteDataService, DistinctSitesGetDistinctInstances)
{
   auto a = RadarSiteDataService::Instance(kSiteA);
   auto b = RadarSiteDataService::Instance(kSiteB);

   ASSERT_NE(a, nullptr);
   ASSERT_NE(b, nullptr);
   EXPECT_NE(a.get(), b.get());
}

/// The defect itself. Before the weak registry this failed: the service outlived every
/// consumer, forever.
TEST(RadarSiteDataService, ReleasingTheLastConsumerDestroysTheService)
{
   std::weak_ptr<RadarSiteDataService> observer;
   {
      auto service = RadarSiteDataService::Instance(kSiteA);
      observer     = service;
      EXPECT_FALSE(observer.expired());
      EXPECT_TRUE(service->active());
   }

   DrainGuiEvents();
   EXPECT_TRUE(observer.expired())
      << "an abandoned site must not stay alive polling and holding decoded history";
}

/// Codex's multi-pane case: one pane leaving a site must not tear it down under another.
TEST(RadarSiteDataService, SurvivesWhileAnyConsumerRemains)
{
   auto paneOne = RadarSiteDataService::Instance(kSiteA);
   auto paneTwo = RadarSiteDataService::Instance(kSiteA);
   std::weak_ptr<RadarSiteDataService> observer = paneOne;

   // Pane one switches away.
   paneOne.reset();
   DrainGuiEvents();

   ASSERT_FALSE(observer.expired())
      << "a departing pane must release only its own reference";
   EXPECT_TRUE(paneTwo->active());
   EXPECT_EQ(paneTwo->radar_site(), kSiteA);

   paneTwo.reset();
   DrainGuiEvents();
   EXPECT_TRUE(observer.expired());
}

/// Rapid A -> B -> A must not hand back a service whose providers were already shut down for
/// good, and must not leak the intermediate one.
TEST(RadarSiteDataService, RevisitingASiteBuildsAFreshService)
{
   RadarSiteDataService* firstAddress = nullptr;
   {
      auto a       = RadarSiteDataService::Instance(kSiteA);
      firstAddress = a.get();
      EXPECT_TRUE(a->active());
   }
   DrainGuiEvents();

   {
      auto b = RadarSiteDataService::Instance(kSiteB);
      EXPECT_TRUE(b->active());
   }
   DrainGuiEvents();

   auto again = RadarSiteDataService::Instance(kSiteA);
   ASSERT_NE(again, nullptr);
   EXPECT_TRUE(again->active())
      << "a revisited site must get working providers, not a shut-down carcass";
   EXPECT_EQ(again->radar_site(), kSiteA);
   // Address reuse by the allocator is legal, so identity is asserted through `active()` above
   // rather than by comparing pointers; this only records that a new object was constructed.
   (void) firstAddress;
}

TEST(RadarSiteDataService, ExpiredRegistryEntriesAreCleanedUp)
{
   const auto before = RadarSiteDataService::InstanceCountForTesting();

   {
      auto a = RadarSiteDataService::Instance(kSiteA);
      auto b = RadarSiteDataService::Instance(kSiteB);
      EXPECT_GE(RadarSiteDataService::InstanceCountForTesting(), before + 2);
   }

   DrainGuiEvents();
   // Instance() prunes expired entries; ask for something to trigger it, then drop it again.
   { auto scratch = RadarSiteDataService::Instance("KTWX"); }
   DrainGuiEvents();

   EXPECT_LE(RadarSiteDataService::InstanceCountForTesting(), before)
      << "the registry must not accumulate an entry per site ever visited";
}

TEST(RadarSiteDataService, DeactivationIsTerminalForTheReleasedInstance)
{
   auto service = RadarSiteDataService::Instance(kSiteA);
   ASSERT_TRUE(service->active());

   std::weak_ptr<RadarSiteDataService> observer = service;
   service.reset();
   DrainGuiEvents();

   ASSERT_TRUE(observer.expired());
}

/// A released site must leave nothing of its own behind in the shared queue. The count is
/// asserted through the queue rather than the service so this stays true regardless of how many
/// tasks the warm chain happens to have posted.
TEST(RadarSiteDataService, ReleaseDropsThatSitesQueuedWorkOnly)
{
   RadarTaskQueue queue;
   queue.SetMaxConcurrency(1);

   std::atomic_bool gate {false};
   std::atomic_int  otherSiteRuns {0};

   // Hold the only slot so the entries below stay queued.
   queue.Post(RadarTaskQueue::Priority::Foreground,
              "BLOCKER",
              [&gate]()
              {
                 while (!gate.load())
                 {
                    std::this_thread::sleep_for(std::chrono::milliseconds {1});
                 }
              });

   for (int i = 0; i < 3; ++i)
   {
      queue.Post(RadarTaskQueue::Priority::Background, std::string {kSiteA} + "/warm", []() {});
   }
   queue.Post(RadarTaskQueue::Priority::Background,
              std::string {kSiteB} + "/warm",
              [&otherSiteRuns]() { ++otherSiteRuns; });

   EXPECT_EQ(queue.DropQueued(std::string {kSiteA} + "/"), 3U);

   gate = true;
   const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};
   while (otherSiteRuns.load() == 0 && std::chrono::steady_clock::now() < deadline)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds {1});
   }
   EXPECT_EQ(otherSiteRuns.load(), 1)
      << "dropping one site's queued work must leave every other site's alone";
}

TEST(RadarSiteDataService, HistoryMinutesIsClampedToTheSupportedRange)
{
   const int original = RadarSiteDataService::HistoryMinutes();

   RadarSiteDataService::SetHistoryMinutes(1);
   EXPECT_EQ(RadarSiteDataService::HistoryMinutes(), 5);

   RadarSiteDataService::SetHistoryMinutes(9999);
   EXPECT_EQ(RadarSiteDataService::HistoryMinutes(), 120);

   RadarSiteDataService::SetHistoryMinutes(original);
}

/**
 * The launch window. RadarSweepProduct connects to its service's signals, and a Qt connection
 * does not own its sender - so without an owning member the service would be constructed, asked
 * for a volume and destroyed before the constructor returned. PaneController's own reference is
 * not acquired until 23 lines later, which is why this cannot be left to the pane.
 */
TEST(RadarSweepProductLifetime, ProductKeepsItsDataServiceAlive)
{
   std::weak_ptr<RadarSiteDataService> observer;

   auto product = products::RadarSweepProduct::Instance(kSiteA, "Reflectivity", 0.0f);
   ASSERT_NE(product, nullptr);

   {
      // Any consumer asking for the same site must get the instance the product is holding,
      // which proves the product is holding one at all.
      auto service = RadarSiteDataService::Instance(kSiteA);
      observer     = service;
   }
   DrainGuiEvents();

   EXPECT_FALSE(observer.expired())
      << "the sweep product must own its data service, not borrow it from the pane";

   product.reset();
   DrainGuiEvents();
   EXPECT_TRUE(observer.expired())
      << "and must release it when the product itself goes away";
}

} // namespace wxlens::data::test
