#include <base/logger.h>
#include <base/str.h>

#include <engine/shared/ringbuffer.h>

#include <game/client/lucent/console/copy_feedback.h>
#include <game/client/lucent/console/filters.h>
#include <game/client/lucent/console/layout.h>
#include <game/client/lucent/console/log.h>
#include <game/client/lucent/console/selection.h>
#include <game/client/lucent/motion/transition.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>

namespace
{
	int Characters(const char *pText)
	{
		return str_utf8_offset_bytes_to_chars(pText, str_length(pText));
	}
	std::string Copy(const CLucentConsoleSelection &Selection)
	{
		std::string Result;
		bool First = true;
		Selection.Append(Result, First, 10, "first", 5);
		Selection.Append(Result, First, 11, "Привет 🌍\nмир", Characters("Привет 🌍\nмир"));
		Selection.Append(Result, First, 12, "", 0);
		Selection.Append(Result, First, 13, "last", 4);
		Selection.Append(Result, First, 14, "new arrival", 11);
		return Result;
	}
}

TEST(LucentConsole, UnicodeCopyAcrossOffscreenEntries)
{
	CLucentConsoleSelection Selection;
	Selection.Begin({10, 2});
	Selection.m_Focus = {13, 2};
	EXPECT_EQ(Copy(Selection), "rst\nПривет 🌍\nмир\n\nla");
	std::swap(Selection.m_Anchor, Selection.m_Focus);
	EXPECT_EQ(Copy(Selection), "rst\nПривет 🌍\nмир\n\nla");
	// No viewport or incoming-message index participates in selection identity.
	EXPECT_EQ(Selection.Range(14, 11), std::make_pair(0, 0));
	Selection.Begin({11, 7});
	Selection.m_Focus = {11, 8};
	EXPECT_EQ(Copy(Selection), "🌍");
}

TEST(LucentConsole, ReplacementAndClampedEndpoints)
{
	CLucentConsoleSelection Selection;
	Selection.Begin({10, 0});
	Selection.m_Focus = {13, 4};
	Selection.Begin({11, 0});
	EXPECT_FALSE(Selection.HasSelection());
	Selection.m_Focus = {11, 10000};
	EXPECT_EQ(Copy(Selection), "Привет 🌍\nмир");
	Selection.Clear();
	EXPECT_EQ(Copy(Selection), "");
}

TEST(LucentConsole, RawMetadataSurvivesPresentationAndCopy)
{
	const char *pRaw = "2026-10-26 09:10:53 T menus/browser: Loaded community icon 'away'";
	CLucentConsoleSelection Selection;
	Selection.Begin({1, 0});
	Selection.m_Focus = {1, Characters(pRaw)};
	std::string Result;
	bool First = true;
	Selection.Append(Result, First, 1, pRaw, Characters(pRaw));
	EXPECT_EQ(Result, pRaw);
	// The same model also copies unstructured legacy/RCON text, without fabricated metadata.
	Selection.Begin({2, 0});
	Selection.m_Focus = {2, 6};
	Result.clear();
	First = true;
	Selection.Append(Result, First, 2, "Сервер", Characters("Сервер"));
	EXPECT_EQ(Result, "Сервер");
}

TEST(LucentConsole, LongMultilineCopy)
{
	const std::string Text = std::string(12000, 'x') + "\nконец";
	CLucentConsoleSelection Selection;
	Selection.Begin({1, 11998});
	Selection.m_Focus = {1, Characters(Text.c_str())};
	std::string Result;
	bool First = true;
	Selection.Append(Result, First, 1, Text.c_str(), Characters(Text.c_str()));
	EXPECT_EQ(Result, "xx\nконец");
}

