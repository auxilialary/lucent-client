#include "test.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/client/graphics_threaded.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/chat.h>
#include <game/client/lucent/hud/layout.h>
#include <game/client/lucent/hud/visuals.h>
#include <game/client/lucent/hud/weapon_scale.h>
#include <game/client/lucent/motion/transition.h>
#include <game/client/lucent/scoreboard/layout.h>
#include <game/client/lucent/scoreboard/panel.h>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace
{
	struct CConfigRestore
	{
		CConfig m_Saved = g_Config;
		~CConfigRestore() { g_Config = m_Saved; }
	};
	struct CConfigSession
	{
		std::unique_ptr<IKernel> m_pKernel{IKernel::Create()};
		IConsole *m_pConsole;
		IConfigManager *m_pConfig;
		explicit CConfigSession(IStorage *pStorage)
		{
			m_pKernel->RegisterInterface(pStorage, false);
			m_pConsole = CreateConsole(CFGFLAG_CLIENT).release();
			m_pKernel->RegisterInterface(m_pConsole);
			m_pConfig = CreateConfigManager();
			m_pKernel->RegisterInterface(m_pConfig);
			m_pConsole->Init();
			m_pConfig->Init();
		}
	};
}

TEST(LucentHud, ConfigSurvivesFreshSessionsAndReset)
{
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_LucentHudScale, 100);
		// Set every real registered Lucent HUD variable through its console command.
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		char aCommand[128]; \
		str_format(aCommand, sizeof(aCommand), "%s %d", #ScriptName, (Def) == (Max) ? (Min) : (Max)); \
		Session.m_pConsole->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED); \
		EXPECT_EQ(g_Config.m_##Name, (Def) == (Max) ? (Min) : (Max)); \
	}
#include <game/client/lucent/hud/config_variables.h>
#undef MACRO_CONFIG_INT
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_LucentHudScale, 100);
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) EXPECT_EQ(g_Config.m_##Name, (Def) == (Max) ? (Min) : (Max));
#include <game/client/lucent/hud/config_variables.h>
#undef MACRO_CONFIG_INT
		Session.m_pConsole->ExecuteLine("lucent_hud_scale 1", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentHudScale, 50);
		Session.m_pConsole->ExecuteLine("lucent_hud_scale 9999", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentHudScale, 150);
		Session.m_pConsole->ExecuteLine("lucent_hud_opacity -9999", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentHudOpacity, 0);
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Session.m_pConfig->Reset(#ScriptName);
#include <game/client/lucent/hud/config_variables.h>
#undef MACRO_CONFIG_INT
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) EXPECT_EQ(g_Config.m_##Name, Def);
#include <game/client/lucent/hud/config_variables.h>
#undef MACRO_CONFIG_INT
	}
}

TEST(LucentHud, InventoryUsesOnlyKnownOwnershipAndKeepsProtocolOrder)
{
	for(unsigned Mask = 0; Mask < 64; ++Mask)
		for(int Active = -1; Active < 7; ++Active)
		{
			const auto Known = LucentHud::Weapons(Mask, Active, true);
			const unsigned ActiveBit = Active >= 0 && Active < 6 ? 1u << Active : 0;
			const unsigned Expected = Mask | ActiveBit;
			unsigned Seen = 0;
			for(int i = 0; i < Known.m_Count; ++i)
			{
				EXPECT_GE(Known.m_aIds[i], 0);
				EXPECT_LT(Known.m_aIds[i], 6);
				if(i > 0)
					EXPECT_LT(Known.m_aIds[i - 1], Known.m_aIds[i]);
				Seen |= 1u << Known.m_aIds[i];
			}
			EXPECT_EQ(Seen, Expected);
			const auto Unknown = LucentHud::Weapons(Mask, Active, false);
			EXPECT_EQ(Unknown.m_Count, ActiveBit ? 1 : 0);
			if(Unknown.m_Count)
				EXPECT_EQ(Unknown.m_aIds[0], Active);
		}
}

TEST(LucentHud, AnchorsRemainSymmetricAndSeparateAtSupportedResolutions)
{
	for(const auto Resolution : {std::pair{1920, 1080}, std::pair{2560, 1440}, std::pair{1280, 960}})
		for(int Scale : {50, 75, 100, 125, 150})
		{
			const float Width = 300.f * Resolution.first / Resolution.second;
			const float PixelScale = Resolution.second / 300.f;
			const LucentHud::CLayout Layout(Width, 300.f, Scale);
			EXPECT_FLOAT_EQ(Layout.Hotbar(0).w, 0);
			EXPECT_FLOAT_EQ(2 * LucentHud::SlotWidth, 44.f);
			EXPECT_FLOAT_EQ(2 * LucentHud::SlotGap, 5.f);
			float PreviousWidth = 0;
			for(int Count = 1; Count <= 6; ++Count)
			{
				const auto Bar = Layout.Hotbar(Count);
				EXPECT_NEAR(Bar.x + Bar.w / 2, Width / 2, .001f);
				EXPECT_GT(Bar.w, PreviousWidth);
				EXPECT_GE(Bar.x, 0);
				EXPECT_LE((Bar.x + Bar.w) * PixelScale, Resolution.first);
				EXPECT_LE((Bar.y + Bar.h) * PixelScale, Resolution.second);
				if(Count > 1)
					EXPECT_NEAR(Bar.w - PreviousWidth, Layout.BarUnit(LucentHud::SlotWidth + LucentHud::SlotGap), .001f);
				const auto First = Layout.Slot(0, Count);
				const auto Last = Layout.Slot(Count - 1, Count);
				// Content fits exactly: no empty placeholder slots or oversized outer padding.
				EXPECT_NEAR(First.x - Bar.x, Layout.BarUnit(LucentHud::BarPadding), .001f);
				EXPECT_NEAR(Bar.x + Bar.w - Last.x - Last.w, Layout.BarUnit(LucentHud::BarPadding), .001f);
				EXPECT_NEAR(Bar.h - First.h, Layout.BarUnit(2 * LucentHud::BarPadding), .001f);
				PreviousWidth = Bar.w;
				for(int i = 0; i < Count; ++i)
				{
					const auto Slot = Layout.Slot(i, Count);
					EXPECT_GT(Slot.x, Bar.x);
					EXPECT_LT(Slot.x + Slot.w, Bar.x + Bar.w);
					EXPECT_GT(Slot.y, Bar.y);
					EXPECT_LT(Slot.y + Slot.h, Bar.y + Bar.h);
					if(i > 0)
					{
						const auto Prev = Layout.Slot(i - 1, Count);
						EXPECT_GT(Slot.x, Prev.x + Prev.w);
					}
				}
			}
			for(bool Score : {false, true})
				for(bool Speed : {false, true})
				{
					const auto Coords = Layout.Coordinates(Layout.Unit(160), Score, Speed);
					EXPECT_GE(Coords.x, 0);
					EXPECT_GE(Coords.y, 0);
					EXPECT_LT(Coords.x + Coords.w, Width);
					EXPECT_LT(Coords.y + Coords.h, Layout.Hotbar(6).y);
					if(Score)
						EXPECT_LT(Coords.y + Coords.h, 229.f);
				}
			const auto Timer = Layout.Timer(96, true, true);
			EXPECT_GE(Timer.y, 0);
			EXPECT_NEAR(Timer.x + Timer.w / 2, Width / 2, .001f);
			for(float RecordingWidth : {40.f, 90.f, 160.f})
				for(float ReadoutOffset : {0.f, 18.f, 36.f})
				{
					const auto Recording = LucentHud::BelowIfOverlapping(Layout.TopRight(RecordingWidth, 12, ReadoutOffset), Timer, Layout.Unit(LucentHud::Gap));
					EXPECT_TRUE(Recording.x >= Timer.x + Timer.w || Recording.y >= Timer.y + Timer.h);
					EXPECT_GE(Recording.x, 0);
					EXPECT_LT(Recording.x + Recording.w, Width);
					EXPECT_LE(Recording.y + Recording.h, 300.f);
				}
		}
	EXPECT_EQ(LucentHud::Opacity(-100), 0.f);
	EXPECT_EQ(LucentHud::Opacity(1000), 1.f);
}

