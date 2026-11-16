#include <game/client/lucent/hud/watchers.h>
#include <gtest/gtest.h>

using namespace LucentHud::Watchers;

TEST(LucentWatchers, OtherLocalTeeIsEligibleInEitherDirection)
{
	EXPECT_TRUE(Eligible(7, 3, true, true)); // dummy watching main
	EXPECT_TRUE(Eligible(3, 7, true, true)); // main watching dummy
	EXPECT_FALSE(Eligible(3, 3, true, true));
	EXPECT_FALSE(Eligible(7, 3, false, true));
	EXPECT_FALSE(Eligible(7, 3, true, false));
}

TEST(LucentWatchers, InferenceRequiresMatchingCountAndNoExactNames)
{
	EXPECT_TRUE(InferNames(false, 0, 1, 1));
	EXPECT_FALSE(InferNames(false, 0, 1, 2));
	EXPECT_FALSE(InferNames(false, 0, 0, 0));
	EXPECT_FALSE(InferNames(false, 1, 2, 2));
	EXPECT_FALSE(InferNames(true, 0, 1, 1));
}