TEST(LucentConsole, RingRecyclingInvalidatesOnlyAffectedSelection)
{
	struct CEntry
	{
		uint64_t m_Id;
		char m_aText[96];
	};
	CStaticRingBuffer<CEntry, 1024, CRingBufferBase::FLAG_RECYCLE> Ring;
	CLucentConsoleSelection Selection;
	Ring.SetPopCallback([&](CEntry *pEntry) { Selection.Evict(pEntry->m_Id); });
	for(uint64_t Id = 1; Id <= 4; ++Id)
		Ring.Allocate(sizeof(CEntry))->m_Id = Id;
	Selection.Begin({3, 1});
	Selection.m_Focus = {4, 2};
	Ring.PopFirst();
	EXPECT_TRUE(Selection.HasSelection());
	Ring.PopFirst();
	EXPECT_TRUE(Selection.HasSelection());
	for(uint64_t Id = 5; Id < 40; ++Id)
		Ring.Allocate(sizeof(CEntry))->m_Id = Id;
	EXPECT_FALSE(Selection.HasSelection());
	Selection.Begin({39, 0});
	Selection.m_Focus = {39, 3};
	EXPECT_TRUE(Selection.HasSelection());
}

TEST(LucentConsole, SourceGroupsUseMetadataAndRespectSpecificSources)
{
	EXPECT_EQ(ConsoleLogGroup("chat/server"), EConsoleLogGroup::SERVER);
	EXPECT_EQ(ConsoleLogGroup("chat/whisper"), EConsoleLogGroup::CHAT);
	EXPECT_EQ(ConsoleLogGroup("gfx/opengl/shader"), EConsoleLogGroup::GRAPHICS);
	EXPECT_EQ(ConsoleLogGroup("client/network"), EConsoleLogGroup::NETWORK);
	EXPECT_EQ(ConsoleLogGroup("stun/6"), EConsoleLogGroup::NETWORK);
	EXPECT_EQ(ConsoleLogGroup("menus/browser"), EConsoleLogGroup::BROWSER);
	EXPECT_EQ(ConsoleLogGroup("unregistered runtime source"), EConsoleLogGroup::OTHER);
	EXPECT_EQ(ConsoleLogGroup("notgfx/opengl"), EConsoleLogGroup::OTHER);
}

TEST(LucentConsole, CopyButtonFitsViewportAtDifferentResolutions)
{
	for(float Width : {160.f, 320.f, 800.f, 1920.f})
		for(float Height : {24.f, 100.f, 400.f})
			for(float X : {0.f, Width / 2, Width})
				for(float Y : {0.f, Height / 2, Height})
				{
					const auto [ButtonX, ButtonY] = ConsoleCopyPosition(Width, Height, X, Y, 16, 4);
					EXPECT_GE(ButtonX, 0);
					EXPECT_GE(ButtonY, 0);
					EXPECT_LE(ButtonX + 16, Width);
					EXPECT_LE(ButtonY + 16, Height);
				}
	EXPECT_EQ(ConsoleCopyPosition(320, 100, 319, 40, 16, 4), std::make_pair(299.f, 24.f));
}

TEST(LucentConsole, CategorySeverityIntersectionAndEmptySelection)
{
	using namespace LucentConsole;
	CFilters Filters;
	for(int Group = 0; Group < int(EConsoleLogGroup::COUNT); ++Group)
		for(int Level = -1; Level <= 4; ++Level)
			EXPECT_TRUE(Filters.Allows(EConsoleLogGroup(Group), Level));
	// Start empty, then independently include two categories and exclude Info.
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::CHAT, 2));
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Filters.ToggleCategory(EConsoleLogGroup::GRAPHICS);
	Filters.ToggleSeverity(2);
	EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::CHAT, 0));
	EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::GRAPHICS, 1));
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::CHAT, 2));
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::NETWORK, 0));
	Filters.ResetCategories();
	EXPECT_TRUE(Filters.CategoriesUnrestricted());
	EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::NETWORK, 0));
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::NETWORK, 2)); // All resets categories only.
	Filters.ToggleSeverity(-1);
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::SERVER, -1));
}

