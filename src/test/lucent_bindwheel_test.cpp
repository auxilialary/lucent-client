#include "test.h"

#include <base/io.h>

#include <engine/console.h>
#include <engine/shared/config.h>

#include <game/client/lucent/bindwheel/import.h>
#include <game/client/lucent/bindwheel/presentation.h>
#include <game/client/lucent/settings/bindwheel.h>
#include <game/client/lucent/settings/workspace.h>

#include <gtest/gtest.h>

using namespace LucentBindWheel;

TEST(LucentBindWheel, ImportSuggestsAUsableSourceAndKeepsSharedClientIdentityHonest)
{
	CSource Broken{"C:/Users/example/AppData/Roaming/DDNet/entity_bindwheel.cfg", {}};
	Broken.m_Data.m_Binds = {{"broken", "say x"}};
	Broken.m_Data.m_Errors = 1;
	CSource Shared{"C:/Users/example/AppData/Roaming/DDNet/settings_tclient.cfg", {}};
	Shared.m_Data.m_Binds = {{"working", "say hello"}};
	EXPECT_EQ(SuggestedSource({}), -1);
	EXPECT_EQ(SuggestedSource({Broken, Shared}), 1);
	EXPECT_EQ(SuggestedSource({Shared, Broken}), 0);
	EXPECT_STREQ(SourceClient(Broken), "E Client");
	EXPECT_STREQ(SourceClient(Shared), "T / Best Client");
	EXPECT_STREQ(SourceLocation(Shared), "Saved settings");
	EXPECT_STREQ(SourceClient({"C:\\Games\\BestClient\\settings_tclient.cfg", {}}), "Best Client");
	EXPECT_STREQ(SourceClient({"C:/Games/TClient/settings_tclient.cfg", {}}), "T Client");
	EXPECT_STREQ(SourceLocation({"C:/Games/TClient/settings_tclient.cfg", {}}), "Client folder");
}

TEST(LucentBindWheel, AutoDiscoveryReadsAndSuggestsWheelWithoutAManualFilePath)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	for(const char *pName : {"entity_bindwheel.cfg", "settings_tclient.cfg"})
	{
		IOHANDLE File = Storage->OpenFile(pName, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		const std::string Text = "add_bindwheel hello \"say hello\"\n";
		io_write(File, Text.data(), unsigned(Text.size()));
		io_close(File);
	}
	char aRoot[IO_MAX_PATH_LENGTH];
	Storage->GetCompletePath(IStorage::TYPE_SAVE, "", aRoot, sizeof(aRoot));
	const auto Found = DiscoverInRoots({aRoot, std::string(aRoot) + "/."});
	ASSERT_EQ(Found.size(), 2u); // aliases of the same files are not extra wheels
	const int Suggested = SuggestedSource(Found);
	ASSERT_GE(Suggested, 0);
	EXPECT_EQ(Found[Suggested].m_Data.m_Binds, (CPage{{"hello", "say hello"}}));
	EXPECT_STREQ(SourceClient(Found[Suggested]), "E Client");
}

TEST(LucentBindWheel, ImportDialogFitsWithCollapsedOrExpandedOptions)
{
	for(float Width : {360.f, 480.f, 620.f})
		for(float Height : {260.f, 360.f, 412.f, 500.f})
			for(bool Options : {false, true})
			{
				const CUIRect View{14, 14, Width, Height};
				const auto L = LucentSettings::CBindWheelImportLayout::Calculate(View, Options);
				for(const auto &Rect : {L.m_Header, L.m_Target, L.m_Sources, L.m_Review, L.m_Options, L.m_Secondary, L.m_Notice, L.m_Footer})
				{
					EXPECT_GE(Rect.w, 0);
					EXPECT_GE(Rect.h, 0);
					EXPECT_GE(Rect.x, View.x);
					EXPECT_GE(Rect.y, View.y);
					EXPECT_LE(Rect.x + Rect.w, View.x + View.w + .001f);
					EXPECT_LE(Rect.y + Rect.h, View.y + View.h + .001f);
				}
				EXPECT_LT(L.m_Sources.x + L.m_Sources.w, L.m_Review.x);
				EXPECT_LE(L.m_Review.y + L.m_Review.h, L.m_Secondary.y + .001f);
			}
}

