#include <base/str.h>
#include <game/client/lucent/voting/catalog.h>

#include <gtest/gtest.h>

#include <array>

TEST(LucentVoting, SeparatesServerTypeVotesCommandsAndMapFilters)
{
	using namespace LucentVoting;
	EXPECT_STREQ(ParseMapType("☒ Nᴏᴠɪᴄᴇ Mᴀᴘs").m_pName, "Novice");
	EXPECT_TRUE(ParseMapType("☒ Nᴏᴠɪᴄᴇ Mᴀᴘs").m_Active);
	EXPECT_STREQ(ParseMapType("☐ DDᴍᴀX.Eᴀsʏ Mᴀᴘs").m_pName, "DDmaX.Easy");
	EXPECT_FALSE(ParseMapType("☐ Brutal Maps").m_Active);
	EXPECT_EQ(ParseMapType("☐ Set an arbitrary option").m_pName, nullptr);

	const char *apDescriptions[] = {
		"☒ Nᴏᴠɪᴄᴇ Mᴀᴘs", "☐ Mᴏᴅᴇʀᴀᴛᴇ Mᴀᴘs", "☐ Brutal Maps", "Random map (Reason=Stars)",
		"Make sure no one is racing before voting!", "---- Easy ----", "Easy map | 1/5 ★", "---- New ----", "New map | 2/5 ★",
	};
	std::array<CVoteOptionClient, std::size(apDescriptions)> aOptions{};
	for(size_t i = 0; i < aOptions.size(); ++i)
	{
		str_copy(aOptions[i].m_aDescription, apDescriptions[i]);
		aOptions[i].m_pNext = i + 1 < aOptions.size() ? &aOptions[i + 1] : nullptr;
	}
	CCatalog Catalog;
	Catalog.Rebuild(aOptions.data());
	EXPECT_EQ(Catalog.m_vMapTypes, (std::vector<int>{0, 1, 2}));
	EXPECT_EQ(Catalog.m_vOptions[1].m_pOption, &aOptions[1]);
	EXPECT_TRUE(Catalog.m_vOptions[3].m_Command);
	EXPECT_EQ(Catalog.Filter("", {"Easy", "New"}, false), (std::vector<int>{3, 6, 8}));
	EXPECT_EQ(Catalog.Filter("", {"New"}, false), (std::vector<int>{3, 8}));
	EXPECT_EQ(Catalog.Filter("new", {"New"}, false), (std::vector<int>{8}));
	EXPECT_EQ(Catalog.Filter("", {"New"}, true), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8}));
	EXPECT_TRUE(Catalog.Filter("make sure", {}, false).empty());
	EXPECT_EQ(Catalog.Filter("make sure", {}, true), (std::vector<int>{4}));

	Catalog.Rebuild(&aOptions[1]); // A checkbox alone is not a DDNet type menu.
	EXPECT_TRUE(Catalog.m_vMapTypes.empty());
	EXPECT_TRUE(Catalog.Matches(0, "Mᴀᴘs", false));
}

TEST(LucentVoting, ContentHeightIncludesSectionHeadersAndBottomGutter)
{
	using namespace LucentVoting;
	EXPECT_FLOAT_EQ(ListHeight(31, 0, 0), 0);
	EXPECT_FLOAT_EQ(ListHeight(31, 12, 3), 31 * 12 + 21 * 3 + 8);
	EXPECT_FLOAT_EQ(ListHeight(23, 1, 0), 23 + 8);
}

TEST(LucentVoting, SidebarAdaptsToAvailableUiWidthAndHeight)
{
	using LucentVoting::SidebarLayout;
	EXPECT_FLOAT_EQ(SidebarLayout(1100, 490, true).m_Width, 164);
	EXPECT_FLOAT_EQ(SidebarLayout(460, 410, true).m_Width, 115);
	EXPECT_FLOAT_EQ(SidebarLayout(459, 410, true).m_Width, 0);
	EXPECT_FLOAT_EQ(SidebarLayout(410, 500, true).m_Height, 148);
	EXPECT_FLOAT_EQ(SidebarLayout(330, 300, true).m_Height, 96);
	EXPECT_FLOAT_EQ(SidebarLayout(330, 300, false).m_Height, 0);
}

TEST(LucentVoting, SplitsOnlyRecognizedMapMetadata)
{
	using namespace LucentVoting;
	const auto Map = ParseMapLabel("Sono by Nixus & Ilija_ | 5/5 ★");
	EXPECT_EQ(Map.m_Title, "Sono");
	EXPECT_EQ(Map.m_Author, "Nixus & Ilija_");
	EXPECT_EQ(Map.m_Rating, "5/5 ★");
	const auto NoAuthor = ParseMapLabel("Chill Wood | 4/5 ★");
	EXPECT_EQ(NoAuthor.m_Title, "Chill Wood");
	EXPECT_TRUE(NoAuthor.m_Author.empty());
	const auto Ambiguous = ParseMapLabel("Made by hand by Tee | 0/5 ★");
	EXPECT_EQ(Ambiguous.m_Title, "Made by hand by Tee");
	EXPECT_TRUE(Ambiguous.m_Author.empty());
	EXPECT_TRUE(ParseMapLabel("Restart by vote | 7/5 ★").m_Rating.empty());
	EXPECT_TRUE(ParseMapLabel("Random map (Reason=Stars)").m_Title.empty());

	const auto Details = ParseMapDetails("1200 ⚑ | 07:21.93 ◷ ");
	EXPECT_EQ(Details.m_Finishes, "1200 ⚑");
	EXPECT_EQ(Details.m_Time, "07:21.93 ◷");
	EXPECT_EQ(ParseMapDetails("0 ⚑").m_Finishes, "0 ⚑");
	EXPECT_TRUE(ParseMapDetails("0 ⚑").m_Time.empty());
	EXPECT_TRUE(ParseMapDetails("0 ⚑ | custom ◷").m_Finishes.empty());
}