TEST(LucentConsole, AllChatServerRemoveChatAll)
{
	LucentConsole::CFilters Filters;
	Filters.ToggleSeverity(1);
	const auto Expect = [&](std::initializer_list<EConsoleLogGroup> Selected, bool All) {
		EXPECT_EQ(Filters.CategoriesUnrestricted(), All);
		for(int i = 0; i < int(EConsoleLogGroup::COUNT); ++i)
		{
			const auto Group = EConsoleLogGroup(i);
			const bool Allowed = All || std::find(Selected.begin(), Selected.end(), Group) != Selected.end();
			EXPECT_EQ(Filters.Category(Group), Allowed);
			EXPECT_EQ(Filters.Allows(Group, 2), Allowed);
			EXPECT_FALSE(Filters.Allows(Group, 1)); // Category actions do not reset severity.
		}
	};
	Expect({}, true);
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Expect({EConsoleLogGroup::CHAT}, false);
	Filters.ToggleCategory(EConsoleLogGroup::SERVER);
	Expect({EConsoleLogGroup::CHAT, EConsoleLogGroup::SERVER}, false);
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Expect({EConsoleLogGroup::SERVER}, false);
	Filters.ResetCategories();
	Expect({}, true);
}

TEST(LucentConsole, LastCategoryCanBeDeselectedWithoutReturningToAll)
{
	LucentConsole::CFilters Filters;
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	EXPECT_FALSE(Filters.CategoriesUnrestricted());
	for(int i = 0; i < int(EConsoleLogGroup::COUNT); ++i)
		EXPECT_FALSE(Filters.Allows(EConsoleLogGroup(i), 2));
	Filters.ToggleCategory(EConsoleLogGroup::SERVER);
	EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::SERVER, 2));
	EXPECT_FALSE(Filters.Allows(EConsoleLogGroup::CHAT, 2));
	Filters.ResetCategories();
	EXPECT_TRUE(Filters.CategoriesUnrestricted());
}

TEST(LucentConsole, MoreCategoriesUseTheSameExclusiveThenAdditiveSelection)
{
	for(auto Group : {EConsoleLogGroup::CLIENT, EConsoleLogGroup::ASSETS, EConsoleLogGroup::RECORD, EConsoleLogGroup::EDITOR, EConsoleLogGroup::OTHER})
	{
		LucentConsole::CFilters Filters;
		Filters.ToggleCategory(Group);
		for(int i = 0; i < int(EConsoleLogGroup::COUNT); ++i)
			EXPECT_EQ(Filters.Allows(EConsoleLogGroup(i), 2), EConsoleLogGroup(i) == Group);
		Filters.ToggleCategory(EConsoleLogGroup::CHAT);
		EXPECT_TRUE(Filters.Allows(Group, 2));
		EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::CHAT, 2));
		Filters.ToggleCategory(Group);
		EXPECT_FALSE(Filters.Allows(Group, 2));
		EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::CHAT, 2));
		Filters.ResetCategories();
		EXPECT_TRUE(Filters.Allows(Group, 2));
	}
}

TEST(LucentConsole, MoreIndicatorTracksSelectedAdditionalCategories)
{
	LucentConsole::CFilters Filters;
	Filters.ToggleSeverity(4);
	EXPECT_FALSE(Filters.HasAdditionalCategories());
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	EXPECT_FALSE(Filters.HasAdditionalCategories());
	LucentConsole::CPopoverState Popup;
	Popup.Toggle(LucentConsole::EPopover::MORE);
	EXPECT_FALSE(Filters.HasAdditionalCategories());
	Filters.ToggleCategory(EConsoleLogGroup::CLIENT);
	EXPECT_TRUE(Filters.HasAdditionalCategories());
	Filters.ToggleCategory(EConsoleLogGroup::CLIENT);
	EXPECT_FALSE(Filters.HasAdditionalCategories());
	Filters.ResetCategories();
	EXPECT_FALSE(Filters.HasAdditionalCategories());
	EXPECT_FALSE(Filters.Severity(4));
	for(auto Group : {EConsoleLogGroup::CLIENT, EConsoleLogGroup::ASSETS, EConsoleLogGroup::RECORD, EConsoleLogGroup::EDITOR, EConsoleLogGroup::OTHER})
	{
		Filters.ResetCategories();
		Filters.ToggleCategory(Group);
		EXPECT_TRUE(Filters.HasAdditionalCategories());
		Popup.Close();
		EXPECT_TRUE(Filters.HasAdditionalCategories());
		Filters.ToggleCategory(Group);
		EXPECT_FALSE(Filters.HasAdditionalCategories()); // Empty selection is not an extra-category selection.
	}
}

