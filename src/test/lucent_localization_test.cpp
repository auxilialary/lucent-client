#include "test.h"

#include <engine/shared/sound_pack.h>
#include <engine/storage.h>

#include <game/client/lucent/localization.h>
#include <game/client/lucent/settings/sound_packs.h>

#include <gtest/gtest.h>

namespace
{
	struct CResetLanguage
	{
		~CResetLanguage() { g_Localization.Load("", nullptr, nullptr); }
	};
}
TEST(LucentLocalization, EnglishRussianSwitchAndIndependentContexts)
{
	CResetLanguage Reset;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	ASSERT_TRUE(g_Localization.Load("", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Sounds");
	ASSERT_TRUE(g_Localization.Load("languages/russian.txt", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Звуки");
	EXPECT_STREQ(Localize("Categories", "Lucent Voting"), "Категории");
	EXPECT_STREQ(Localize("Start vote", "Lucent Voting"), "Начать голосование");
	EXPECT_STREQ(LCNTLocalize("Cursor motion blur"), "Размытие движения курсора");
	EXPECT_STREQ(LCNTLocalize("Import sound pack"), "Импортировать");
	EXPECT_STREQ(LCNTLocalize("Replaced %d of %d sounds"), "Заменено %d из %d звуков");
	EXPECT_STREQ(Localize("Country"), "Страна"); // Migrated uncontextual client text.
	EXPECT_STREQ(Localize("Endless jump", "Lucent HUD modifier"), "Бесконечные прыжки");
	for(const auto &Description : LucentSoundPacks::Descriptions)
		EXPECT_STRNE(LCNTLocalize(Description.m_pLabel), Description.m_pLabel);
	EXPECT_STREQ(LCNTLocalize("Missing translation stays English"), "Missing translation stays English");
	EXPECT_FALSE(g_Localization.Load("languages/does-not-exist.txt", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Звуки");
	ASSERT_TRUE(g_Localization.Load("languages/german.txt", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Sounds");
	EXPECT_STREQ(LCNTLocalize("Refresh"), "Refresh");
	EXPECT_STREQ(Localize("Endless jump", "Lucent HUD modifier"), "Endless jump");
	ASSERT_TRUE(g_Localization.Load("languages/russian.txt", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Звуки");
	ASSERT_TRUE(g_Localization.Load("", Storage.get(), nullptr));
	EXPECT_STREQ(LCNTLocalize("Sounds"), "Sounds");
}

TEST(LucentSoundPacks, PolishedLayoutFitsWideAndCompactViews)
{
	using LucentSettings::CSoundPackLayout;
	for(float Width : {280.f, 420.f, 619.f, 620.f, 900.f, 1400.f})
	{
		const CUIRect View{20, 30, Width, 400};
		const auto Layout = CSoundPackLayout::Calculate(View);
		EXPECT_GE(Layout.m_Left.w, 0);
		EXPECT_GE(Layout.m_Right.w, 0);
		EXPECT_LE(Layout.m_Left.x + Layout.m_Left.w, View.x + View.w);
		EXPECT_LE(Layout.m_Right.x + Layout.m_Right.w, View.x + View.w);
		if(Layout.m_TwoColumns)
		{
			EXPECT_LT(Layout.m_Left.w, Layout.m_Right.w);
			EXPECT_GE(Layout.m_Right.x - Layout.m_Left.x - Layout.m_Left.w, 24);
		}
		const int Columns = CSoundPackLayout::CategoryColumns(Layout.m_Right.w);
		EXPECT_GE(Columns, 1);
		EXPECT_LE(Columns, 5);
		EXPECT_GE(Layout.m_Right.w / Columns, 100);
	}
}

TEST(LucentSoundPacks, CategoryGapsAndContentSizedDialogs)
{
	using Layout = LucentSettings::CSoundPackLayout;
	EXPECT_FALSE(Layout::ShowCategoryCount(-1));
	EXPECT_FALSE(Layout::ShowCategoryCount(0));
	EXPECT_TRUE(Layout::ShowCategoryCount(1));
	for(float Width : {240.f, 420.f, 640.f, 1000.f})
	{
		const CUIRect Row{10, 20, Width, 36};
		const int Columns = Layout::CategoryColumns(Width);
		for(int i = 0; i < Columns; ++i)
		{
			const auto Button = Layout::CategoryButton(Row, i, Columns);
			EXPECT_GT(Button.w, 0);
			EXPECT_FLOAT_EQ(Button.h, 28);
			EXPECT_LE(Button.x + Button.w, Row.x + Row.w + .001f);
			if(i > 0)
			{
				const auto Previous = Layout::CategoryButton(Row, i - 1, Columns);
				EXPECT_NEAR(Button.x - Previous.x - Previous.w, Layout::Gap, .001f);
			}
		}
	}
	EXPECT_LT(Layout::CompareHeight(0), 260);
	EXPECT_FLOAT_EQ(Layout::ReviewHeight(1), 32);
	EXPECT_FLOAT_EQ(Layout::ReviewHeight(2), 64);
	EXPECT_FLOAT_EQ(Layout::ReviewHeight(100), 96);
	EXPECT_LT(Layout::CompareHeight(2), 360);
	EXPECT_EQ(Layout::CompareHeight(3), Layout::CompareHeight(100));
}
