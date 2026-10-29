#include <base/str.h>

#include <game/client/lucent/settings/player.h>
#include <game/client/lucent/settings/player_countries.h>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace
{
	bool Matches(const char *pQuery)
	{
		return LucentSettings::PlayerCountryMatches("RU", "Russia", "Россия", "643", pQuery,
			[](const char *pText, const char *pNeedle) { return str_utf8_find_nocase(pText, pNeedle) != nullptr; });
	}
}
TEST(LucentPlayer, UnicodeAndCaseInsensitiveCountrySearch)
{
	EXPECT_TRUE(Matches("россия"));
	EXPECT_TRUE(Matches("РОСС"));
	EXPECT_TRUE(Matches("rUsSiA"));
	EXPECT_TRUE(Matches("ru"));
	EXPECT_TRUE(Matches("643"));
	EXPECT_TRUE(Matches(""));
	EXPECT_FALSE(Matches("Japan"));
	EXPECT_FALSE(Matches("日本"));
}
TEST(LucentPlayer, ResourceExtensionsAndLongNamesRemainSearchable)
{
	const auto Contains = [](const char *pText, const char *pQuery) { return str_utf8_find_nocase(pText, pQuery) != nullptr; };
	EXPECT_TRUE(LucentSettings::PlayerCountryMatches("GB-NIR", "Northern Ireland", "Северная Ирландия", "902", "ireland", Contains));
	EXPECT_TRUE(LucentSettings::PlayerCountryMatches("GB-NIR", "Northern Ireland", "Северная Ирландия", "902", "северная", Contains));
	EXPECT_TRUE(LucentSettings::PlayerCountryMatches("custom", "custom", "custom", "999", "CUSTOM", Contains));
	EXPECT_TRUE(LucentSettings::PlayerCountryMatches("GS", "South Georgia & South Sandwich Islands", "", "239", "sandwich", Contains));
}
TEST(LucentPlayer, CountryNamesAreUniqueAndSpecialFlagsHaveNames)
{
	std::set<std::string> Codes;
	for(const auto &Entry : LucentSettings::PlayerCountryNames)
	{
		EXPECT_TRUE(Codes.insert(Entry.m_pCode).second);
		EXPECT_NE(*Entry.m_pName, '\0');
	}
	for(const auto *pCode : {"default", "EU", "GB-ENG", "GB-NIR", "GB-SCT", "GB-WLS", "ES-CT", "ES-GA"})
		EXPECT_NE(LucentSettings::PlayerCountryName(pCode), nullptr);
	EXPECT_EQ(LucentSettings::PlayerCountryName("additional-resource"), nullptr);
}
TEST(LucentPlayer, PopupSelectionNotifiesOnlyTheBoundProfile)
{
	int Country[2] = {-1, 643};
	int Notifications[2] = {};
	LucentSettings::CPlayerBindings Bindings;
	Bindings.m_Profiles[0].m_pCountry = &Country[0];
	Bindings.m_Profiles[1].m_pCountry = &Country[1];
	Bindings.m_Changed = [&](bool Dummy) { ++Notifications[int(Dummy)]; };
	EXPECT_FALSE(Bindings.SelectCountry(true, 643));
	EXPECT_EQ(Notifications[1], 0);
	EXPECT_TRUE(Bindings.SelectCountry(true, 901));
	EXPECT_EQ(Country[0], -1);
	EXPECT_EQ(Country[1], 901);
	EXPECT_EQ(Notifications[0], 0);
	EXPECT_EQ(Notifications[1], 1);
	EXPECT_TRUE(Bindings.SelectCountry(false, 840));
	EXPECT_EQ(Country[1], 901);
	EXPECT_EQ(Notifications[0], 1);
}

TEST(LucentPlayer, PreviewIdentityReadsLiveProfileBuffers)
{
	char aName[32] = "Player", aDummyName[32] = "Dummy";
	char aClan[16] = "Clan";
	int Country = -1;
	LucentSettings::CPlayerBindings B;
	B.m_Profiles[0] = {aName, sizeof(aName), aClan, sizeof(aClan), &Country, "fallback", {}};
	B.m_Profiles[1] = {aDummyName, sizeof(aDummyName), aClan, sizeof(aClan), &Country, "dummy fallback", {}};
	EXPECT_STREQ(B.m_Profiles[0].DisplayName(), "Player");
	EXPECT_STREQ(B.m_Profiles[1].DisplayName(), "Dummy");
	str_copy(aName, "Новое имя");
	str_copy(aClan, "New clan");
	EXPECT_STREQ(B.m_Profiles[0].DisplayName(), "Новое имя");
	EXPECT_STREQ(B.m_Profiles[0].m_pClan, "New clan");
	EXPECT_STREQ(B.m_Profiles[1].DisplayName(), "Dummy");
	aDummyName[0] = 0;
	EXPECT_STREQ(B.m_Profiles[1].DisplayName(), "dummy fallback");
}

TEST(LucentPlayer, DefaultFirstPreservesPeerOrderAndDoesNotDuplicate)
{
	std::vector<int> Codes = {20, 276, 643, -1};
	const auto IsDefault = [](int Code) { return Code == -1; };
	LucentSettings::PlayerDefaultFirst(Codes, IsDefault);
	EXPECT_EQ(Codes, (std::vector<int>{-1, 20, 276, 643}));
	EXPECT_EQ(std::count(Codes.begin(), Codes.end(), -1), 1);
	LucentSettings::PlayerDefaultFirst(Codes, IsDefault);
	EXPECT_EQ(Codes, (std::vector<int>{-1, 20, 276, 643}));
	std::vector<int> Filtered = {276, 643};
	LucentSettings::PlayerDefaultFirst(Filtered, IsDefault);
	EXPECT_EQ(Filtered, (std::vector<int>{276, 643}));
	std::vector<int> Empty;
	LucentSettings::PlayerDefaultFirst(Empty, IsDefault);
	EXPECT_TRUE(Empty.empty());
	std::vector<int> OnlyDefault = {-1};
	LucentSettings::PlayerDefaultFirst(OnlyDefault, IsDefault);
	EXPECT_EQ(OnlyDefault, (std::vector<int>{-1}));
}
TEST(LucentPlayer, ReopenedProfileSelectionUsesReorderedCountryIndex)
{
	const int ProfileCountry[] = {-1, 643};
	for(int Profile : {0, 1, 0, 1})
	{
		std::vector<int> Codes = {20, 276, 643, -1};
		LucentSettings::PlayerDefaultFirst(Codes, [](int Code) { return Code == -1; });
		const auto Selected = std::find(Codes.begin(), Codes.end(), ProfileCountry[Profile]);
		ASSERT_NE(Selected, Codes.end());
		const auto Index = std::distance(Codes.begin(), Selected);
		EXPECT_EQ(Index, Profile == 0 ? 0 : 3);
		EXPECT_EQ(Codes[Index], ProfileCountry[Profile]);
	}
}