TEST(LucentConsole, EmptyHistoryDistinguishesNoBacklogFromFilteredMessages)
{
	struct CEntry
	{
		int m_LineCount;
		EConsoleLogGroup m_Group;
		int m_Severity;
	};
	CStaticRingBuffer<CEntry, 1024, CRingBufferBase::FLAG_RECYCLE> Backlog;
	LucentConsole::CVisibleEntries<CEntry> View;
	LucentConsole::CFilters Filters;
	const auto State = [&]() {
		View.Update(Backlog, [&](const auto &Entry) { return Filters.Allows(Entry.m_Group, Entry.m_Severity); });
		return LucentConsole::EmptyHistory(Backlog.First() != nullptr, !View.Entries().empty());
	};
	const auto Add = [&](EConsoleLogGroup Group) {
		View.Invalidate();
		*Backlog.Allocate(sizeof(CEntry)) = {1, Group, 2};
	};
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::BACKLOG);
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Add(EConsoleLogGroup::GRAPHICS);
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::FILTERED);
	Add(EConsoleLogGroup::CHAT);
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::NONE); // Matching arrivals automatically restore the normal view.
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	View.Invalidate();
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::FILTERED);
	Filters.ResetCategories();
	View.Invalidate();
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::NONE);
	Filters.ToggleSeverity(2);
	View.Invalidate();
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::FILTERED);
	Filters.ToggleSeverity(2);
	View.Invalidate();
	EXPECT_EQ(State(), LucentConsole::EEmptyHistory::NONE);
}

TEST(LucentConsole, DisplayFilterDoesNotEnableCollection)
{
	LucentConsole::CFilters Filters;
	CLogFilter Collection;
	CLogMessage Message{};
	Message.m_Level = LEVEL_DEBUG;
	EXPECT_TRUE(Filters.Allows(EConsoleLogGroup::CLIENT, LEVEL_DEBUG));
	EXPECT_TRUE(Collection.Filters(&Message));
	Filters.ToggleSeverity(LEVEL_DEBUG);
	Filters.ToggleSeverity(LEVEL_DEBUG);
	EXPECT_EQ(Collection.m_MaxLevel.load(), LEVEL_INFO);
	Message.m_Level = LEVEL_TRACE;
	EXPECT_TRUE(Collection.Filters(&Message));
	Collection.m_MaxLevel = LEVEL_TRACE; // Only an explicit collection change admits future Trace.
	EXPECT_FALSE(Collection.Filters(&Message));
}

TEST(LucentConsole, AdditionalAuditedCategories)
{
	EXPECT_EQ(ConsoleLogGroup("client"), EConsoleLogGroup::CLIENT);
	EXPECT_EQ(ConsoleLogGroup("console"), EConsoleLogGroup::CLIENT);
	EXPECT_EQ(ConsoleLogGroup("skins7"), EConsoleLogGroup::ASSETS);
	EXPECT_EQ(ConsoleLogGroup("sound/opus"), EConsoleLogGroup::ASSETS);
	EXPECT_EQ(ConsoleLogGroup("map/load"), EConsoleLogGroup::ASSETS);
	EXPECT_EQ(ConsoleLogGroup("videorecorder/libav"), EConsoleLogGroup::RECORD);
	EXPECT_EQ(ConsoleLogGroup("editor/automap"), EConsoleLogGroup::EDITOR);
	EXPECT_EQ(ConsoleLogGroup("my gfx error"), EConsoleLogGroup::OTHER);
}

