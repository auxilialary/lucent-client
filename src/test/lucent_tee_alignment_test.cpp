#include <game/client/lucent/hud/tee_alignment.h>

#include <gtest/gtest.h>

using namespace LucentHud::Alignment;

TEST(LucentTeeAlignment, IndependentModesSwapControlledAndExcludeLocalTees)
{
	CTarget aTargets[] = {{0, 10, 10, true}, {1, 10, 20, true}, {2, 10, 30, true}, {3, 10, 40, true}};
	EXPECT_EQ(Select(aTargets, 4, 0, 1, true, -1), 1);
	EXPECT_EQ(Select(aTargets, 4, 0, 1, false, -1), 2);
	aTargets[0].m_Y = 25;
	EXPECT_EQ(Select(aTargets, 4, 1, 0, true, -1), 0);
	EXPECT_EQ(Select(aTargets, 4, 1, 0, false, -1), 2);
	aTargets[2].m_Valid = false;
	EXPECT_EQ(Select(aTargets, 4, 1, 0, false, -1), 3);
	EXPECT_EQ(Select(aTargets, 4, 1, -1, true, -1), -1);
	EXPECT_EQ(Select(aTargets, 4, -1, 0, false, -1), -1);
	aTargets[1].m_Valid = false;
	EXPECT_EQ(Select(aTargets, 4, 1, 0, false, -1), -1);
}

TEST(LucentTeeAlignment, WorldCoordinatesHysteresisAndNoCameraLimit)
{
	CTarget aTargets[] = {{0, .2f, -10000, true}, {1, 2.2f, 10000, true}};
	EXPECT_EQ(Select(aTargets, 2, 0, 1, true, -1), 1);
	aTargets[1].m_X = 2.9f;
	EXPECT_EQ(Select(aTargets, 2, 0, 1, true, -1), -1);
	EXPECT_EQ(Select(aTargets, 2, 0, 1, true, 1), 1);
	aTargets[1].m_X = 3.21f;
	EXPECT_EQ(Select(aTargets, 2, 0, 1, true, 1), -1);
	aTargets[1].m_X = .2f;
	aTargets[1].m_Y = -10000;
	EXPECT_EQ(Select(aTargets, 2, 0, 1, true, 1), -1);
}

TEST(LucentTeeAlignment, FreezeRequiresKnownServerState)
{
	EXPECT_FALSE(Frozen(false, -1, 1, 100, 1));
	EXPECT_FALSE(Frozen(true, 100, 0, 100, 1));
	EXPECT_FALSE(Frozen(true, 0, 0, 100, 1));
	EXPECT_TRUE(Frozen(true, 101, 0, 100, 1));
	EXPECT_TRUE(Frozen(true, -1, 0, 100, 1));
	EXPECT_TRUE(Frozen(true, 0, 1, 100, 1));
}

TEST(LucentTeeAlignment, FrameRateIndependentRevealFadeAndCrossfade)
{
	for(int Fps : {30, 60, 144})
	{
		CMode Mode;
		for(int i = 0; i < Fps; ++i)
			Mode.Update(4, false, 1.f / Fps);
		EXPECT_FLOAT_EQ(Mode.m_aLayers[Mode.m_Current].m_Alpha, 1);
		Mode.Update(5, true, 1.f / Fps);
		EXPECT_EQ(Mode.Target(), 5);
		EXPECT_GT(Mode.m_aLayers[1 - Mode.m_Current].m_Alpha, 0);
		EXPECT_LT(Mode.m_aLayers[Mode.m_Current].m_Alpha, 1);
		for(int i = 0; i < Fps; ++i)
			Mode.Update(5, true, 1.f / Fps);
		const float Reveal = Mode.m_aLayers[Mode.m_Current].m_Reveal;
		Mode.Update(5, false, 1.f / Fps);
		EXPECT_FLOAT_EQ(Mode.m_aLayers[Mode.m_Current].m_Reveal, Reveal);
		EXPECT_FLOAT_EQ(Mode.m_aLayers[Mode.m_Current].m_Alpha, 1);
		for(int i = 0; i < Fps; ++i)
			Mode.Update(-1, false, 1.f / Fps);
		for(const auto &Layer : Mode.m_aLayers)
		{
			EXPECT_EQ(Layer.m_Id, -1);
			EXPECT_FLOAT_EQ(Layer.m_Alpha, 0);
		}
	}
}