TEST(LucentBindWheel, DragReorderPreservesIntermediateActionsAndRejectsInvalidDrops)
{
	CPage Page{{"a", "say a"}, {"b", "say b"}, {"c", "say c"}, {"d", "say d"}};
	ASSERT_TRUE(Reorder(Page, 0, 3));
	EXPECT_EQ(Page, (CPage{{"b", "say b"}, {"c", "say c"}, {"d", "say d"}, {"a", "say a"}}));
	ASSERT_TRUE(Reorder(Page, 3, 1));
	EXPECT_EQ(Page, (CPage{{"b", "say b"}, {"a", "say a"}, {"c", "say c"}, {"d", "say d"}}));
	const auto Before = Page;
	EXPECT_FALSE(Reorder(Page, -1, 2));
	EXPECT_FALSE(Reorder(Page, 1, 4));
	EXPECT_FALSE(Reorder(Page, 1, 1));
	EXPECT_EQ(Page, Before);
}

TEST(LucentBindWheel, SharedPreviewAndGameFlipReturnsAndSupportsDisabledAnimation)
{
	CFlipPresentation Flip;
	EXPECT_EQ(Flip.Update(0, 0, true).m_Page, 0);
	Flip.Update(1, 1, true);
	const auto Mid = Flip.Update(1, 1.03, true);
	EXPECT_GT(Mid.m_Width, 0);
	EXPECT_LT(Mid.m_Width, 1);
	const auto Return = Flip.Update(0, 1.03, true);
	EXPECT_FLOAT_EQ(Return.m_Width, Mid.m_Width);
	const auto Front = Flip.Update(0, 2, true);
	EXPECT_EQ(Front.m_Page, 0);
	EXPECT_FLOAT_EQ(Front.m_Width, 1);
	const auto Back = Flip.Update(1, 3, false);
	EXPECT_EQ(Back.m_Page, 1);
	EXPECT_FLOAT_EQ(Back.m_Width, 1);
	EXPECT_EQ(Flip.Update(0, 4, false).m_Page, 0);
}

TEST(LucentBindWheel, SettingsFitTheViewportWithoutPageScrolling)
{
	for(float Width : {320.f, 540.f, 800.f, 1040.f, 1400.f})
		for(float Height : {280.f, 360.f, 440.f, 560.f, 720.f})
		{
			const CUIRect View{23, 19, Width, Height};
			const auto L = LucentSettings::CBindWheelLayout::Calculate(View);
			for(const auto &Rect : {L.m_Title, L.m_Tabs, L.m_Editor, L.m_Preview, L.m_Controls, L.m_Footer})
			{
				EXPECT_GE(Rect.w, 0);
				EXPECT_GE(Rect.h, 0);
				EXPECT_GE(Rect.x, View.x);
				EXPECT_GE(Rect.y, View.y);
				EXPECT_LE(Rect.x + Rect.w, View.x + View.w + .001f);
				EXPECT_LE(Rect.y + Rect.h, View.y + View.h + .001f);
			}
			EXPECT_LT(L.m_Editor.x + L.m_Editor.w, L.m_Preview.x);
			EXPECT_LE(L.m_Preview.y + L.m_Preview.h, L.m_Controls.y);
			EXPECT_LE(L.m_Controls.y + L.m_Controls.h, L.m_Footer.y + .001f);
		}
}