TEST(LucentConsole, VisibleCacheSelectionAndRingEviction)
{
	struct CEntry
	{
		uint64_t m_Id;
		int m_LineCount;
		EConsoleLogGroup m_Group;
		int m_Severity;
		char m_aText[96];
	};
	CStaticRingBuffer<CEntry, 4096, CRingBufferBase::FLAG_RECYCLE> Backlog;
	LucentConsole::CVisibleEntries<CEntry> View;
	LucentConsole::CFilters Filters;
	CLucentConsoleSelection Selection;
	Backlog.SetPopCallback([&](CEntry *pEntry) { View.Invalidate(); Selection.Evict(pEntry->m_Id); });
	const auto Add = [&](uint64_t Id, EConsoleLogGroup Group, int Severity, const char *pText, int Lines) {
		View.Invalidate();
		auto *pEntry = Backlog.Allocate(sizeof(CEntry));
		pEntry->m_Id = Id;
		pEntry->m_Group = Group;
		pEntry->m_Severity = Severity;
		pEntry->m_LineCount = Lines;
		str_copy(pEntry->m_aText, pText);
	};
	const auto Update = [&]() { View.Update(Backlog, [&](const auto &Entry) { return Filters.Allows(Entry.m_Group, Entry.m_Severity); }); };
	const auto CopyBacklog = [&]() {
		std::string Result;
		bool First = true;
		for(auto *pEntry = Backlog.First(); pEntry; pEntry = Backlog.Next(pEntry))
			Selection.Append(Result, First, pEntry->m_Id, pEntry->m_aText, Characters(pEntry->m_aText));
		return Result;
	};
	Add(1, EConsoleLogGroup::CHAT, 2, "Привет", 1);
	Add(2, EConsoleLogGroup::GRAPHICS, 1, "длинная\nстрока", 2);
	Add(3, EConsoleLogGroup::SERVER, -1, "RCON", 1);
	Selection.Begin({1, 0});
	Selection.m_Focus = {3, 4};
	const auto Before = CopyBacklog();
	Update();
	EXPECT_EQ(View.Lines(), 4);
	const unsigned Rebuilds = View.Rebuilds();
	for(int Frame = 0; Frame < 1000; ++Frame)
		Update();
	EXPECT_EQ(View.Rebuilds(), Rebuilds); // No per-frame history rebuild.
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Filters.ToggleCategory(EConsoleLogGroup::SERVER);
	View.Invalidate();
	Update();
	ASSERT_EQ(View.Entries().size(), 2);
	EXPECT_EQ(View.Entries()[0]->m_Id, 1);
	EXPECT_EQ(View.Entries()[1]->m_Id, 3);
	EXPECT_EQ(View.Lines(), 2);
	// The search traversal sees the same view as rendering and scrolling.
	int Matches = 0;
	for(auto *pEntry : View.Entries())
		Matches += str_utf8_find_nocase(pEntry->m_aText, "строка") != nullptr;
	EXPECT_EQ(Matches, 0);
	EXPECT_EQ(CopyBacklog(), Before); // Temporarily hidden text remains part of the original range.
	Filters.ToggleSeverity(-1);
	View.Invalidate();
	Update();
	ASSERT_EQ(View.Entries().size(), 1);
	EXPECT_EQ(View.Entries()[0]->m_Id, 1);
	Filters.ToggleCategory(EConsoleLogGroup::GRAPHICS);
	Filters.ToggleSeverity(-1);
	View.Invalidate();
	Update();
	ASSERT_EQ(View.Entries().size(), 3);
	EXPECT_EQ(View.Entries()[1]->m_Id, 2);
	EXPECT_EQ(CopyBacklog(), Before);
	Matches = 0;
	for(auto *pEntry : View.Entries())
		Matches += str_utf8_find_nocase(pEntry->m_aText, "СТРОКА") != nullptr;
	EXPECT_EQ(Matches, 1);
	for(int Group = 0; Group < int(EConsoleLogGroup::COUNT); ++Group)
		if(Filters.Category(EConsoleLogGroup(Group)))
			Filters.ToggleCategory(EConsoleLogGroup(Group));
	View.Invalidate();
	Update();
	EXPECT_TRUE(View.Entries().empty());
	EXPECT_EQ(View.Lines(), 0);
	EXPECT_EQ(CopyBacklog(), Before);
	Filters.ResetCategories();
	View.Invalidate();
	Update();
	ASSERT_EQ(View.Entries().size(), 3);
	for(uint64_t Id = 4; Id < 80; ++Id)
		Add(Id, EConsoleLogGroup::CLIENT, 2, "new arrival", 1);
	Update();
	EXPECT_FALSE(Selection.HasSelection());
	for(auto *pEntry : View.Entries())
		EXPECT_GT(pEntry->m_Id, 3);
}