TEST(LucentTeeAlignment, PlacementClearsActualLowerBoundsIncludingSlide)
{
	for(int Pixels : {1080, 1440})
		for(int Scale : {50, 100, 150})
		{
			const LucentHud::CLayout Layout(300.f * 16 / 9, 300, Scale);
			const auto Bar = Layout.Hotbar(6);
			const LucentHud::CRect Status{Bar.x, Bar.y - Layout.Unit(30), Bar.w, Layout.Unit(25)};
			const LucentHud::CRect Bounds[] = {Bar, Status};
			const float Slide = 5.f * 300 / Pixels;
			const float Bottom = BottomAbove(Bounds, 2, Layout.m_Width, 300, Layout.Unit(99), Layout.Unit(3) + Slide);
			EXPECT_LT(Bottom + Slide, Status.y);
			EXPECT_GT(Bottom - Layout.Unit(12), 0);
		}
}

TEST(LucentTeeAlignment, LabelsKeepUtf8BoundariesAndMeasureTheEllipsis)
{
	const auto Measure = [](const char *pText) {
		float Width = 0;
		while(*pText)
		{
			const int Codepoint = str_utf8_decode(&pText);
			Width += Codepoint == 0x2026 ? 2.f : 3.f;
		}
		return Width;
	};
	for(const char *pName : {"Main", "Dummy", "Жёлтый猫🙂tester", "日本語のとても長い名前", "éééé"})
		for(float Limit : {0.f, 2.f, 8.f, 40.f, 120.f})
		{
			char aLabel[128];
			const float Width = FitLabel(aLabel, pName, Limit, Measure);
			EXPECT_TRUE(str_utf8_check(aLabel));
			EXPECT_LE(Width, Limit);
			EXPECT_FLOAT_EQ(Width, Measure(aLabel));
			if(Measure(pName) <= Limit)
				EXPECT_STREQ(aLabel, pName);
			else if(aLabel[0])
				EXPECT_NE(str_endswith(aLabel, "…"), nullptr);
		}
}

TEST(LucentTeeAlignment, StackReflowUsesFiniteFrameRateIndependentEaseOut)
{
	for(int Fps : {30, 60, 144})
	{
		CRowPosition Row;
		Row.Update(260, 0);
		Row.Update(240, 0);
		EXPECT_FLOAT_EQ(Row.m_Y, 260);
		for(int i = 0; i < Fps; ++i)
		{
			const float Prev = Row.m_Y;
			Row.Update(240, 1.f / Fps);
			EXPECT_LE(Row.m_Y, Prev);
			EXPECT_GE(Row.m_Y, 240);
		}
		EXPECT_FLOAT_EQ(Row.m_Y, 240);
		Row.Update(260, LucentStyle::Motion::Selection / 2);
		EXPECT_FLOAT_EQ(Row.m_Y, 257.5f);
		Row.Update(260, LucentStyle::Motion::Selection / 2);
		EXPECT_FLOAT_EQ(Row.m_Y, 260);
	}
}

TEST(LucentTeeAlignment, StackClearsStatusHotbarCoordinatesAndChatEnvelopes)
{
	for(float Aspect : {1.f, 4.f / 3, 16.f / 9, 21.f / 9})
		for(int Scale : {50, 100, 150})
			for(int Count : {0, 2, 8, 32})
			{
				const LucentHud::CLayout Layout(300 * Aspect, 300, Scale);
				std::array<float, LucentHud::MaxStatusItems> Widths;
				Widths.fill(Layout.Unit(8));
				const auto Status = Layout.Status(Widths, Count, 0, std::min(2, Count)).m_Bounds;
				for(float ChatTop : {50.f, 200.f})
				{
					const LucentHud::CRect Chat{0, ChatTop, 205, 300 - ChatTop};
					const LucentHud::CRect Bounds[] = {Layout.Hotbar(6), Status, {Layout.m_Width - 65, 240, 60, 10}, Chat};
					const float Gap = Layout.Unit(StackGap), Height = Layout.Unit(CapsuleHeight), Slide = 5.f * 300 / 1080;
					const float Width = Layout.Unit(112);
					const float Bottom = BottomAbove(Bounds, 4, Layout.m_Width, 300, Width, Gap + Slide);
					for(const auto &Rect : Bounds)
						if(Rect.w > 0 && Rect.x < (Layout.m_Width + Width) / 2 && Rect.x + Rect.w > (Layout.m_Width - Width) / 2)
							EXPECT_LT(Bottom + Slide, Rect.y);
					// Both rows have the same horizontal center, with no empty placeholder.
					const float Upper = Bottom - 2 * Height - Gap, Lower = Bottom - Height;
					EXPECT_FLOAT_EQ(Lower - (Upper + Height), Gap);
				}
			}
}