TEST(LucentHud, RotatedWeaponQuadsStayWithinProportionallyReducedHotbar)
{
	// Body sprite W/H and visual size from datasrc/content.py, in protocol order.
	const struct
	{
		const char *m_pName;
		float m_W, m_H, m_VisualSize;
	} aWeapons[] = {
		{"hammer", 4, 3, 96}, {"gun", 4, 2, 64}, {"shotgun", 8, 2, 96},
		{"grenade", 7, 2, 96}, {"laser", 7, 3, 92}, {"ninja", 8, 2, 96}};
	EXPECT_FLOAT_EQ(LucentHud::HotbarFactor, .85f);
	for(int Scale : {50, 75, 100, 125, 150})
	{
		const LucentHud::CLayout Layout(300.f * 16 / 9, 300.f, Scale);
		for(int Count = 1; Count <= 6; ++Count)
			for(int SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
				for(const auto &Weapon : aWeapons)
					for(float AnimatedScale : {.75f, .8125f, .9375f, 1.f})
					{
						SCOPED_TRACE(Weapon.m_pName);
						const auto Bar = Layout.Hotbar(Count);
						const auto Slot = Layout.Slot(SlotIndex, Count);
						const float Normalization = std::hypot(Weapon.m_W, Weapon.m_H);
						const float QuadW = Weapon.m_VisualSize * .25f * Weapon.m_W / Normalization;
						const float QuadH = Weapon.m_VisualSize * .25f * Weapon.m_H / Normalization;
						// At pi*7/4 both axis-aligned extents are (W+H)/sqrt(2).
						const float HalfExtent = (QuadW + QuadH) / std::sqrt(2.f) * LucentHud::WeaponIconScale * LucentHud::HotbarFactor * Layout.m_Scale * AnimatedScale / 2;
						const float CenterX = Slot.x + Slot.w / 2, CenterY = Slot.y + Slot.h / 2;
						EXPECT_GE(CenterX - HalfExtent, Slot.x - .001f);
						EXPECT_LE(CenterX + HalfExtent, Slot.x + Slot.w + .001f);
						// The existing outer padding accommodates the rotated hammer vertically.
						EXPECT_GE(CenterY - HalfExtent, Bar.y - .001f);
						EXPECT_LE(CenterY + HalfExtent, Bar.y + Bar.h + .001f);
					}
	}
}

TEST(LucentHud, IndependentTimerLinesHaveConsistentInsetsAndClearance)
{
	for(int Scale : {50, 75, 100, 125, 150})
	{
		const LucentHud::CLayout Layout(300.f * 16 / 9, 300.f, Scale);
		for(bool Main : {false, true})
			for(bool Secondary : {false, true})
			{
				const auto Bounds = Layout.Timer(96, Main, Secondary);
				if(!Main && !Secondary)
				{
					EXPECT_FLOAT_EQ(Bounds.w, 0);
					EXPECT_FLOAT_EQ(Bounds.h, 0);
					continue;
				}
				const float MainY = Layout.TimerLineY(Bounds, false, Main);
				const float SecondaryY = Layout.TimerLineY(Bounds, true, Main);
				const float FirstY = Main ? MainY : SecondaryY;
				const float BottomY = Secondary ? SecondaryY + Layout.Unit(LucentHud::BodySize) : MainY + Layout.Unit(LucentHud::TimerSize);
				EXPECT_NEAR(FirstY - Bounds.y, Layout.Unit(LucentHud::TimerInset), .001f);
				EXPECT_NEAR(Bounds.y + Bounds.h - BottomY, Layout.Unit(LucentHud::TimerInset), .001f);
				if(Main && Secondary)
					EXPECT_NEAR(SecondaryY - MainY - Layout.Unit(LucentHud::TimerSize), Layout.Unit(LucentHud::TimerLineGap), .001f);
				const auto Recording = LucentHud::BelowIfOverlapping(Layout.TopRight(230, 12), Bounds, Layout.Unit(LucentHud::Gap));
				EXPECT_TRUE(Recording.x >= Bounds.x + Bounds.w || Recording.x + Recording.w <= Bounds.x || Recording.y >= Bounds.y + Bounds.h + Layout.Unit(LucentHud::Gap) - .001f);
				const float NotificationY = std::max(50.f, Bounds.y + Bounds.h + 4.f);
				EXPECT_GE(NotificationY, Bounds.y + Bounds.h + 4.f);
			}
	}
}

TEST(LucentHud, ExpandedChatUsesCurrentHistoryInputAndAlwaysLargeStates)
{
	EXPECT_FALSE(CChat::ExpandedState(false, false, 1));
	EXPECT_TRUE(CChat::ExpandedState(true, false, 1));
	EXPECT_TRUE(CChat::ExpandedState(false, true, 1));
	EXPECT_TRUE(CChat::ExpandedState(false, false, 2));
	EXPECT_FALSE(CChat::ExpandedState(false, true, 0));
	// +show_chat requests expansion independently; HUD also checks actual chat visibility.
	EXPECT_TRUE(CChat::ExpandedState(true, false, 0));
}

TEST(LucentHud, VerticalClockAndTimerAdaptToContentAndVisibility)
{
	for(float ScreenWidth : {400.f, 300.f * 16 / 9})
		for(int Scale : {50, 75, 100, 125, 150})
			for(bool Main : {false, true})
				for(bool Secondary : {false, true})
					for(float ClockWidth : {0.f, 18.f, 120.f})
						for(float TimeWidth : {24.f, 96.f, 180.f})
						{
							const LucentHud::CLayout Layout(ScreenWidth, 300.f, Scale);
							const float GameWidth = Main || Secondary ? TimeWidth : 0.f;
							const auto Bounds = Layout.Timer(GameWidth, Main, Secondary, ClockWidth);
							if(!Main && !Secondary && ClockWidth == 0)
							{
								EXPECT_EQ(Bounds.w, 0);
								EXPECT_EQ(Bounds.h, 0);
								continue;
							}
							EXPECT_NEAR(Bounds.x + Bounds.w / 2, ScreenWidth / 2, .001f);
							EXPECT_NEAR(Bounds.w, Layout.Unit(std::max(GameWidth, ClockWidth) + 2 * LucentHud::TimerPaddingX), .001f);
							EXPECT_GE(Bounds.x, 0);
							EXPECT_LE(Bounds.x + Bounds.w, ScreenWidth);
							float LastBottom = Bounds.y + Layout.Unit(LucentHud::TimerInset);
							if(Main)
								LastBottom = Layout.TimerLineY(Bounds, false, Main) + Layout.Unit(LucentHud::TimerSize);
							if(Secondary)
							{
								const float Y = Layout.TimerLineY(Bounds, true, Main);
								if(Main)
									EXPECT_NEAR(Y - LastBottom, Layout.Unit(LucentHud::TimerLineGap), .001f);
								LastBottom = Y + Layout.Unit(LucentHud::BodySize);
							}
							if(ClockWidth > 0)
							{
								const float Y = Layout.TimerClockY(Bounds, Main, Secondary);
								EXPECT_NEAR(Y - LastBottom, Layout.Unit(Main || Secondary ? LucentHud::ClockLineGap : 0), .001f);
								LastBottom = Y + Layout.Unit(LucentHud::CaptionSize);
							}
							EXPECT_NEAR(Bounds.y + Bounds.h - LastBottom, Layout.Unit(LucentHud::TimerInset), .001f);
							const auto Recording = LucentHud::BelowIfOverlapping(Layout.TopRight(160, 12), Bounds, Layout.Unit(LucentHud::Gap));
							EXPECT_TRUE(Recording.x >= Bounds.x + Bounds.w || Recording.y >= Bounds.y + Bounds.h);
							EXPECT_LE(Recording.y + Recording.h, 300.f);
						}
}

TEST(LucentHud, JumpCountsPreserveUsedAndAvailableWithoutTenJumpTruncation)
{
	const auto Large = LucentHud::Jumps(255, 14, 2, false, false, true);
	EXPECT_EQ(Large.m_Total, 255);
	EXPECT_EQ(Large.m_Available, 240);
	EXPECT_EQ(Large.m_Used, 15);
	const auto Unknown = LucentHud::Jumps(255, 14, 2, false, false, false);
	EXPECT_EQ(Unknown.m_Total, 255);
	EXPECT_FALSE(Unknown.m_Known);
	EXPECT_EQ(LucentHud::Jumps(1, 0, 2, false, false, true).m_Available, 0);
	EXPECT_EQ(LucentHud::Jumps(-1, 0, 2, false, false, true).m_Available, 0);
	// Preserve the legacy recovery rule when the render snapshot has no spent-jump bit.
	EXPECT_EQ(LucentHud::Jumps(-1, 0, 0, false, false, true).m_Available, 1);
	EXPECT_EQ(LucentHud::Jumps(-1, 0, 0, true, false, true).m_Available, 1);
	for(int Total : {-1, 0, 1, 2, 6, 10, 255, INT32_MAX, INT32_MIN})
		for(int Used : {0, 1, 9, 300, INT32_MAX})
			for(bool Grounded : {false, true})
				for(bool Endless : {false, true})
				{
					const auto State = LucentHud::Jumps(Total, Used, 2, Grounded, Endless, true);
					EXPECT_GE(State.m_Available, 0);
					EXPECT_LE(State.m_Available, State.m_Total);
					EXPECT_EQ((int64_t)State.m_Available + State.m_Used, State.m_Total);
					if(Endless && State.m_Total > 0)
						EXPECT_GE(State.m_Available, 1);
				}
}

TEST(LucentHud, StatusRowsWrapCenterAndLeaveHotbarAnchored)
{
	for(float Width : {400.f, 300.f * 16 / 9, 700.f})
		for(int Scale : {50, 75, 100, 125, 150})
			for(float ChatWidth : {0.f, 200.f, 400.f})
			{
				const LucentHud::CLayout Layout(Width, 300, Scale);
				const auto Bar = Layout.Hotbar(6);
				std::array<float, LucentHud::MaxStatusItems> Widths{};
				Widths.fill(8);
				// A large jump counter followed by every available modifier fits the same packing path.
				Widths[0] = 64;
				for(int Count = 0; Count <= LucentHud::MaxStatusItems; ++Count)
				{
					const float Side = ChatWidth > 0 ? 5 + ChatWidth + Layout.Unit(LucentHud::Gap) : 0;
					const auto Status = Layout.Status(Widths, Count, Side);
					EXPECT_FLOAT_EQ(Layout.Hotbar(6).y, Bar.y);
					EXPECT_FLOAT_EQ(Layout.Hotbar(6).h, Bar.h);
					if(!Count)
					{
						EXPECT_EQ(Status.m_RowCount, 0);
						EXPECT_EQ(Status.m_Bounds.w, 0);
						continue;
					}
					EXPECT_GE(Status.m_Bounds.y, 0);
					EXPECT_LE(Status.m_Bounds.y + Status.m_Bounds.h, Bar.y - Layout.Unit(LucentHud::Gap) + .001f);
					for(int r = 0; r < Status.m_RowCount; ++r)
					{
						const auto Row = Status.m_aRows[r];
						EXPECT_NEAR(Row.x + Row.w / 2, Width / 2, .001f);
						EXPECT_GE(Row.x, 0);
						EXPECT_LE(Row.x + Row.w, Width);
						if(r > 0)
							EXPECT_GE(Row.y, Status.m_aRows[r - 1].y + Status.m_aRows[r - 1].h);
						if(Side > 0 && Width - 2 * Side >= Layout.Unit(48))
							EXPECT_GE(Row.x, Side - .001f);
					}
					for(bool Score : {false, true})
						for(bool Speed : {false, true})
						{
							const auto Coords = Layout.Coordinates(Layout.Unit(160), Score, Speed, Status.m_Bounds);
							EXPECT_GE(Coords.y, 0);
							if(Coords.x < Status.m_Bounds.x + Status.m_Bounds.w && Coords.x + Coords.w > Status.m_Bounds.x)
								EXPECT_LE(Coords.y + Coords.h, Status.m_Bounds.y - Layout.Unit(LucentHud::Gap) + .001f);
						}
					for(int i = 0; i < Count; ++i)
					{
						EXPECT_GE(Status.m_aItems[i].x, Status.m_Bounds.x);
						EXPECT_LE(Status.m_aItems[i].x + Status.m_aItems[i].w, Status.m_Bounds.x + Status.m_Bounds.w + .001f);
						EXPECT_GE(Status.m_aItems[i].y, Status.m_Bounds.y);
						EXPECT_LE(Status.m_aItems[i].y + Status.m_aItems[i].h, Status.m_Bounds.y + Status.m_Bounds.h + .001f);
					}
				}
			}
}

TEST(LucentHud, HotbarReducesExactlyFifteenPercentAndStatusPositionsStayAtV13Anchor)
{
	for(float Width : {400.f, 300.f * 16 / 9, 700.f})
		for(int Scale : {50, 75, 100, 125, 150})
		{
			const LucentHud::CLayout Layout(Width, 300.f, Scale);
			const float PreviousBarTop = 300.f - Layout.Unit(6.f + 24.f);
			EXPECT_FLOAT_EQ(Layout.InformationAnchorY(), PreviousBarTop);
			for(int Count = 1; Count <= 6; ++Count)
			{
				const auto Bar = Layout.Hotbar(Count);
				const float PreviousWidth = Layout.Unit(Count * 22.f + (Count - 1) * 2.5f + 4.f);
				EXPECT_NEAR(Bar.w, PreviousWidth * .85f, .001f);
				EXPECT_NEAR(Bar.h, Layout.Unit(24) * .85f, .001f);
				EXPECT_NEAR(Bar.y + Bar.h, 300.f - Layout.Unit(6), .001f);
				EXPECT_NEAR(Bar.x + Bar.w / 2, Width / 2, .001f);
				for(int i = 0; i < Count; ++i)
				{
					const auto Slot = Layout.Slot(i, Count);
					EXPECT_NEAR(Slot.w, Layout.Unit(22) * .85f, .001f);
					EXPECT_NEAR(Slot.h, Layout.Unit(20) * .85f, .001f);
					if(i > 0)
					{
						const auto Previous = Layout.Slot(i - 1, Count);
						EXPECT_NEAR(Slot.x - Previous.x - Previous.w, Layout.Unit(2.5f) * .85f, .001f);
					}
				}
			}
			std::array<float, LucentHud::MaxStatusItems> Widths{};
			Widths.fill(8);
			Widths[0] = 64;
			for(float ChatWidth : {0.f, 200.f, 400.f})
				for(int Count = 1; Count <= LucentHud::MaxStatusItems; ++Count)
				{
					const float Side = ChatWidth > 0 ? 5 + ChatWidth + Layout.Unit(LucentHud::Gap) : 0;
					const auto Status = Layout.Status(Widths, Count, Side);
					float ExpectedBottom = PreviousBarTop - Layout.Unit(4);
					if(Side > 0 && Width - 2 * Side < Layout.Unit(48))
						ExpectedBottom = std::min(ExpectedBottom, 200.f - Layout.Unit(4));
					EXPECT_NEAR(Status.m_Bounds.y + Status.m_Bounds.h, ExpectedBottom, .001f);
					const float PreviousHeight = Layout.Unit(Status.m_RowCount * 12.f + (Status.m_RowCount - 1) * 2.f);
					EXPECT_NEAR(Status.m_Bounds.h, PreviousHeight, .001f);
					for(int r = 0; r < Status.m_RowCount; ++r)
						EXPECT_NEAR(Status.m_aRows[r].y, ExpectedBottom - PreviousHeight + r * Layout.Unit(14), .001f);
				}
		}
}

TEST(LucentHud, TwoJumpIconsAndFourToSixModifiersCenterAsOnePair)
{
	const LucentHud::CLayout Layout(300.f * 16 / 9, 300, 100);
	std::array<float, LucentHud::MaxStatusItems> Widths{};
	Widths.fill(8);
	for(int Modifiers = 4; Modifiers <= 6; ++Modifiers)
	{
		const auto Status = Layout.Status(Widths, 2 + Modifiers, 209, 2);
		const auto Jumps = Status.m_aGroups[0], Mods = Status.m_aGroups[1];
		EXPECT_FLOAT_EQ(Jumps.w, 22.f);
		EXPECT_FLOAT_EQ(Mods.w, Modifiers * 8.f + (Modifiers - 1) * 2.f + 4.f);
		EXPECT_FLOAT_EQ(Jumps.h, 12.f);
		EXPECT_FLOAT_EQ(Mods.h, 12.f);
		EXPECT_NEAR(Mods.x - Jumps.x - Jumps.w, 3.f, .001f);
		EXPECT_NEAR((Jumps.x + Mods.x + Mods.w) / 2, Layout.m_Width / 2, .001f);
		EXPECT_LT(Jumps.x + Jumps.w / 2, Layout.m_Width / 2);
		EXPECT_GT(Mods.x + Mods.w / 2, Layout.m_Width / 2);
	}
	EXPECT_EQ(Layout.Status(Widths, 0, 0, 0).m_Bounds.w, 0);
	EXPECT_EQ(Layout.Status(Widths, 2, 0, 2).m_aGroups[1].w, 0);
	EXPECT_EQ(Layout.Status(Widths, 6, 0, 0).m_aGroups[0].w, 0);
}

TEST(LucentHud, SeparateStatusContainersWrapWithoutOverlapOrLostItems)
{
	for(float Width : {400.f, 300.f * 16 / 9, 700.f})
		for(int Scale : {50, 75, 100, 125, 150})
			for(float ChatWidth : {0.f, 200.f, 400.f})
				for(int Split : {0, 1, 2, 6})
					for(int Count = 1; Count <= LucentHud::MaxStatusItems; ++Count)
					{
						const LucentHud::CLayout Layout(Width, 300, Scale);
						std::array<float, LucentHud::MaxStatusItems> Widths{};
						Widths.fill(8);
						if(Split == 1)
							Widths[0] = 64; // Large-jump numeric element.
						const int Jumps = std::min(Split, Count);
						const float Side = ChatWidth > 0 ? 5 + ChatWidth + Layout.Unit(LucentHud::Gap) : 0;
						const auto Status = Layout.Status(Widths, Count, Side, Jumps);
						EXPECT_NEAR(Status.m_Bounds.x + Status.m_Bounds.w / 2, Width / 2, .001f);
						EXPECT_GE(Status.m_Bounds.x, 0);
						EXPECT_GE(Status.m_Bounds.y, 0);
						EXPECT_LE(Status.m_Bounds.x + Status.m_Bounds.w, Width + .001f);
						EXPECT_LE(Status.m_Bounds.y + Status.m_Bounds.h, Layout.InformationAnchorY() - Layout.Unit(LucentHud::Gap) + .001f);
						const auto A = Status.m_aGroups[0], B = Status.m_aGroups[1];
						if(Jumps > 0 && Jumps < Count)
						{
							const bool Horizontal = B.x >= A.x + A.w;
							if(Horizontal)
								EXPECT_NEAR(B.x - A.x - A.w, Layout.Unit(3), .001f);
							else
								EXPECT_NEAR(B.y - A.y - A.h, Layout.Unit(3), .001f);
						}
						for(int i = 0; i < Count; ++i)
						{
							const auto Group = Status.m_aGroups[i < Jumps ? 0 : 1];
							const auto Item = Status.m_aItems[i];
							EXPECT_GT(Group.w, 0);
							EXPECT_GE(Item.x, Group.x - .001f);
							EXPECT_GE(Item.y, Group.y - .001f);
							EXPECT_LE(Item.x + Item.w, Group.x + Group.w + .001f);
							EXPECT_LE(Item.y + Item.h, Group.y + Group.h + .001f);
						}
						const auto Coords = Layout.Coordinates(Layout.Unit(160), true, true, Status.m_Bounds);
						EXPECT_GE(Coords.y, 0);
						if(Coords.x < Status.m_Bounds.x + Status.m_Bounds.w && Coords.x + Coords.w > Status.m_Bounds.x)
							EXPECT_LE(Coords.y + Coords.h, Status.m_Bounds.y - Layout.Unit(LucentHud::Gap) + .001f);
					}
}

TEST(LucentHud, RoundedPanelsClampToTheirOwnGeometry)
{
	for(float Scale : {.5f, 1.f, 1.5f})
		for(float Height : {0.f, 8.f, 12.f, 20.4f, 34.f})
			for(float Width : {0.f, 6.f, 22.f, 120.f})
			{
				const LucentHud::CRect Rect{0, 0, Width * Scale, Height * Scale};
				const float Radius = LucentHud::PanelRadius(Rect, Scale);
				EXPECT_GE(Radius, 0);
				EXPECT_LE(Radius, Rect.h / 2);
				EXPECT_LE(Radius, Rect.w / 2);
				EXPECT_LE(Radius, LucentHud::Radius * Scale);
			}
	// Small utility panels remain capsule-like; larger panels retain soft corners
	// without inheriting the tiny menu input radius or changing their bounds.
	EXPECT_FLOAT_EQ(LucentHud::PanelRadius({0, 0, 100, 20}, 1), 7.f);
	EXPECT_FLOAT_EQ(LucentHud::PanelRadius({0, 0, 100, 8}, 1), 4.f);
	for(int Scale : {50, 100, 150})
	{
		const LucentHud::CLayout Layout(533.33f, 300, Scale);
		const auto Bar = Layout.Hotbar(6), Slot = Layout.Slot(0, 6);
		EXPECT_FLOAT_EQ(LucentHud::PanelRadius(Bar, Layout.m_Scale * LucentHud::HotbarFactor), Layout.BarUnit(7));
		EXPECT_FLOAT_EQ(LucentHud::PanelRadius(Slot, Layout.m_Scale * LucentHud::HotbarFactor), Layout.BarUnit(7));
		EXPECT_FLOAT_EQ(LucentHud::PanelRadius({0, 0, Layout.Unit(60), Layout.Unit(18)}, Layout.m_Scale), Layout.Unit(7));
		EXPECT_FLOAT_EQ(LucentHud::PanelRadius({0, 0, Layout.Unit(60), Layout.Unit(12)}, Layout.m_Scale), Layout.Unit(6));
	}
}

TEST(LucentHud, PanelTextRetainsContrastOverBrightAndDarkMapSamples)
{
	// Numerical compositing reference, not a substitute for rendered map QA.
	auto Composite = [](ColorRGBA Foreground, ColorRGBA Background) {
		return ColorRGBA(Foreground.r * Foreground.a + Background.r * (1 - Foreground.a),
			Foreground.g * Foreground.a + Background.g * (1 - Foreground.a),
			Foreground.b * Foreground.a + Background.b * (1 - Foreground.a), 1);
	};
	auto Luminance = [](ColorRGBA Color) {
		auto Linear = [](float Value) { return Value <= .04045f ? Value / 12.92f : std::pow((Value + .055f) / 1.055f, 2.4f); };
		return .2126f * Linear(Color.r) + .7152f * Linear(Color.g) + .0722f * Linear(Color.b);
	};
	const float Alpha = LucentHud::Opacity(85);
	for(bool Top : {false, true})
		for(float Map : {0.f, 1.f})
		{
			const auto Background = Composite(LucentHud::PanelSurface(Alpha, Top), ColorRGBA(Map, Map, Map, 1));
			for(const auto Text : {LucentHud::Text(Alpha), LucentHud::Secondary(Alpha)})
			{
				const auto Ink = Composite(Text, Background);
				EXPECT_GE((Luminance(Ink) + .05f) / (Luminance(Background) + .05f), 4.5f);
			}
		}
	EXPECT_FLOAT_EQ(LucentHud::PanelSurface(0, true).a, 0);
}

TEST(LucentHud, TimerGlyphsUseTargetPixelSizesAndLeaveRoomBetweenRows)
{
	for(int Height : {1080, 1440, 2160})
		for(int Scale : {50, 75, 100, 125, 150})
		{
			const LucentHud::CLayout Layout(300.f * 16 / 9, 300, Scale);
			for(float Font : {LucentHud::TimerSize, LucentHud::BodySize, LucentHud::CaptionSize})
			{
				const float Size = Layout.Unit(Font);
				const float Target = LucentHud::TargetFontSize(Size, Height, 300);
				const int Pixels = (int)(Target * Height / 300.f);
				EXPECT_EQ(Pixels, (int)std::clamp(std::round(Size * Height / 300), 6.f, 128.f));
				EXPECT_FLOAT_EQ(LucentHud::TargetFontSize(Target, Height, 300), Target);
			}
			const auto Bounds = Layout.Timer(96, true, true, 18);
			const float MainY = Layout.TimerLineY(Bounds, false, true);
			const float SecondaryY = Layout.TimerLineY(Bounds, true, true);
			const float ClockY = Layout.TimerClockY(Bounds, true, true);
			const float MainSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::TimerSize), Height, 300);
			const float SecondarySize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::BodySize), Height, 300);
			const float ClockSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::CaptionSize), Height, 300);
			EXPECT_GT(SecondaryY, MainY + MainSize);
			EXPECT_GT(ClockY, SecondaryY + SecondarySize);
			EXPECT_LE(ClockY + ClockSize, Bounds.y + Bounds.h);
		}
}