TEST(LucentConsole, PillOffsetsSelectTextPrecisely)
{
	EXPECT_EQ(LucentConsole::PillSelection(2, 4, 0, 19), std::make_pair(2, 4));
	EXPECT_EQ(LucentConsole::PillSelection(24, 27, 23, 12), std::make_pair(1, 4));
	EXPECT_EQ(LucentConsole::PillSelection(0, 200, 23, 5), std::make_pair(0, 5)); // Truncated label.
	EXPECT_EQ(LucentConsole::PillSelection(40, 45, 23, 5), std::make_pair(5, 5)); // Hidden suffix.
}

TEST(LucentConsole, PanelAndCopyGeometryAtRequestedResolutions)
{
	for(const auto [PixelsX, PixelsY] : {std::pair{1920.f, 1080.f}, std::pair{2560.f, 1440.f}})
		for(float Scale : {1.f, 1.25f, 1.5f, 2.f})
		{
			const float Width = PixelsX / PixelsY * 600 / Scale;
			for(bool More : {false, true})
				for(bool Levels : {false, true})
				{
					const float Height = LucentConsole::PanelHeight(Width, More, Levels);
					EXPECT_GE(Height, LucentStyle::Console::FilterHeight);
					EXPECT_EQ(Height, LucentConsole::PanelHeight(Width, false, false));
					EXPECT_LT(Height + 60, 360); // Input + panel leave a non-overlapping log viewport.
				}
			for(float EndX : {0.f, Width / 2, Width})
			{
				const float ButtonX = LucentConsole::CopyControlX(EndX, 90, Width);
				EXPECT_GE(ButtonX, 0);
				EXPECT_LE(ButtonX + 90, Width);
			}
		}
	EXPECT_TRUE(LucentConsole::CompactPanel(400));
	EXPECT_FALSE(LucentConsole::CompactPanel(1066));
}

TEST(LucentConsole, CopyFeedbackWaitsForMouseUpAndExpires)
{
	LucentConsole::CCopyFeedback Feedback;
	Feedback.BeginSelection();
	EXPECT_FALSE(Feedback.Visible(10, true, true, true)); // MouseDown + Drag.
	EXPECT_TRUE(Feedback.Visible(10, false, true, true)); // MouseUp.
	EXPECT_FALSE(Feedback.Visible(10, false, false, true)); // Range outside viewport.
	Feedback.Copied(10);
	EXPECT_TRUE(Feedback.IsCopied());
	EXPECT_TRUE(Feedback.Visible(10.9, false, false, false)); // Range is already cleared.
	EXPECT_FALSE(Feedback.Visible(11.06, false, false, false));
	LucentStyle::Motion::CTransition Fade;
	Fade.Update(true, 10, true, LucentStyle::Console::CopyFade);
	EXPECT_FLOAT_EQ(Fade.Update(true, 10.5, true, LucentStyle::Console::CopyFade), 1);
	EXPECT_FLOAT_EQ(Fade.Update(false, 11.06, true, LucentStyle::Console::CopyFade), 1);
	EXPECT_FLOAT_EQ(Fade.Update(false, 11.19, true, LucentStyle::Console::CopyFade), 0);
}