TEST(LucentBindWheel, SavedEntriesSurviveTheRealConsoleParser)
{
	struct CCapture
	{
		int m_Page = -1, m_Calls = 0;
		CBind m_Bind;
		static void Read(IConsole::IResult *pResult, void *pUser)
		{
			auto &Capture = *static_cast<CCapture *>(pUser);
			Capture.m_Page = pResult->GetInteger(0);
			Capture.m_Bind = {pResult->GetString(1), pResult->GetString(2)};
			++Capture.m_Calls;
		}
	} Capture;
	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	pConsole->Register("lucent_add_bindwheel", "i[page] s[name] r[command]", CFGFLAG_CLIENT, CCapture::Read, &Capture, "");
	pConsole->StoreCommands(false);
	for(const CBind Bind : {CBind{"say # hi", "say \"hello\"; exec \\\"path\\\""}, CBind{std::string(63, '"'), std::string(1023, '\\')}, CBind{"Пусто", ""}})
		for(int Page = 0; Page < 2; ++Page)
		{
			const auto Line = SaveLine(Page, Bind);
			ASSERT_TRUE(pConsole->LineIsValid(Line.c_str()));
			const int Before = Capture.m_Calls;
			pConsole->ExecuteLine(Line.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
			EXPECT_EQ(Capture.m_Calls, Before + 1);
			EXPECT_EQ(Capture.m_Page, Page);
			EXPECT_EQ(Capture.m_Bind, Bind);
		}
}

TEST(LucentBindWheel, ImportsAllClientFormatsWithoutExecutingPayloads)
{
	for(const char *pFormat : {"ec_reset_bindwheel_mouse 1\nec_animate_wheel_time 150\n", "tc_reset_bindwheel_mouse 1\ntc_animate_wheel_time 150\n", "tc_reset_bindwheel_mouse 1\ntc_animate_wheel_time 150\nbc_wheel_scale 75\n"})
	{
		CImport Data;
		Parse(std::string(pFormat) + "bind q +bindwheel\nexec malicious.cfg\nplayer_name untouched\nadd_bindwheel \"Привет\" \"say \\\"hi\\\"; exec some.cfg\"\n", Data);
		ASSERT_EQ(Data.m_Binds.size(), 1u);
		EXPECT_EQ(Data.m_Binds[0].m_Name, "Привет");
		EXPECT_EQ(Data.m_Binds[0].m_Command, "say \"hi\"; exec some.cfg");
		EXPECT_EQ(Data.m_Keys, std::vector<std::string>{"q"});
		EXPECT_EQ(Data.m_ResetMouse, 1);
		EXPECT_EQ(Data.m_Animation, 150);
		EXPECT_EQ(Data.m_Errors, 0);
	}
}

TEST(LucentBindWheel, PreservesSparseOrderDuplicatesAndQuotedRoundTrips)
{
	CImport Data;
	Parse("bindwheel 3 \"last\" \"say last\"\nadd_bindwheel same \"say x\"\nadd_bindwheel same \"say x\"\n", Data);
	ASSERT_EQ(Data.m_Binds.size(), 6u);
	EXPECT_EQ(Data.m_Binds[0], (CBind{"*", ""}));
	EXPECT_EQ(Data.m_Binds[3].m_Name, "last");
	EXPECT_EQ(Data.m_Binds[4], Data.m_Binds[5]);
	const CBind Original{std::string(63, '"'), std::string(1023, '\\')};
	CImport RoundTrip;
	Parse("add_bindwheel " + Quote(Original.m_Name) + " " + Quote(Original.m_Command), RoundTrip);
	ASSERT_EQ(RoundTrip.m_Binds.size(), 1u);
	EXPECT_EQ(RoundTrip.m_Binds[0], Original);
	EXPECT_EQ(RoundTrip.m_Errors, 0);
}

TEST(LucentBindWheel, RejectsInvalidAndOversizedRecords)
{
	CImport Data;
	Parse("bindwheel -1 a b\nbindwheel 64 a b\nadd_bindwheel \"unfinished\nadd_bindwheel a \"b\" extra\n", Data);
	Parse("add_bindwheel " + Quote(std::string(64, 'x')) + " b\n", Data);
	Parse("add_bindwheel a " + Quote(std::string(1024, 'x')) + "\n", Data);
	EXPECT_EQ(Data.m_Errors, 6);
	EXPECT_TRUE(Data.m_Binds.empty());
	for(int i = 0; i < 65; ++i)
		Parse("add_bindwheel a b\n", Data);
	EXPECT_EQ(Data.m_Binds.size(), 64u);
	EXPECT_EQ(Data.m_Errors, 7);
}

TEST(LucentBindWheel, HandlesBomCommentsUnquotedRemaindersAndRemoval)
{
	CImport Data;
	Parse("\xef\xbb\xbf"
	      "add_bindwheel a say hello there # ignored; add_bindwheel bad bad\nadd_bindwheel b \"say # ; still payload\"; add_bindwheel c d\nremove_bindwheel a say hello there\n",
		Data);
	ASSERT_EQ(Data.m_Binds.size(), 2u);
	EXPECT_EQ(Data.m_Binds[0].m_Command, "say # ; still payload");
	EXPECT_EQ(Data.m_Binds[1].m_Name, "c");
	Parse("delete_all_bindwheel_binds\n", Data);
	EXPECT_TRUE(Data.m_Binds.empty());
}

TEST(LucentBindWheel, ShiftSwitchesBothWaysAndReleaseUsesCurrentPageWithoutRender)
{
	std::array<CPage, 2> Pages = {CPage{{"one", "say one"}}, CPage{{"two", "say two"}}};
	CSession Session;
	Session.Open(false, true);
	Session.m_Cursor = vec2(140, 0);
	Session.Shift(true);
	EXPECT_EQ(Session.Execute(Pages, 68, true), "say two");
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
	Session.Open(true, false);
	Session.Shift(false);
	EXPECT_EQ(Session.Execute(Pages, 68, true), "say one");
}

TEST(LucentBindWheel, CancelAndCenterNeverExecuteAndHoverExecutesOnce)
{
	std::array<CPage, 2> Pages = {CPage{{"one", "say one"}}, CPage{}};
	CSession Session;
	Session.Open(false, true);
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
	Session.Open(false, true);
	Session.m_Cursor = vec2(140, 0);
	Session.Cancel(); // Escape, focus loss, menus, disconnect and mutations share cancellation.
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
	Session.Open(false, false);
	EXPECT_EQ(Session.Execute(Pages, 68, false), "say one");
	Session.Open(false, true); // Repeated key stroke must not reset the live session.
	EXPECT_TRUE(Session.Execute(Pages, 68, false).empty());
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
	Session.Open(true, false);
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
	Session.Open(false, false);
	Pages[0].clear();
	EXPECT_TRUE(Session.Execute(Pages, 68, true).empty());
}

TEST(LucentBindWheel, CancellationSurvivesKeyRepeatAndNewPhysicalPressCanReopen)
{
	CSession Session;
	ASSERT_TRUE(Session.Press(true));
	Session.Open(false, true);
	Session.Cancel();
	EXPECT_FALSE(Session.Press(false));
	EXPECT_FALSE(Session.Active());
	Session.ReleaseStroke();
	EXPECT_TRUE(Session.Press(true));
	Session.Open(false, true);
	EXPECT_TRUE(Session.Active());
	Session.Cancel();
	// Menus may have swallowed the release; CBinds identifies a fresh physical press.
	EXPECT_TRUE(Session.Press(true));
	Session.Open(false, true);
	EXPECT_TRUE(Session.Active());
}

TEST(LucentBindWheel, DirectionBoundariesAndWorkspaceId)
{
	EXPECT_EQ(Select(vec2(0, 0), 64, 68), -1);
	EXPECT_EQ(Select(vec2(68, 0), 8, 68), -1);
	EXPECT_EQ(Select(vec2(140, 0), 0, 68), -1);
	EXPECT_EQ(Select(vec2(140, 0), 8, 68), 0);
	EXPECT_EQ(Select(vec2(0, -140), 8, 68), 6);
	EXPECT_EQ(Select(vec2(-140, 0), 8, 68), 4);
	EXPECT_EQ(LucentSettings::Pages[LucentSettings::PageIndex(LucentSettings::PAGE_BINDWHEEL)].m_Id, LucentSettings::PAGE_BINDWHEEL);
}

TEST(LucentBindWheel, LoadsPortableSourceAndCompanionPreferencesWithoutWritingSource)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	char aPath[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, "entity_bindwheel.cfg", aPath, sizeof(aPath));
	const std::string Original = "add_bindwheel a \"say hello\"\n";
	IOHANDLE File = io_open(aPath, IOFLAG_WRITE);
	ASSERT_NE(File, nullptr);
	io_write(File, Original.data(), unsigned(Original.size()));
	io_close(File);
	File = pStorage->OpenFile("settings_entity.cfg", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	const std::string Extra = "ec_reset_bindwheel_mouse 1\nec_animate_wheel_time 170\nplayer_name unchanged\n";
	io_write(File, Extra.data(), unsigned(Extra.size()));
	io_close(File);
	CSource Source;
	ASSERT_TRUE(LoadSource(aPath, Source));
	const std::string Path = aPath;
	const auto Separator = Path.find_last_of("/\\");
	ASSERT_NE(Separator, std::string::npos);
	EXPECT_TRUE(SameSourceFile(Path, Path.substr(0, Separator) + "/./" + Path.substr(Separator + 1)));
	EXPECT_FALSE(SameSourceFile(Path, Path + ".missing"));
	ASSERT_EQ(Source.m_Data.m_Binds.size(), 1u);
	EXPECT_EQ(Source.m_Data.m_Animation, 170);
	EXPECT_EQ(Source.m_Data.m_ResetMouse, 1);
	CImport Again;
	ASSERT_TRUE(ReadConfig(aPath, Again));
	EXPECT_EQ(Again.m_Binds, Source.m_Data.m_Binds);
	EXPECT_FALSE(LoadSource("nonexistent-bindwheel.cfg", Source));
}

TEST(LucentBindWheel, PreviewDropTargetsExcludeTheHoleOutsideAndFlippingFace)
{
	EXPECT_EQ(PreviewHit(vec2(130, 0), 190, 1, 4), 0);
	EXPECT_EQ(PreviewHit(vec2(0, 130), 190, 1, 4), 1);
	EXPECT_EQ(PreviewHit(vec2(-130, 0), 190, 1, 4), 2);
	EXPECT_EQ(PreviewHit(vec2(0, -130), 190, 1, 4), 3);
	EXPECT_EQ(PreviewHit(vec2(79, 0), 190, 1, 4), -1);
	EXPECT_EQ(PreviewHit(vec2(184, 0), 190, 1, 4), -1);
	EXPECT_EQ(PreviewHit(vec2(130, 0), 190, .5f, 4), -1);
	EXPECT_EQ(PreviewHit(vec2(130, 0), 190, 1, 0), -1);
	EXPECT_EQ(PreviewHit(vec2(0, 0), 0, 1, 4), -1);

}

TEST(LucentBindWheel, CrossPageTransferIsAtomicAndUndoRestoresBothOrders)
{
	CPages Pages;
	Pages[0] = {{"a", "say a"}, {"b", "say b"}, {"c", "say c"}};
	Pages[1] = {{"x", "say x"}, {"y", "say y"}};
	const auto Original = Pages;
	CEditHistory History;
	ASSERT_TRUE(CanTransfer(Pages, 0, 1, 1, 1));
	History.Remember(Pages);
	ASSERT_TRUE(Transfer(Pages, 0, 1, 1, 1));
	EXPECT_EQ(Pages[0], (CPage{{"a", "say a"}, {"c", "say c"}}));
	EXPECT_EQ(Pages[1], (CPage{{"x", "say x"}, {"b", "say b"}, {"y", "say y"}}));
	ASSERT_TRUE(History.Undo(Pages));
	EXPECT_EQ(Pages, Original);
	EXPECT_FALSE(History.Undo(Pages));
	// An empty destination accepts the first bind at index zero.
	Pages[1].clear();
	ASSERT_TRUE(Transfer(Pages, 0, 2, 1, 0));
	EXPECT_EQ(Pages[1], (CPage{{"c", "say c"}}));
	ASSERT_TRUE(Transfer(Pages, 1, 0, 0, 2));
	EXPECT_EQ(Pages[0], Original[0]);
}

TEST(LucentBindWheel, RejectedCrossPageTransfersLeaveBothPagesUntouched)
{
	CPages Pages;
	Pages[0] = {{"a", "say a"}};
	Pages[1].resize(MaxBinds, {"full", "say full"});
	const auto Original = Pages;
	for(const auto &Move : {std::array<int, 4>{0, 0, 1, 0}, {0, 0, 0, 0}, {-1, 0, 1, 0}, {0, 0, 2, 0}, {0, -1, 1, 0}, {0, 1, 1, 0}, {1, 0, 0, 2}, {1, 0, 0, -1}})
	{
		EXPECT_FALSE(Transfer(Pages, Move[0], Move[1], Move[2], Move[3]));
		EXPECT_EQ(Pages, Original);
	}
}

TEST(LucentBindWheel, UndoHistoryRestoresSuccessiveEditsAndIsBounded)
{
	CPages Pages;
	CEditHistory History;
	for(int i = 0; i < 40; ++i)
	{
		History.Remember(Pages);
		Pages[0] = {{std::to_string(i), "say changed"}};
	}
	for(int i = 38; i >= 7; --i)
	{
		ASSERT_TRUE(History.Undo(Pages));
		EXPECT_EQ(Pages[0][0].m_Name, std::to_string(i));
	}
	EXPECT_FALSE(History.Undo(Pages));
	History.Remember(Pages);
	History.Clear();
	EXPECT_FALSE(History.Undo(Pages));
}