TEST(LucentHud, TimerFormatCellsStayStableAcrossSecondsMinutesHoursAndDays)
{
	struct CTimeCase
	{
		int64_t m_Seconds;
		const char *m_pKey;
	};
	const CTimeCase aCases[] = {
		{0, "00:00"}, {1, "00:00"}, {9, "00:00"}, {10, "00:00"},
		{59, "00:00"}, {60, "00:00"}, {599, "00:00"}, {600, "00:00"},
		{3599, "00:00"}, {3600, "00:00:00"}, {35999, "00:00:00"},
		{36000, "00:00:00"}, {86399, "00:00:00"}, {86400, "0d 00:00:00"},
		{9 * 86400, "0d 00:00:00"}, {10 * 86400, "00d 00:00:00"}};
	for(const auto &Case : aCases)
	{
		char aTime[64], aFormat[64];
		str_time(Case.m_Seconds * 100, ETimeFormat::DAYS, aTime, sizeof(aTime));
		LucentHud::TimerFormatTemplate(aFormat, aTime);
		EXPECT_STREQ(aFormat, Case.m_pKey);
		for(int Height : {1080, 1440})
		{
			const LucentHud::CLayout Layout(300.f * 16 / 9, 300, 100);
			const auto Bounds = Layout.Timer(96, true, false, 18);
			EXPECT_NEAR((Bounds.x + Bounds.w / 2) * Height / 300, (Layout.m_Width / 2) * Height / 300, .001f);
		}
	}
	char aFormat[64];
	LucentHud::TimerFormatTemplate(aFormat, "Finish 12:34.56 -00:01.23");
	EXPECT_STREQ(aFormat, "Finish 00:00.00 -00:00.00");
	EXPECT_FLOAT_EQ(LucentHud::TimerSize, 9.f);
}

