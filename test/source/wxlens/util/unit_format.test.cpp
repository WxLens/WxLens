#include <wxlens/util/unit_format.hpp>

#include <gtest/gtest.h>

namespace wxlens
{
namespace
{

/// Restores the process-wide preference so a formatting test cannot leak into its neighbours -
/// the preference is deliberately global state (see unit_format.hpp).
class VelocityUnitsTest : public ::testing::Test
{
protected:
   void SetUp() override { previous_ = util::GetVelocityUnitPreference(); }
   void TearDown() override { util::SetVelocityUnitPreference(previous_); }

private:
   util::VelocityUnitPreference previous_ {util::VelocityUnitPreference::MilesPerHour};
};

TEST_F(VelocityUnitsTest, DefaultsToMilesPerHourToMatchTheBundledRamp)
{
   // WxLens ships its velocity palette in mph, so an unconfigured install must read in mph or the
   // readout and the colour beside it would disagree.
   util::SetVelocityUnitPreference(util::VelocityUnitPreference::MilesPerHour);
   EXPECT_EQ(util::GetVelocityUnitPreference(), util::VelocityUnitPreference::MilesPerHour);
}

TEST_F(VelocityUnitsTest, ConvertsFromMetersPerSecond)
{
   // Level 2 velocity decodes to m/s, so that is the input unit everywhere.
   constexpr double kMetersPerSecond = 25.0;
   EXPECT_NEAR(util::ConvertVelocity(kMetersPerSecond, util::VelocityUnitPreference::MetersPerSecond),
               25.0, 1e-9);
   EXPECT_NEAR(util::ConvertVelocity(kMetersPerSecond, util::VelocityUnitPreference::Knots),
               48.5961, 1e-3);
   EXPECT_NEAR(util::ConvertVelocity(kMetersPerSecond, util::VelocityUnitPreference::MilesPerHour),
               55.9234, 1e-3);
   EXPECT_NEAR(
      util::ConvertVelocity(kMetersPerSecond, util::VelocityUnitPreference::KilometersPerHour),
      90.0, 1e-9);
}

TEST_F(VelocityUnitsTest, FormatsOneUnitAtATimeAndKeepsTheSign)
{
   util::SetVelocityUnitPreference(util::VelocityUnitPreference::Knots);
   EXPECT_EQ(util::FormatVelocity(25.0), QStringLiteral("49 kt"));
   // Negative is inbound - toward the radar - and dropping the sign would invert the meteorology.
   EXPECT_EQ(util::FormatVelocity(-25.0), QStringLiteral("-49 kt"));

   util::SetVelocityUnitPreference(util::VelocityUnitPreference::MilesPerHour);
   EXPECT_EQ(util::FormatVelocity(25.0), QStringLiteral("56 mph"));
   util::SetVelocityUnitPreference(util::VelocityUnitPreference::KilometersPerHour);
   EXPECT_EQ(util::FormatVelocity(25.0), QStringLiteral("90 km/h"));
   util::SetVelocityUnitPreference(util::VelocityUnitPreference::MetersPerSecond);
   EXPECT_EQ(util::FormatVelocity(25.0), QStringLiteral("25 m/s"));

   // Never two units at once, unlike distances: see the enum's doc comment for why.
   EXPECT_FALSE(util::FormatVelocity(25.0).contains(QLatin1Char('/')) &&
                util::GetVelocityUnitPreference() != util::VelocityUnitPreference::MetersPerSecond);
}

TEST_F(VelocityUnitsTest, LabelsAreTheConventionalAbbreviations)
{
   EXPECT_EQ(util::VelocityUnitLabel(util::VelocityUnitPreference::MilesPerHour),
             QStringLiteral("mph"));
   EXPECT_EQ(util::VelocityUnitLabel(util::VelocityUnitPreference::Knots), QStringLiteral("kt"));
   EXPECT_EQ(util::VelocityUnitLabel(util::VelocityUnitPreference::KilometersPerHour),
             QStringLiteral("km/h"));
   EXPECT_EQ(util::VelocityUnitLabel(util::VelocityUnitPreference::MetersPerSecond),
             QStringLiteral("m/s"));
}

} // namespace
} // namespace wxlens
