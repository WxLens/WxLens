// Test driver for the reused wxdata GTest suite (docs/adr/0002-wxdata-reuse-strategy.md).
// Qt-free by design: unlike the legacy repo's wxtest.cpp, this does not depend on
// QCoreApplication or scwx::qt::* (wxdata itself has no Qt dependency, so its tests shouldn't
// need one either).

#include <scwx/util/logger.hpp>

#include <aws/core/Aws.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   Aws::SDKOptions awsSdkOptions;
   Aws::InitAPI(awsSdkOptions);

   ::testing::InitGoogleTest(&argc, argv);
   int result = RUN_ALL_TESTS();

   Aws::ShutdownAPI(awsSdkOptions);

   return result;
}