TEST(LucentHud, ShippedDefaultFontProvidesTabularDigitAdvances)
{
	// Read the actual shipped TrueType cmap/hmtx rather than assume glyph ink widths are equal.
	const auto Root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
	std::ifstream File(Root / "data/fonts/DejaVuSans.ttf", std::ios::binary);
	ASSERT_TRUE(File.good());
	const std::vector<unsigned char> Data((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	ASSERT_GT(Data.size(), 1024u);
	auto U16 = [&](size_t Offset) -> uint16_t { return (Data.at(Offset) << 8) | Data.at(Offset + 1); };
	auto U32 = [&](size_t Offset) -> uint32_t { return ((uint32_t)Data.at(Offset) << 24) | ((uint32_t)Data.at(Offset + 1) << 16) | ((uint32_t)Data.at(Offset + 2) << 8) | Data.at(Offset + 3); };
	auto Table = [&](const char *pName) -> size_t {
		for(size_t i = 0; i < U16(4); ++i)
		{
			const size_t Offset = 12 + 16 * i;
			if(Data.at(Offset) == pName[0] && Data.at(Offset + 1) == pName[1] && Data.at(Offset + 2) == pName[2] && Data.at(Offset + 3) == pName[3])
				return U32(Offset + 8);
		}
		return 0;
	};
	const size_t Cmap = Table("cmap"), Hhea = Table("hhea"), Hmtx = Table("hmtx");
	ASSERT_NE(Cmap, 0u);
	ASSERT_NE(Hhea, 0u);
	ASSERT_NE(Hmtx, 0u);
	size_t Subtable = 0;
	for(size_t i = 0; i < U16(Cmap + 2); ++i)
	{
		const size_t Offset = Cmap + 4 + 8 * i;
		const size_t Candidate = Cmap + U32(Offset + 4);
		if(U16(Offset) == 3 && U16(Offset + 2) == 1 && U16(Candidate) == 4)
			Subtable = Candidate;
	}
	ASSERT_NE(Subtable, 0u);
	const size_t Segments = U16(Subtable + 6) / 2;
	const size_t Ends = Subtable + 14, Starts = Ends + 2 * Segments + 2;
	const size_t Deltas = Starts + 2 * Segments, Ranges = Deltas + 2 * Segments;
	const int Metrics = U16(Hhea + 34);
	ASSERT_GT(Metrics, 0);
	int Advance = -1;
	for(int Code = '0'; Code <= '9'; ++Code)
	{
		int Glyph = 0;
		for(size_t i = 0; i < Segments; ++i)
		{
			if(U16(Starts + 2 * i) > Code || U16(Ends + 2 * i) < Code)
				continue;
			const int Delta = (int16_t)U16(Deltas + 2 * i);
			const int Range = U16(Ranges + 2 * i);
			Glyph = Range ? U16(Ranges + 2 * i + Range + 2 * (Code - U16(Starts + 2 * i))) : Code;
			if(!Range || Glyph)
				Glyph = (Glyph + Delta) & 65535;
			break;
		}
		ASSERT_GT(Glyph, 0);
		const int Current = U16(Hmtx + 4 * std::min(Glyph, Metrics - 1));
		if(Advance < 0)
			Advance = Current;
		EXPECT_EQ(Current, Advance) << "digit " << (char)Code;
	}
}

TEST(LucentHud, WeaponSelectionRetargetsFromVisibleValueAtEveryFrameRate)
{
	for(int Fps : {30, 60, 144, 240})
	{
		LucentStyle::Motion::CTransition Selection;
		const double Duration = LucentStyle::Motion::Selection;
		Selection.Update(true, 10, true, Duration);
		float Previous = 0;
		for(int Frame = 1; Frame <= Fps; ++Frame)
		{
			const float Value = Selection.Update(true, 10 + Frame / double(Fps), true, Duration);
			EXPECT_GE(Value, Previous);
			EXPECT_LE(Value, 1);
			Previous = Value;
		}
		EXPECT_FLOAT_EQ(Previous, 1);
		Selection.Update(false, 11, true, Duration);
		const float Fading = Selection.Update(false, 11 + Duration / 2, true, Duration);
		EXPECT_GT(Fading, 0);
		EXPECT_LT(Fading, 1);
		EXPECT_FLOAT_EQ(Selection.Update(true, 11 + Duration / 2, true, Duration), Fading);
		EXPECT_FLOAT_EQ(Selection.Update(true, 12, true, Duration), 1);
	}
}

TEST(LucentHud, WeaponScaleMovesBothSpritesAndRetargetsWithoutJumps)
{
	LucentHud::CWeaponScale State;
	const auto Inventory = LucentHud::Weapons(63, 0, true);
	State.Update(Inventory, 0, 7, 10);
	EXPECT_FLOAT_EQ(State.Scale(0), 1);
	EXPECT_FLOAT_EQ(State.Scale(1), .75f);
	State.Update(Inventory, 1, 7, 11);
	EXPECT_FLOAT_EQ(State.Scale(0), 1);
	EXPECT_FLOAT_EQ(State.Scale(1), .75f);
	State.Update(Inventory, 1, 7, 11.1);
	// Half of 200 ms, cubic ease-out = 0.875 in both directions.
	EXPECT_NEAR(State.Scale(0), .78125f, .000001f);
	EXPECT_NEAR(State.Scale(1), .96875f, .000001f);
	const float Old = State.Scale(0), New = State.Scale(1);
	State.Update(Inventory, 0, 7, 11.1);
	EXPECT_FLOAT_EQ(State.Scale(0), Old);
	EXPECT_FLOAT_EQ(State.Scale(1), New);
	State.Update(Inventory, 0, 7, 11.3);
	EXPECT_FLOAT_EQ(State.Scale(0), 1);
	EXPECT_FLOAT_EQ(State.Scale(1), .75f);

	for(int Fps : {30, 60, 144, 240})
	{
		State = {};
		State.Update(Inventory, 0, 7, 20);
		State.Update(Inventory, 1, 7, 21);
		float PreviousOld = 1, PreviousNew = .75f;
		for(int Frame = 1; Frame <= (int)std::ceil(.2 * Fps); ++Frame)
		{
			State.Update(Inventory, 1, 7, 21 + std::min(Frame / double(Fps), .2));
			EXPECT_LE(State.Scale(0), PreviousOld);
			EXPECT_GE(State.Scale(1), PreviousNew);
			EXPECT_GE(State.Scale(0), .75f);
			EXPECT_LE(State.Scale(1), 1);
			PreviousOld = State.Scale(0);
			PreviousNew = State.Scale(1);
		}
		EXPECT_FLOAT_EQ(State.Scale(0), .75f);
		EXPECT_FLOAT_EQ(State.Scale(1), 1);
	}
}

TEST(LucentHud, WeaponScaleHandlesInventoryNinjaOwnerAndSessionReset)
{
	LucentHud::CWeaponScale State;
	for(unsigned Mask = 0; Mask < 64; ++Mask)
		for(int Active = 0; Active < 6; ++Active)
		{
			State = {};
			const auto Inventory = LucentHud::Weapons(Mask, Active, true);
			State.Update(Inventory, Active, 7, 10);
			for(int Slot = 0; Slot < Inventory.m_Count; ++Slot)
				EXPECT_FLOAT_EQ(State.Scale(Inventory.m_aIds[Slot]), Inventory.m_aIds[Slot] == Active ? 1.f : .75f);
		}
	State = {};
	State.Update(LucentHud::Weapons(31, 0, true), 0, 7, 20);
	State.Update(LucentHud::Weapons(63, 5, true), 5, 7, 21);
	EXPECT_FLOAT_EQ(State.Scale(5), .75f); // Newly obtained ninja grows from inactive size.
	State.Update(LucentHud::Weapons(63, 5, true), 5, 7, 21.2);
	EXPECT_FLOAT_EQ(State.Scale(5), 1);
	State.Update(LucentHud::Weapons(31, 0, true), 0, 7, 22);
	State.Update(LucentHud::Weapons(63, 5, true), 5, 7, 23);
	EXPECT_FLOAT_EQ(State.Scale(5), .75f); // Expired ninja cannot reuse the old full scale.
	State.Update(LucentHud::Weapons(63, 2, true), 2, 8, 23.01);
	EXPECT_FLOAT_EQ(State.Scale(2), 1); // Spectator/dummy owner switch starts settled.
	EXPECT_FLOAT_EQ(State.Scale(5), .75f);
	State.Update({}, -1, 8, 24);
	State.Update(LucentHud::Weapons(0, 4, false), 4, 8, 25);
	EXPECT_FLOAT_EQ(State.Scale(4), 1); // Unknown inventory uses only the active snapshot weapon.
	State = {}; // CHud::OnReset clears the same presentation state on connection changes.
	State.Update(LucentHud::Weapons(63, 3, true), 3, 8, 26);
	EXPECT_FLOAT_EQ(State.Scale(3), 1);
	EXPECT_FLOAT_EQ(State.Scale(4), .75f);
}

TEST(LucentHud, NeutralWeaponSamplingPreservesMipmapsAndWorldPolicy)
{
	for(float Bias : {-15.f, -.5f, 0.f, .5f, 15.f})
	{
		EXPECT_FLOAT_EQ(TextureFlag::LodBias(0, Bias), Bias);
		EXPECT_FLOAT_EQ(TextureFlag::LodBias(TextureFlag::UI_SPRITE, Bias), 0);
	}
	EXPECT_EQ(TextureFlag::UI_SPRITE & TextureFlag::NO_MIPMAPS, 0u);
	EXPECT_TRUE(TextureFlag::LimitMipLevels(0, 2048, 1536));
	EXPECT_FALSE(TextureFlag::LimitMipLevels(TextureFlag::UI_SPRITE, 2048, 1536));
	EXPECT_FALSE(TextureFlag::LimitMipLevels(0, 256, 64));
	EXPECT_EQ(TextureFlag::UI_SPRITE & (TextureFlag::NO_2D_TEXTURE | TextureFlag::TO_3D_TEXTURE | TextureFlag::TO_2D_ARRAY_TEXTURE), 0u);
	// At 1080p the original shotgun crop is 256x64, while its active
	// unrotated quad is about 65.5x16.4 px. Shrinking to .75 raises the
	// necessary LOD by log2(1/.75); a negative bias underfilters both states.
	const float PixelsPerUnit = 1080.f / 300;
	const float ActiveWidth = 24.f * 8 / std::hypot(8.f, 2.f) * LucentHud::WeaponIconScale * LucentHud::HotbarFactor * PixelsPerUnit;
	const float ActiveLod = std::log2(256 / ActiveWidth);
	const float InactiveLod = std::log2(256 / (ActiveWidth * .75f));
	EXPECT_GT(InactiveLod, ActiveLod);
	EXPECT_NEAR(InactiveLod - ActiveLod, std::log2(1.f / .75f), .000001f);
	EXPECT_LT(InactiveLod + TextureFlag::LodBias(0, -.5f), InactiveLod);
	EXPECT_FLOAT_EQ(InactiveLod + TextureFlag::LodBias(TextureFlag::UI_SPRITE, -.5f), InactiveLod);
}

TEST(LucentHud, PanelPaintAndTextAnchorsUseOneDevicePixelGrid)
{
	for(int Height : {360, 720, 1080, 1440, 2160, 4320})
		for(float Aspect : {1.f, 4.f / 3, 16.f / 9, 21.f / 9})
			for(int Scale : {50, 75, 100, 125, 150})
			{
				const LucentHud::CLayout Layout(300 * Aspect, 300, Scale);
				const float Pixel = 300.f / Height;
				for(vec2 Origin : {vec2(0, 0), vec2(-13.25f, 4.125f)})
				{
					const LucentHud::CPixelGrid Grid(Origin, {Pixel, Pixel});
					const struct
					{
						LucentHud::CRect m_Rect;
						float m_Scale;
					} Samples[] = {
						{Layout.Timer(80, true, true, 20), Layout.m_Scale},
						{Layout.Hotbar(6), Layout.m_Scale * LucentHud::HotbarFactor},
						{Layout.Slot(2, 6), Layout.m_Scale * LucentHud::HotbarFactor},
						{Layout.Coordinates(Layout.Unit(90), false, true), Layout.m_Scale},
						{{11, 43, Layout.Unit(36), Layout.Unit(12)}, Layout.m_Scale},
						{{11, 43, Layout.Unit(72), Layout.Unit(18)}, Layout.m_Scale}};
					for(const auto &Sample : Samples)
					{
						auto Logical = Sample.m_Rect;
						Logical.x += Pixel * .37f;
						Logical.y += Pixel * .49f;
						const auto G = LucentHud::CPanelGeometry::Calculate(Logical, Sample.m_Scale, Grid);
						ASSERT_GT(G.m_Count, 0);
						EXPECT_LE(std::abs(G.m_Bounds.x - Logical.x), Pixel / 2 + .0001f);
						EXPECT_LE(std::abs(G.m_Bounds.y - Logical.y), Pixel / 2 + .0001f);
						EXPECT_LE(std::abs(G.m_Bounds.x + G.m_Bounds.w - Logical.x - Logical.w), Pixel / 2 + .0001f);
						EXPECT_LE(std::abs(G.m_Bounds.y + G.m_Bounds.h - Logical.y - Logical.h), Pixel / 2 + .0001f);
						for(vec2 Point : {vec2(G.m_Bounds.x, G.m_Bounds.y), vec2(G.m_Bounds.x + G.m_Bounds.w, G.m_Bounds.y + G.m_Bounds.h),
							    Grid.Snap({Logical.x + Logical.w / 2, Logical.y + Layout.Unit(3)})})
						{
							for(int Axis = 0; Axis < 2; ++Axis)
							{
								const float Phase = (Point[Axis] - Origin[Axis]) / Pixel;
								EXPECT_NEAR(Phase, std::round(Phase), .002f);
								EXPECT_NEAR(Grid.Snap(Point)[Axis], Point[Axis], Pixel * .002f);
							}
						}
						EXPECT_NEAR(2 * G.m_Radius / Pixel, std::round(2 * G.m_Radius / Pixel), .002f);
						EXPECT_NEAR(2 * G.m_BorderWidth / Pixel, std::round(2 * G.m_BorderWidth / Pixel), .002f);
					}
				}
			}
}

TEST(LucentHud, RoundedPanelCoverageIsNestedClosedAndSmoothAtPhysicalResolution)
{
	for(int Height : {360, 720, 1080, 1440, 2160, 4320})
		for(float Scale : {.5f, 1.f, 1.5f})
			for(float Width : {6.f, 22.f, 120.f})
				for(float HeightUnits : {8.f, 12.f, 18.f, 24.f, 31.f})
				{
					const float Pixel = 300.f / Height;
					const LucentHud::CPixelGrid Grid({0, 0}, {Pixel, Pixel});
					const auto G = LucentHud::CPanelGeometry::Calculate({-7.33f, 5.22f, Width * Scale, HeightUnits * Scale}, Scale, Grid);
					ASSERT_GT(G.m_Count, 0);
					EXPECT_LE(G.m_Count, LucentHud::CPanelGeometry::MaxPoints);
					EXPECT_LE(G.m_Radius, std::min(G.m_Bounds.w, G.m_Bounds.h) / 2);
					EXPECT_NEAR(2 * G.m_Feather, Pixel, .00001f);
					EXPECT_GT(G.m_BorderPeak, 0);
					EXPECT_LE(G.m_BorderPeak, 1);
					const float ChordError = (G.m_Radius + G.m_Feather) * (1 - std::cos(pi / (4 * G.m_Steps)));
					EXPECT_LE(ChordError / Pixel, .126f);
					const auto Offsets = G.BorderOffsets();
					EXPECT_GT(G.m_Radius + Offsets[0], 0);
					for(int I = 1; I < 4; ++I)
						EXPECT_GE(Offsets[I], Offsets[I - 1]);
					double PreviousArea = 0;
					for(float Offset : Offsets)
					{
						const auto Boundary = G.Boundary(Offset);
						double Area = 0;
						for(int I = 0; I < G.m_Count; ++I)
						{
							const auto P = Boundary[I], Q = Boundary[(I + 1) % G.m_Count];
							EXPECT_TRUE(std::isfinite(P.x) && std::isfinite(P.y));
							EXPECT_NEAR(length(G.m_Normals[I]), 1, .000001f);
							EXPECT_GE(P.x, G.m_Bounds.x - G.m_Feather - .0001f);
							EXPECT_LE(P.x, G.m_Bounds.x + G.m_Bounds.w + G.m_Feather + .0001f);
							EXPECT_GE(P.y, G.m_Bounds.y - G.m_Feather - .0001f);
							EXPECT_LE(P.y, G.m_Bounds.y + G.m_Bounds.h + G.m_Feather + .0001f);
							Area += double(P.x) * Q.y - double(Q.x) * P.y;
						}
						EXPECT_GT(Area, 0);
						EXPECT_GE(Area + .0001, PreviousArea);
						PreviousArea = Area;
					}
				}
}

TEST(LucentHud, TinyAndInvalidPanelCoverageNeverInvertsCorners)
{
	const float Nan = std::numeric_limits<float>::quiet_NaN(), Infinity = std::numeric_limits<float>::infinity();
	const LucentHud::CPixelGrid Grid({0, 0}, {.25f, .25f});
	for(const auto Rect : {LucentHud::CRect{0, 0, 0, 10}, {0, 0, -1, 10}, {0, 0, 10, 0}, {Nan, 0, 10, 10}, {0, 0, Infinity, 10}})
		EXPECT_EQ(LucentHud::CPanelGeometry::Calculate(Rect, 1, Grid).m_Count, 0);
	for(float Scale : {0.f, -1.f, Nan, Infinity})
		EXPECT_EQ(LucentHud::CPanelGeometry::Calculate({0, 0, 10, 10}, Scale, Grid).m_Count, 0);
	for(float Side : {.01f, .1f, .25f, .375f, .5f, 1.f})
	{
		const auto G = LucentHud::CPanelGeometry::Calculate({.03f, .07f, Side, Side}, 1, Grid);
		if(G.m_Count == 0)
			continue;
		EXPECT_GT(G.m_Feather, 0);
		EXPECT_GT(G.m_Radius + G.BorderOffsets()[0], 0);
		EXPECT_TRUE(std::isfinite(G.m_BorderPeak));
		EXPECT_LE(G.m_BorderPeak, 1);
	}
	const LucentHud::CPixelGrid InvalidGrid({Nan, Infinity}, {0, Nan});
	const auto Point = InvalidGrid.Snap({2.3f, 4.7f});
	EXPECT_TRUE(std::isfinite(Point.x) && std::isfinite(Point.y));
}

// Scoreboard uses the same UI-independent geometry and Motion helpers as production.

TEST(LucentScoreboard, AllPlayersAndSpectatorsFitAtSupportedAspectRatios)
{
	for(float Aspect : {.75f, 1.f, 4.f / 3, 16.f / 9, 21.f / 9, 32.f / 9})
		for(int Count : {0, 1, 2, 16, 32, 64, 128})
			for(bool Teams : {false, true})
				for(int Red : {0, Count / 2, Count})
					for(int Specs : {0, 1, 128 - Count})
					{
						if(Count + Specs > 128)
							continue;
						const int R = Teams ? Red : Count, B = Teams ? Count - Red : 0;
						const auto L = LucentScoreboard::Calculate(600 * Aspect, 600, R, B, Teams, Specs, 620);
						const int PerTeam = L.m_RedColumns;
						ASSERT_GT(PerTeam, 0);
						EXPECT_LE(L.m_Width, 600 * Aspect - 32 + .001f);
						EXPECT_LE(L.m_Height + L.m_SpectatorHeight, 480.001f);
						EXPECT_GT(L.ColumnWidth(), 0);
						EXPECT_GT(L.m_Row, 0);
						EXPECT_EQ(L.m_SpectatorHeight == 0, Specs == 0);
						int Seen = 0;
						for(int Group = 0; Group < (Teams ? 2 : 1); ++Group)
						{
							const int TeamCount = Group ? B : R;
							const int PerTeam = Group ? L.m_BlueColumns : L.m_RedColumns;
							for(int C = 0; C < PerTeam; ++C)
							{
								const auto Range = LucentScoreboard::ColumnRange(TeamCount, C, PerTeam);
								const int Start = Range.m_Start, End = Range.m_End;
								if(End > Start)
									Seen += End - Start;
								EXPECT_LE(End - Start, L.m_Rows);
							}
						}
						EXPECT_EQ(Seen, Count);
						EXPECT_GE(L.m_SpectatorColumns * L.m_SpectatorRows, Specs);
						EXPECT_LE(28 + L.m_SpectatorRows * L.m_SpectatorRow, Specs ? L.m_SpectatorHeight + .001f : 28);
					}
}

TEST(LucentScoreboard, CompactSparseAndExpandedDenseLayout)
{
	const auto Small = LucentScoreboard::Calculate(1066, 600, 1, 0, false, 0);
	const auto Large = LucentScoreboard::Calculate(1066, 600, 128, 0, false, 0);
	EXPECT_EQ(Small.m_Columns, 1);
	EXPECT_LT(Small.m_Height, Large.m_Height);
	EXPECT_LT(Small.m_Width, Large.m_Width);
	EXPECT_GT(Large.m_Columns, 1);
	const auto Spectators = LucentScoreboard::Calculate(1066, 600, 0, 0, false, 128);
	EXPECT_GT(Spectators.m_SpectatorColumns, 2);
}

TEST(LucentScoreboard, LongTextFitsWithoutRemovingGlyphsAndScaleIsResolutionIndependent)
{
	for(float Width : {20.f, 60.f, 120.f})
	{
		const float Font = LucentScoreboard::FitFont(12, 500, Width);
		EXPECT_GT(Font, 0);
		EXPECT_LE(500 * Font / 12, Width + .001f);
	}
	for(int Pixels : {480, 720, 1080, 1440, 2160})
	{
		const float Aspect = 16.f / 9;
		const auto L = LucentScoreboard::Calculate(600 * Aspect, 600, 128, 0, false, 0);
		EXPECT_LE(L.m_Width * Pixels / 600, Pixels * Aspect);
		EXPECT_LE(L.m_Height * Pixels / 600, Pixels);
	}
}

TEST(LucentScoreboard, ExtentsRetargetContinuouslyAndVisibilityFinishesOnTime)
{
	LucentScoreboard::CExtent Extent;
	EXPECT_FLOAT_EQ(Extent.Update(400, 0), 400);
	EXPECT_FLOAT_EQ(Extent.Update(800, 1), 400);
	const float Mid = Extent.Update(800, 1.08);
	EXPECT_GT(Mid, 400);
	EXPECT_LT(Mid, 800);
	EXPECT_FLOAT_EQ(Extent.Update(500, 1.08), Mid);
	EXPECT_NEAR(Extent.Update(500, 1.24), 500, .001f);
	LucentStyle::Motion::CTransition Visibility;
	EXPECT_FLOAT_EQ(Visibility.Update(true, 0, true, .17f), 0);
	EXPECT_GT(Visibility.Update(true, .08, true, .17f), 0);
	EXPECT_NEAR(Visibility.Update(true, .17, true, .17f), 1, .0001f);
	EXPECT_FLOAT_EQ(Visibility.Update(false, .2, true, .11f), 1);
	EXPECT_NEAR(Visibility.Update(false, .31, true, .11f), 0, .0001f);
	EXPECT_EQ(LucentStyle::Motion::CubicOut(.08f / .08f), 1);
	EXPECT_FLOAT_EQ(LucentStyle::Motion::Lerp(.98f, 1.f, 0), .98f);
	EXPECT_FLOAT_EQ(LucentStyle::Motion::Lerp(.98f, 1.f, 1), 1);
}

TEST(LucentScoreboard, EveryClientAppearsExactlyOnceIncludingEmptyAndUnbalancedTeams)
{
	for(int Count = 0; Count <= 128; ++Count)
		for(int Columns = 1; Columns <= 4; ++Columns)
		{
			std::array<int, 128> Visits{};
			for(int Column = 0; Column < Columns; ++Column)
			{
				const auto Range = LucentScoreboard::ColumnRange(Count, Column, Columns);
				ASSERT_LE(Range.m_Start, Range.m_End);
				ASSERT_LE(Range.m_End, Count);
				for(int I = Range.m_Start; I < Range.m_End; ++I)
					++Visits[I];
			}
			for(int I = 0; I < 128; ++I)
				EXPECT_EQ(Visits[I], I < Count ? 1 : 0);
		}
}
TEST(LucentScoreboard, CachedLayoutAndCoverageInvalidateOnContentAndFramebufferChanges)
{
	LucentScoreboard::CPlanner Planner;
	LucentScoreboard::CInput Input{1066, 600, 1, 0, false, 0, 400};
	const auto First = Planner.Update(Input);
	EXPECT_EQ(Planner.Update(Input).m_Width, First.m_Width);
	Input.m_Spectators = 1;
	EXPECT_GT(Planner.Update(Input).m_SpectatorHeight, 0);
	Input.m_Spectators = 0;
	EXPECT_EQ(Planner.Update(Input).m_SpectatorHeight, 0);
	Input.m_Red = 128;
	EXPECT_GT(Planner.Update(Input).m_Columns, First.m_Columns);
	LucentScoreboard::CPanelCache Cache;
	const LucentHud::CRect Rect{30, 60, 400, 200};
	const auto Initial = Cache.Get(Rect, {{0, 0}, {1, 1}});
	EXPECT_GT(Initial.m_Count, 0);
	EXPECT_EQ(Cache.Get(Rect, {{0, 0}, {1, 1}}).m_Feather, Initial.m_Feather);
	EXPECT_LT(Cache.Get(Rect, {{0, 0}, {.5f, .5f}}).m_Feather, Initial.m_Feather);
	EXPECT_EQ(Cache.Get({30, 60, 500, 200}, {{0, 0}, {.5f, .5f}}).m_Bounds.w, 500);
}

TEST(LucentScoreboard, IndependentHeaderStaysCenteredAndContentSizedForEveryGrid)
{
	for(float Viewport : {450.f, 800.f, 1066.f, 1600.f})
	{
		const float Center = Viewport / 2;
		const auto Header = LucentScoreboard::HeaderBounds(Center, 60, Viewport, 90, 50);
		EXPECT_FLOAT_EQ(Header.w, 2 * LucentScoreboard::HeaderPadding + 90 + LucentScoreboard::HeaderContentGap + 50);
		EXPECT_FLOAT_EQ(Header.x + Header.w / 2, Center);
		for(int Count : {0, 1, 2, 16, 32, 64, 128})
			for(bool Teams : {false, true})
			{
				const auto L = LucentScoreboard::Calculate(Viewport, 600, Teams ? Count / 2 : Count, Teams ? Count - Count / 2 : 0, Teams, 0);
				const auto SameHeader = LucentScoreboard::HeaderBounds(Center, 60, Viewport, 90, 50);
				EXPECT_FLOAT_EQ(SameHeader.w, Header.w);
				EXPECT_FLOAT_EQ(SameHeader.x, Header.x);
				for(int C = 0; C < L.m_Columns && (Count > 0 || Teams); ++C)
				{
					const auto Bounds = LucentScoreboard::ColumnBounds(L, (Viewport - L.m_Width) / 2, 60, C, L.m_Rows);
					EXPECT_FLOAT_EQ(Bounds.y - (Header.y + Header.h), LucentScoreboard::HeaderGap);
					EXPECT_LE(Bounds.y + Bounds.h, 60 + L.m_Height + .001f);
					if(C > 0)
					{
						const auto Previous = LucentScoreboard::ColumnBounds(L, (Viewport - L.m_Width) / 2, 60, C - 1, L.m_Rows);
						EXPECT_NEAR(Bounds.x - Previous.x - Previous.w, L.m_Gap, .001f);
					}
				}
			}
		const auto Long = LucentScoreboard::HeaderBounds(Center, 60, Viewport, 3000, 50);
		EXPECT_LE(Long.w, LucentScoreboard::HeaderMaxWidth);
		EXPECT_LE(Long.w, Viewport * .75f);
		EXPECT_FLOAT_EQ(Long.x + Long.w / 2, Center);
		EXPECT_GT(Long.w - 2 * LucentScoreboard::HeaderPadding - LucentScoreboard::HeaderContentGap - 50, 0);
	}
}

TEST(LucentScoreboard, NoSpectatorSpaceOrContainerAtZeroEvenDuringCollapse)
{
	const auto Many = LucentScoreboard::Calculate(1066, 600, 32, 0, false, 96);
	EXPECT_GT(LucentScoreboard::SpectatorSpace(Many, 96), 0);
	// Count takes precedence over a previous animated extent or stale layout.
	EXPECT_EQ(LucentScoreboard::SpectatorSpace(Many, 0), 0);
	const auto None = LucentScoreboard::Calculate(1066, 600, 32, 0, false, 0);
	EXPECT_EQ(None.m_SpectatorRows, 0);
	EXPECT_EQ(None.m_SpectatorHeight, 0);
	EXPECT_EQ(LucentScoreboard::SpectatorSpace(None, 0), 0);
}

TEST(LucentScoreboard, IndependentPanelsRetargetAndFadeWithoutChangingHeaderWidth)
{
	LucentScoreboard::CPanelMotion Panel;
	const auto Header = LucentScoreboard::HeaderBounds(533, 60, 1066, 80, 50);
	const auto First = Panel.Update({333, 104, 400, 96}, 0, true);
	EXPECT_FLOAT_EQ(First.x, 333);
	Panel.Update({333, 104, 400, 96}, .16, true);
	EXPECT_NEAR(Panel.m_Visibility.m_Value, 1, .0001f);
	const auto Start = Panel.Update({16, 104, 338, 400}, 1, true);
	EXPECT_FLOAT_EQ(Start.x, First.x);
	const auto Mid = Panel.Update({16, 104, 338, 400}, 1.08, true);
	EXPECT_LT(Mid.x, Start.x);
	EXPECT_GT(Mid.x, 16);
	const auto Reverse = Panel.Update({333, 104, 400, 96}, 1.08, true);
	EXPECT_FLOAT_EQ(Reverse.x, Mid.x);
	const auto Hidden = Panel.Update({}, 1.1, false);
	const auto Reopened = Panel.Update({16, 104, 338, 400}, 1.1, true);
	EXPECT_FLOAT_EQ(Hidden.x, Reopened.x);
	EXPECT_FLOAT_EQ(Hidden.w, Reopened.w);
	Panel.Update({}, 1.3, false);
	Panel.Update({}, 1.42, false);
	EXPECT_NEAR(Panel.m_Visibility.m_Value, 0, .0001f);
	EXPECT_FLOAT_EQ(Header.w, LucentScoreboard::HeaderBounds(533, 60, 1066, 80, 50).w);
}

TEST(LucentScoreboard, HeaderClockKeepsRoundCountdownAndRaceTimeSemantics)
{
	EXPECT_EQ(LucentScoreboard::RoundTime(5000, 50, 2000, 0, 0, false, false), 60);
	EXPECT_EQ(LucentScoreboard::RoundTime(5000, 50, 2000, 5, 0, false, false), 240);
	EXPECT_EQ(LucentScoreboard::RoundTime(5000, 50, 2000, 5, 0, false, true), 0);
	EXPECT_EQ(LucentScoreboard::RoundTime(5000, 50, 2000, 0, -4000, true, false), 20);
	EXPECT_EQ(LucentScoreboard::RoundTime(100, 50, 2000, 0, 0, false, false), 0);
	EXPECT_EQ(LucentScoreboard::RoundTime(10000, 50, 0, 1, 0, false, false), 0);
}
