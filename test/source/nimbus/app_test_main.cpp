// Test driver for Nimbus's own C++ model classes (docs/ROADMAP.md: "test the C++ models
// independently of QML"). Unlike wxdata_test_main.cpp this does need a QCoreApplication, because
// the classes under test are QObjects using signals, properties and QVariant.

#include <scwx/util/logger.hpp>

#include <QCoreApplication>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::warn);

   QCoreApplication app(argc, argv);

   ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