TEST(LucentConsole, NewSelectionCancelsCopiedFeedbackImmediately)
{
	LucentConsole::CCopyFeedback Feedback;
	Feedback.Copied(20);
	EXPECT_TRUE(Feedback.Visible(20.1, false, false, false));
	Feedback.BeginSelection();
	EXPECT_FALSE(Feedback.IsCopied());
	EXPECT_FALSE(Feedback.Visible(20.2, true, true, true));
	EXPECT_TRUE(Feedback.Visible(20.3, false, true, true));
	Feedback.Copied(21, false); // Ctrl+C of a filtered/offscreen range has no floating anchor.
	EXPECT_FALSE(Feedback.Visible(21.1, false, false, false));
}

TEST(LucentConsole, PopoverDismissalPreservesFiltersAndDisplayState)
{
	LucentConsole::CPopoverState Popup;
	LucentConsole::CFilters Filters;
	Filters.ToggleCategory(EConsoleLogGroup::CHAT);
	Popup.Toggle(LucentConsole::EPopover::MORE);
	EXPECT_TRUE(Popup.IsOpen());
	EXPECT_FALSE(Popup.DismissOutside(true, false)); // Multiselect remains open.
	EXPECT_FALSE(Popup.DismissOutside(false, true)); // Anchor press is handled by its toggle.
	EXPECT_TRUE(Popup.DismissOutside(false, false));
	EXPECT_FALSE(Popup.IsOpen());
	EXPECT_EQ(Popup.m_Display, LucentConsole::EPopover::MORE); // Fade can finish without input capture.
	EXPECT_FALSE(Filters.Category(EConsoleLogGroup::RECORD));
	Popup.Toggle(LucentConsole::EPopover::MORE);
	Popup.Toggle(LucentConsole::EPopover::MORE);
	EXPECT_FALSE(Popup.IsOpen());
	Popup.Toggle(LucentConsole::EPopover::MORE);
	Popup.Toggle(LucentConsole::EPopover::LEVELS);
	EXPECT_EQ(Popup.m_Open, LucentConsole::EPopover::LEVELS);
	EXPECT_TRUE(Popup.Close()); // Esc consumes just this dismissal.
	EXPECT_FALSE(Popup.Close()); // Subsequent Esc belongs to console/search.
}

TEST(LucentConsole, ContextOverlaysFitWithoutReservingHistoryHeight)
{
	for(float Width : {160.f, 320.f, 533.f, 1066.f, 1920.f, 2560.f})
		for(float Height : {40.f, 100.f, 280.f})
			for(float X : {0.f, Width / 2, Width})
				for(float Y : {24.f, Height / 2 + 24, Height + 24})
				{
					const auto Copy = LucentConsole::CopyOverlay(X, Y, 11, 100, 16, 0, 24, Width, Height);
					const auto Popup = LucentConsole::AboveAnchor(X, Height + 60, 246, 260, 0, 24, Width, Height, 6);
					for(auto Rect : {Copy, Popup})
					{
						EXPECT_GE(Rect.m_X, 0);
						EXPECT_GE(Rect.m_Y, 24);
						EXPECT_LE(Rect.m_X + Rect.m_W, Width);
						EXPECT_LE(Rect.m_Y + Rect.m_H, Height + 24); // Excludes input area.
					}
				}
	const auto Above = LucentConsole::CopyOverlay(100, 100, 11, 100, 16, 0, 24, 500, 240);
	EXPECT_FLOAT_EQ(Above.m_Y, 78); // Directly above selection, gap 6.
	const auto Below = LucentConsole::CopyOverlay(100, 24, 11, 100, 16, 0, 24, 500, 240);
	EXPECT_FLOAT_EQ(Below.m_Y, 41); // Nearest fitting alternative at the top edge.
}