TEST(LucentVoting, RecognizesServerHeadingsWithoutANameWhitelist)
{
	using LucentVoting::SectionName;
	EXPECT_EQ(SectionName("---- NEW MAPS ----"), "NEW MAPS");
	EXPECT_EQ(SectionName("──── Новые карты ────"), "Новые карты");
	EXPECT_EQ(SectionName("--- arbitrary server category ---"), "arbitrary server category");
	EXPECT_EQ(SectionName("─── OTHER MAPS ───"), "OTHER MAPS");
	EXPECT_TRUE(SectionName("---- Change map").empty());
	EXPECT_TRUE(SectionName("-----").empty());
}

TEST(LucentVoting, CompactRowsPreserveServerIndicesAndExposeOriginalOptions)
{
	const char *apDescriptions[] = {
		"Random map (Reason=Stars)",
		"---- NEW MAPS ----",
		"Sono by Nixus & Ilija_ | 5/5 ★",
		"1200 ⚑ | 07:21.93 ◷ ",
		"Change gravity",
		"─── Другие карты ───",
		"Chill Wood",
		"Custom server option ⚑",
	};
	std::array<CVoteOptionClient, std::size(apDescriptions)> aOptions{};
	for(size_t i = 0; i < aOptions.size(); ++i)
	{
		str_copy(aOptions[i].m_aDescription, apDescriptions[i]);
		aOptions[i].m_pNext = i + 1 < aOptions.size() ? &aOptions[i + 1] : nullptr;
	}

	LucentVoting::CCatalog Catalog;
	Catalog.Rebuild(aOptions.data());
	ASSERT_EQ(Catalog.m_vCategories.size(), 3u);
	ASSERT_EQ(Catalog.m_vOptions.size(), aOptions.size());
	EXPECT_EQ(Catalog.m_vCategories[0].m_Count, 1);
	EXPECT_EQ(Catalog.m_vCategories[1].m_Count, 2);
	EXPECT_EQ(Catalog.m_vCategories[2].m_Count, 2);
	EXPECT_EQ(Catalog.m_vOptions[2].m_pOption, &aOptions[2]);
	EXPECT_STREQ(Catalog.m_vOptions[2].m_pDetails, apDescriptions[3]);
	EXPECT_TRUE(Catalog.Matches(2, "SONO", false));
	EXPECT_TRUE(Catalog.Matches(2, "07:21.93", false));
	EXPECT_FALSE(Catalog.Matches(2, "07:21.93", true));
	EXPECT_FALSE(Catalog.Matches(3, "", false));
	EXPECT_TRUE(Catalog.Matches(3, "07:21.93", true));
	EXPECT_FALSE(Catalog.Matches(1, "", false));
	EXPECT_TRUE(Catalog.Matches(1, "NEW MAPS", true));
	EXPECT_TRUE(Catalog.Matches(4, "gravity", false));
	EXPECT_TRUE(Catalog.Matches(7, "option", false));
	EXPECT_FALSE(Catalog.Matches(7, "missing", false));
	EXPECT_EQ(Catalog.Filter("", {"NEW MAPS", "Другие карты"}, false), (std::vector<int>{2, 4, 6, 7}));
	EXPECT_EQ(Catalog.Filter("chill", {"NEW MAPS", "Другие карты"}, false), (std::vector<int>{6}));
	EXPECT_TRUE(Catalog.Filter("chill", {"NEW MAPS"}, false).empty());
	EXPECT_EQ(Catalog.Filter("", {"NEW MAPS"}, true), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
	EXPECT_EQ(Catalog.VisibleSelection(Catalog.Filter("Sono", {}, false), &aOptions[2], apDescriptions[2]), 0);
	EXPECT_EQ(Catalog.VisibleSelection(Catalog.Filter("Chill", {}, false), &aOptions[2], apDescriptions[2]), -1);

	// Removing earlier options shifts the server index while retaining the same vote.
	Catalog.Rebuild(&aOptions[2]);
	const auto vShifted = Catalog.Filter("", {}, false);
	const int Shifted = Catalog.VisibleSelection(vShifted, &aOptions[2], apDescriptions[2]);
	ASSERT_GE(Shifted, 0);
	EXPECT_EQ(vShifted[Shifted], 0);
	str_copy(aOptions[2].m_aDescription, "Reused option slot");
	Catalog.Rebuild(&aOptions[2]);
	EXPECT_EQ(Catalog.VisibleSelection(Catalog.Filter("", {}, false), &aOptions[2], apDescriptions[2]), -1);

	Catalog.Rebuild(&aOptions[6]);
	ASSERT_EQ(Catalog.m_vCategories.size(), 1u);
	ASSERT_EQ(Catalog.m_vOptions.size(), 2u);
	EXPECT_EQ(Catalog.m_vOptions[0].m_pOption, &aOptions[6]);
	EXPECT_FALSE(Catalog.m_HasDecorations);
	Catalog.Rebuild(nullptr);
	EXPECT_TRUE(Catalog.m_vOptions.empty());
}
