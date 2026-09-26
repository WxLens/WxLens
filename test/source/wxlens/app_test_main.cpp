// Test driver for WxLens's own C++ model classes (docs/ROADMAP.md: "test the C++ models
// independently of QML"). Unlike wxdata_test_main.cpp this does need a QCoreApplication, because
// the classes under test are QObjects using signals, properties and QVariant.

#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <aws/core/Aws.h>

#include <QCoreApplication>
#include <cstddef>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::warn);

   QCoreApplication app(argc, argv);

   // RadarSiteDataService constructs an AWS S3 client in its constructor, so the SDK has to be
   // initialized before any lifecycle test builds one - exactly as main() does, and as
   // wxdata_test_main.cpp already does for the provider suites. No test here makes a request.
   Aws::SDKOptions awsSdkOptions {};
   Aws::InitAPI(awsSdkOptions);

   // RadarTaskQueue dispatches through scwx::util::async, which only runs if something is
   // running the io_context. Matching the application's runner count keeps the queue's
   // concurrency behaviour under test the same as the one that ships.
   boost::asio::io_context& ioContext     = scwx::util::io_context();
   auto                     ioContextWork = boost::asio::make_work_guard(ioContext);
   constexpr std::size_t    kIoRunners    = 4;
   boost::asio::thread_pool ioThreadPool {kIoRunners};
   for (std::size_t runner = 0; runner < kIoRunners; ++runner)
   {
      boost::asio::post(ioThreadPool, [&ioContext]() { ioContext.run(); });
   }

   ::testing::InitGoogleTest(&argc, argv);
   const int result = RUN_ALL_TESTS();

   // Reset the work guard and let run() return once the queue is genuinely empty, rather than
   // calling stop(), which discards handlers that have been dispatched but not yet started and
   // would leave RadarTaskQueue waiting on completions that can never arrive.
   ioContextWork.reset();
   ioThreadPool.join();
   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