TEST(LucentConsole, CopyBesidePrefersRightEdgeOfVisibleFragment)
{
	using namespace LucentConsole;
	const COverlayPosition Fragment{100, 80, 120, 11}, Bounds{0, 24, 600, 250};
	const std::array Occupied{Fragment};
	const auto Button = CopyBeside(Fragment, 100, 16, Bounds, Occupied);
	EXPECT_FLOAT_EQ(Button.m_X, 231); // Right edge + 11 UI, independent of drag direction.
	EXPECT_FLOAT_EQ(Button.m_Y, 80);
	EXPECT_FALSE(Overlaps(Button, Fragment));
}

TEST(LucentConsole, CopyBesideFallsBackLeftThenBelowWithoutOverlap)
{
	using namespace LucentConsole;
	const COverlayPosition Bounds{0, 24, 500, 250};
	const COverlayPosition RightEdge{380, 80, 110, 11};
	const auto Left = CopyBeside(RightEdge, 100, 16, Bounds, std::array{RightEdge});
	EXPECT_FLOAT_EQ(Left.m_X, 269);
	EXPECT_FALSE(Overlaps(Left, RightEdge));
	const COverlayPosition FullLine{0, 80, 500, 11};
	const auto Below = CopyBeside(FullLine, 100, 16, Bounds, std::array{FullLine});
	EXPECT_FLOAT_EQ(Below.m_Y, 102);
	const std::array Wrapped{COverlayPosition{0, 69, 500, 11}, FullLine};
	const auto WrappedBelow = CopyBeside(FullLine, 100, 16, Bounds, Wrapped);
	for(const auto &Fragment : Wrapped)
		EXPECT_FALSE(Overlaps(WrappedBelow, Fragment));
	const auto NoSpace = CopyBeside({0, 24, 500, 250}, 100, 16, Bounds, std::array{Bounds});
	EXPECT_FLOAT_EQ(NoSpace.m_W, 0); // Never cover a fully selected viewport; Ctrl+C remains available.
}

TEST(LucentConsole, CopyBesideClippingAndScaleBounds)
{
	using namespace LucentConsole;
	for(float Width : {160.f, 320.f, 533.f, 1066.f, 1920.f, 2560.f})
		for(float Scale : {1.f, 1.25f, 1.5f})
		{
			const COverlayPosition Bounds{0, 24, Width, 280};
			for(float Y : {24.f, 150.f, 293.f})
			{
				const COverlayPosition Fragment{20, Y, Width - 40, 11};
				const auto Button = CopyBeside(Fragment, 90 * Scale, 16, Bounds, std::array{Fragment});
				if(Button.m_W == 0)
					continue;
				EXPECT_GE(Button.m_X, 0);
				EXPECT_GE(Button.m_Y, 24);
				EXPECT_LE(Button.m_X + Button.m_W, Width);
				EXPECT_LE(Button.m_Y + Button.m_H, 304);
				EXPECT_FALSE(Overlaps(Button, Fragment));
			}
		}
}

TEST(LucentConsole, PopoversSizeToContentAndUseTwoColumnLevels)
{
	using namespace LucentConsole;
	using namespace LucentStyle::Console;
	EXPECT_FLOAT_EQ(PopoverContentHeight(false, false, 0), 2 * PopoverPadding + 5 * PopoverRow);
	EXPECT_GE(PopoverWidth, 190);
	EXPECT_LE(PopoverWidth, 220);
	EXPECT_GE(LevelsWidth, 250);
	EXPECT_LE(LevelsWidth, 280);
	EXPECT_FLOAT_EQ(PopoverContentHeight(true, false, 12), 2 * PopoverPadding + 2 * PopoverHeader + 3 * PopoverRow + FilterGap + PopoverRow + 12);
	EXPECT_FLOAT_EQ(PopoverContentHeight(true, false, 24) - PopoverContentHeight(true, false, 12), 12);
	EXPECT_FLOAT_EQ(PopoverContentHeight(true, false, 12) - PopoverContentHeight(true, true, 12), PopoverRow);
}
