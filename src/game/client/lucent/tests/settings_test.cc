#include <game/client/lucent/settings/general.h>
#include <game/client/lucent/settings/navigation.h>
#include <game/client/lucent/settings/player.h>
#include <game/client/lucent/settings/player_countries.h>
#include <game/client/lucent/settings/sound.h>
#include <game/client/lucent/ui/slider_geometry.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

#define CHECK(Expression) \
	do \
	{ \
		if(!(Expression)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #Expression); \
			std::abort(); \
		} \
	} while(false)

static void CheckPlayerSettings(const char *pRoot)
{
	using namespace LucentSettings;
	for(float Scale : {.75f, 1.f, 1.5f, 2.f})
		for(float Width : {0.f, 1.f, 320.f, 560.f, 800.f, 1200.f})
		{
			const CUIRect View{17, 31, Width / Scale, 600 / Scale};
			const auto Layout = CPlayerLayout::Calculate(View, [](int, float W) { return 13.f * std::ceil(180.f / W); });
			CHECK(Layout.m_Form.x >= View.x && Layout.m_Form.w >= 0);
			CHECK(Layout.m_Form.x + Layout.m_Form.w <= View.x + View.w + .001f);
			CHECK(Layout.m_Form.w <= CPlayerLayout::FormMaxWidth);
			if(View.w > CPlayerLayout::FormMaxWidth)
				CHECK(Layout.m_Form.x + Layout.m_Form.w < View.x + View.w);
			for(const auto Rect : {Layout.m_NameLabel, Layout.m_Name, Layout.m_ClanLabel, Layout.m_Clan, Layout.m_CountryLabel, Layout.m_Country, Layout.m_Cooldown})
			{
				CHECK(Rect.x == Layout.m_Form.x && Rect.w == Layout.m_Form.w);
				CHECK(Rect.h >= 0);
			}
			CHECK(Layout.m_Name.y + Layout.m_Name.h <= Layout.m_ClanLabel.y);
			CHECK(Layout.m_Clan.y + Layout.m_Clan.h <= Layout.m_CountryLabel.y);
			CHECK(Layout.m_Country.y + Layout.m_Country.h <= Layout.m_Cooldown.y);
			CHECK(Layout.m_Name.h == Layout.m_Clan.h && Layout.m_Clan.h == Layout.m_Country.h);
			if(View.w >= 320)
				CHECK(Layout.m_Height < 360);
			const auto CountryRow = CPlayerCountryRowLayout::Calculate(Layout.m_Country);
			CHECK(CountryRow.m_Flag.w <= CPlayerCountryRowLayout::FlagWidth);
			CHECK(CountryRow.m_Flag.h <= CPlayerCountryRowLayout::FlagWidth / 2);
			CHECK(std::abs(CountryRow.m_Flag.w - 2 * CountryRow.m_Flag.h) < .001f);
			CHECK(CountryRow.m_Flag.x >= Layout.m_Country.x);
			CHECK(CountryRow.m_Flag.x + CountryRow.m_Flag.w <= CountryRow.m_Name.x + .001f);
			CHECK(CountryRow.m_Name.x + CountryRow.m_Name.w <= CountryRow.m_Arrow.x + .001f);
			CHECK(CountryRow.m_Arrow.x + CountryRow.m_Arrow.w <= Layout.m_Country.x + Layout.m_Country.w + .001f);
			CHECK(CountryRow.m_Name.w >= 0 && CountryRow.m_Arrow.w >= 0);

			if(Layout.m_TwoColumns)
				CHECK(Layout.m_PreviewRow.x >= Layout.m_Form.x + Layout.m_Form.w);
			else
				CHECK(Layout.m_PreviewHeading.y >= Layout.m_Cooldown.y + Layout.m_Cooldown.h);
			CHECK(Layout.m_PreviewRow.x + Layout.m_PreviewRow.w <= View.x + View.w + .001f);
			const auto Preview = CPlayerPreviewLayout::Calculate(Layout.m_PreviewRow);
			CHECK(Preview.m_Tee.x + Preview.m_Tee.w <= Preview.m_Name.x + .001f);
			CHECK(Preview.m_Name.x + Preview.m_Name.w <= Preview.m_Clan.x + .001f);
			CHECK(Preview.m_Clan.x + Preview.m_Clan.w <= Preview.m_Flag.x + .001f);
			const auto ListRow = CPlayerCountryListRowLayout::Calculate({17, 31, View.w, 28});
			CHECK(ListRow.m_Flag.w <= 28 && ListRow.m_Flag.h <= 14);
			CHECK(ListRow.m_Flag.x + ListRow.m_Flag.w <= ListRow.m_Name.x + .001f);
			CHECK(ListRow.m_Name.x + ListRow.m_Name.w <= ListRow.m_Code.x + .001f);
			CHECK(ListRow.m_Code.x + ListRow.m_Code.w <= View.x + View.w + .001f);
			const auto Popup = CPlayerGalleryLayout::Popup({0, 0, std::max(20.f, View.w), std::max(20.f, View.h)});
			CHECK(std::abs(Popup.x + Popup.w / 2 - std::max(20.f, View.w) / 2) < .001f);
			const auto Gallery = CPlayerGalleryLayout::Calculate(View);
			CHECK(Gallery.m_Columns >= 1 && Gallery.m_Columns <= 2);
			CHECK(Gallery.m_Title.y + Gallery.m_Title.h <= Gallery.m_Search.y);
			CHECK(std::fmod(Gallery.m_Grid.h, CPlayerGalleryLayout::CellHeight) == 0);
			CHECK(Gallery.m_Search.y + Gallery.m_Search.h <= Gallery.m_Hint.y);
			CHECK(Gallery.m_Hint.y + Gallery.m_Hint.h <= Gallery.m_Grid.y);
			CHECK(Gallery.m_Grid.h >= 0 && Gallery.m_Grid.y + Gallery.m_Grid.h <= View.y + View.h + .001f);
		}
	for(float Height : {0.f, 1.f, 10.f, 38.f, 180.f})
	{
		const auto Gallery = CPlayerGalleryLayout::Calculate({0, 0, 240, Height});
		CHECK(Gallery.m_Grid.h >= 0 && Gallery.m_Grid.y + Gallery.m_Grid.h <= Height);
	}
	int PlayerCountry = -1, DummyCountry = 840, Changes = 0;
	bool ChangedDummy = false;
	CPlayerBindings Profiles;
	Profiles.m_Profiles[0].m_pCountry = &PlayerCountry;
	Profiles.m_Profiles[1].m_pCountry = &DummyCountry;
	Profiles.m_Changed = [&](bool Dummy) { ++Changes; ChangedDummy = Dummy; };
	CHECK(!Profiles.SelectCountry(false, -1) && Changes == 0);
	CHECK(Profiles.SelectCountry(true, 643) && Changes == 1 && ChangedDummy);
	CHECK(PlayerCountry == -1 && DummyCountry == 643);
	CHECK(Profiles.SelectCountry(false, 901) && Changes == 2 && !ChangedDummy);
	CHECK(PlayerCountry == 901 && DummyCountry == 643);
	CHECK(std::string(PlayerCountryName("GB-ENG")) == "England");
	CHECK(std::string(PlayerCountryName("ES-GA")) == "Galicia");
	CHECK(PlayerCountryName("custom") == nullptr);
	const auto Contains = [](const char *Text, const char *Query) { return std::string(Text).find(Query) != std::string::npos; };
	CHECK(PlayerCountryMatches("RU", "Russia", "localized-name", "643", "Russia", Contains));
	CHECK(PlayerCountryMatches("RU", "Russia", "localized-name", "643", "localized-name", Contains));
	CHECK(PlayerCountryMatches("RU", "Russia", "localized-name", "643", "RU", Contains));
	CHECK(PlayerCountryMatches("RU", "Russia", "localized-name", "643", "643", Contains));
	CHECK(PlayerCountryMatches("unknown", "unknown", "unknown", "999", "", Contains));
	CHECK(!PlayerCountryMatches("RU", "Russia", "localized-name", "643", "absent", Contains));
	CModePresentation ProfileMode;
	ProfileMode.m_Advanced = true;
	ProfileMode.UpdateMode(0, false);
	CHECK(ProfileMode.m_ShownAdvanced && ProfileMode.ContentInteractive());
	ProfileMode.SelectMode(false);
	ProfileMode.UpdateMode(.01, true);
	CHECK(ProfileMode.m_ShownAdvanced && !ProfileMode.ContentInteractive());
	ProfileMode.UpdateMode(.2, true);
	CHECK(!ProfileMode.m_ShownAdvanced);
	ProfileMode.UpdateMode(.4, true);
	CHECK(ProfileMode.ContentInteractive());
	ProfileMode.SelectMode(true);
	ProfileMode.UpdateMode(.41, true);
	ProfileMode.UpdateMode(2, true);
	CHECK(ProfileMode.m_ShownAdvanced && ProfileMode.ContentInteractive());

	if(pRoot)
	{
		const auto Source = [&](const char *pPath) {
			std::ifstream File(std::string(pRoot) + "/" + pPath);
			CHECK(File.good());
			std::ostringstream Buffer;
			Buffer << File.rdbuf();
			std::string Result;
			for(unsigned char Ch : Buffer.str())
				if(!std::isspace(Ch))
					Result += char(Ch);
			return Result;
		};
		const auto Player = Source("src/game/client/components/menus_settings_player.cpp");
		for(const char *pName : {"m_PlayerName", "m_PlayerClan", "m_ClDummyName", "m_ClDummyClan"})
			CHECK(Player.find("sizeof(g_Config." + std::string(pName) + ")") != std::string::npos);
		CHECK(Player.find("&g_Config.m_PlayerCountry") != std::string::npos);
		CHECK(Player.find("&g_Config.m_ClDummyCountry") != std::string::npos);
		CHECK(Player.find("m_Dummy=Dummy;SetNeedSendInfo();m_Dummy=Previous") != std::string::npos);
		CHECK(Player.find("m_aNextChangeInfo[Profile]") != std::string::npos);
		CHECK(Player.find("RenderIdleTeeAtCenter(") != std::string::npos);
		CHECK(Player.find("CSkins7::ms_apSkinVariables[int(Dummy)]") != std::string::npos);
		CHECK(Player.find("Dummy?g_Config.m_ClDummySkin:g_Config.m_ClPlayerSkin") != std::string::npos);

		const auto Renderer = Source("src/game/client/lucent/settings/player.cpp");
		CHECK(Renderer.find("RenderModeHeader(") != std::string::npos);
		CHECK(Renderer.find("m_ChooseButton") == std::string::npos);
		CHECK(Renderer.find("RenderPreview(Page,Layout,Profile)") != std::string::npos);
		CHECK(Renderer.find("OpenFlags(Page,Layout.m_Country,Profile)") != std::string::npos);
		CHECK(Renderer.find("FontIcon::CHEVRON_DOWN") != std::string::npos);
		CHECK(Renderer.find("Localize(\"Country\")") != std::string::npos);
		CHECK(Source("data/languages/russian.txt").find("Country==Страна") != std::string::npos);

		CHECK(Renderer.find("Localize(\"Player\"),Localize(\"Dummy\")") != std::string::npos);
		CHECK(Renderer.find("str_utf8_find_nocase(pText,pQuery)") != std::string::npos);
		CHECK(Renderer.find("Page.m_pFlags->Num()") != std::string::npos);
		CHECK(Renderer.find("Props.m_ClampToScreen=true") != std::string::npos);
		CHECK(Renderer.find("ClosePopupMenu(&Page,true)") != std::string::npos);
		CHECK(Renderer.find("ConsumeHotkey(CUi::HOTKEY_ESCAPE)") != std::string::npos);
		CHECK(Renderer.find("FlushInputs(Page);BlurInputs(Page)") != std::string::npos);
		CHECK(Renderer.find("Page.m_Bindings.SelectCountry(bool(Page.m_PopupProfile),Country)") != std::string::npos);
		CHECK(Renderer.find("!Page.m_Scroll.RectClipped(Rect)") != std::string::npos);
		CHECK(Renderer.find("pUi->DoEditBox(&Input,&Rect,11)") != std::string::npos);
		CHECK(Renderer.find("HasComposition()") != std::string::npos);
		CHECK(Renderer.find("FocusField(Page,2,int(Dummy))") != std::string::npos);
		const auto Popups = Source("src/game/client/ui_popups.cpp");
		CHECK(Popups.find("if(!Inside){ClosePopupMenu(pId)") != std::string::npos);
		CHECK(Popups.find("ConsumeHotkey(HOTKEY_ESCAPE)") != std::string::npos);
		std::ifstream FlagIndex(std::string(pRoot) + "/data/countryflags/index.txt");
		CHECK(FlagIndex.good());
		int ResourceNames = 0;
		std::vector<std::string> ResourceOrder;
		for(std::string Line; std::getline(FlagIndex, Line);)
		{
			if(!Line.empty() && Line.back() == '\r')
				Line.pop_back();
			if(Line.empty() || Line[0] == '#' || Line[0] == '=')
				continue;
			CHECK(PlayerCountryName(Line) != nullptr);
			ResourceOrder.push_back(Line);
			++ResourceNames;
		}
		CHECK(ResourceNames == std::size(PlayerCountryNames));
		std::sort(ResourceOrder.begin(), ResourceOrder.end());
		auto PeerOrder = ResourceOrder;
		PeerOrder.erase(std::remove(PeerOrder.begin(), PeerOrder.end(), "default"), PeerOrder.end());
		PlayerDefaultFirst(ResourceOrder, [](const std::string &Code) { return Code == "default"; });
		CHECK(ResourceOrder.front() == "default");
		CHECK(std::count(ResourceOrder.begin(), ResourceOrder.end(), "default") == 1);
		CHECK(std::vector<std::string>(ResourceOrder.begin() + 1, ResourceOrder.end()) == PeerOrder);
		CHECK(Renderer.find("if(Page.m_Search.IsEmpty())PlayerDefaultFirst(vFlags,") != std::string::npos);
		CHECK(Renderer.find("PlayerDefaultFirst(vFlags,") < Renderer.find("Selected=int(i)"));
	}
	std::puts("Player profile bindings/callbacks, name/code search, popup/focus contracts and resize/scale geometry passed");
}

int main(int argc, char **argv)
{
	CheckPlayerSettings(argc > 1 ? argv[1] : nullptr);
	using namespace LucentSettings;
	using C = EGeneralControl;
	const C Main[] = {C::DYNAMIC_CAMERA, C::WEAPON_PICKUP, C::WEAPON_EMPTY, C::DEFAULT_ZOOM,
		C::AUTO_DEMO, C::MAX_DEMOS, C::BEST_RACE_DEMO};
	const C Advanced[] = {C::SMOOTH_CAMERA, C::SKIP_START, C::UPDATE_RATE, C::POWER_SAVE,
		C::SETTINGS_FILE, C::SAVES_FILE, C::CONFIG_DIRECTORY, C::AUTO_SCREENSHOT, C::MAX_SCREENSHOTS,
		C::AUTO_STATBOARD, C::MAX_STATBOARD, C::AUTO_CSV, C::MAX_CSV, C::REPLAYS, C::REPLAY_LENGTH};
	CHECK(std::size(Main) + std::size(Advanced) == GeneralControlCount);
	const auto Measure = [](auto, float W) { return 13.f * std::ceil(180.f / std::max(1.f, W)); };
	const auto Snapshot = [&](const CGeneralLayout &Layout, const CGeneralState &State, float &Height) {
		std::array<CUIRect, GeneralControlCount> Rects{};
		std::array<bool, GeneralControlCount> Seen{};
		Height = Layout.Compose(State, Measure, [&](C Control, CUIRect Row) {
			CHECK(!Seen[int(Control)]);
			Seen[int(Control)] = true;
			CHECK(Row.w >= 0 && Row.h >= CGeneralLayout::RowHeight);
			CHECK(Row.x >= Layout.m_Left.x);
			const auto Slider = CGeneralLayout::Slider(Row);
			CHECK(Slider.x == Row.x && Slider.w == Row.w);
			const auto Column = Row.x < Layout.m_Right.x || !Layout.m_TwoColumns ? Layout.m_Left : Layout.m_Right;
			const auto Content = CGeneralLayout::ContentRegion(Column);
			CHECK(Row.x == Content.x && Row.w == Content.w);
			const bool IsSlider = Control == C::DEFAULT_ZOOM || Control == C::UPDATE_RATE || Control == C::REPLAY_LENGTH ||
				Control == C::MAX_DEMOS || Control == C::MAX_SCREENSHOTS || Control == C::MAX_STATBOARD || Control == C::MAX_CSV;
			if(IsSlider)
			{
				const auto Region = CGeneralLayout::ContentRegion(Column);
				CHECK(Row.w == Region.w && Row.x == Region.x);
				CUIRect Caption, Value, Rail;
				CGeneralLayout::SliderRegions(Row, Caption, Value, Rail);
				CHECK(Value.x + Value.w <= Row.x + Row.w + .001f);
				CHECK(Value.y + Value.h <= Slider.y + .001f);
				CHECK(Caption.x == Content.x);
				CHECK(std::abs(Value.x + Value.w - Content.x - Content.w) < .001f);
				for(float Pixel : {.5f, .75f, 1.f, 1.5f, 2.f, 3.f})
					for(float Position : {0.f, .5f, 1.f})
					{
						const CUIRect PaintRail = {Slider.x, Slider.y + 5, Slider.w, Slider.h - 10};
						const float HandleWidth = std::min(std::max(33.f, PaintRail.h), std::max(0.f, PaintRail.w / 3));
						const CUIRect Handle = {PaintRail.x + (PaintRail.w - HandleWidth) * Position, PaintRail.y, HandleWidth, PaintRail.h};
						const auto Paint = LucentUi::CSliderGeometry::Calculate(PaintRail, Handle, Position, Pixel, true);
						CHECK(Paint.m_Track.x == Caption.x);
						CHECK(std::abs(Paint.m_Track.x + Paint.m_Track.w - Value.x - Value.w) < .001f);
						CHECK(Paint.m_Thumb.x >= Content.x - .001f);
						CHECK(Paint.m_Thumb.x + Paint.m_Thumb.w <= Content.x + Content.w + .001f);
					}
			}
			else
			{
				CUIRect Caption, Checkbox;
				const float LabelOffset = LucentStyle::Checkbox::Size + LucentStyle::Style::LabelGap;
				Row.VSplitLeft(std::min(LabelOffset, Row.w), &Checkbox, &Caption);
				CHECK(Checkbox.x == Content.x);
				CHECK(std::abs(Caption.x + Caption.w - Content.x - Content.w) < .001f);
				CHECK(Row.h >= Measure(Control, std::max(1.f, Row.w - LabelOffset)) + 8);
			}
			CHECK(Slider.y + Slider.h <= Row.y + Row.h + .001f);
			Row.h *= State.m_Reveal[int(Control)];
			Rects[int(Control)] = Row; }, [](EGeneralSection, CUIRect) {});
		for(int i = 0; i < GeneralControlCount; ++i)
		{
			CHECK(Seen[i] == State.m_Visible[i]);
			if(!Seen[i])
				continue;
			CHECK(Rects[i].y + Rects[i].h <= Layout.m_Left.y + Height + .01f);
			for(int j = i + 1; j < GeneralControlCount; ++j)
			{
				if(!Seen[j])
					continue;
				const auto A = Rects[i], B = Rects[j];
				CHECK(std::min(A.x + A.w, B.x + B.w) - std::max(A.x, B.x) <= .001f ||
					std::min(A.y + A.h, B.y + B.h) - std::max(A.y, B.y) <= .001f);
			}
		}
		return Rects;
	};
	for(float Width : {0.f, 160.f, 400.f, 619.f, 620.f, 956.f, 1600.f})
		for(bool Mode : {false, true})
			for(int Flags = 0; Flags < 64; ++Flags)
			{
				const auto Layout = CGeneralLayout::Calculate({54, 40, Width, 500});
				CHECK(Layout.m_TwoColumns == (Width >= 620));
				CHECK(Layout.m_Right.x + Layout.m_Right.w <= 54 + Width + .001f);
				if(Layout.m_TwoColumns)
					CHECK(Layout.m_Left.x + Layout.m_Left.w < Layout.m_Right.x && Layout.m_Right.w > Layout.m_Left.w);
				CGeneralState Target;
				Target.m_Visible[int(C::SMOOTH_CAMERA)] = Flags & 1;
				for(size_t i = 0; i < CGeneralPresentation::Limits.size(); ++i)
					Target.m_Enabled[int(CGeneralPresentation::Limits[i])] = Flags & (2 << i);
				const auto ValuesBefore = Target;
				CGeneralPresentation Presentation;
				Presentation.m_Advanced = Mode;
				const auto State = Presentation.Update(Target, 0, false);
				CHECK(State.m_Advanced == Mode && State.m_ContentAlpha == 1);
				for(C Control : Main)
					CHECK(State.m_Visible[int(Control)] == (!Mode && (Control != C::MAX_DEMOS || Target.m_Enabled[int(Control)])));
				for(C Control : Advanced)
				{
					bool Visible = Mode && Target.m_Visible[int(Control)];
					for(C Limit : CGeneralPresentation::Limits)
						if(Control == Limit)
							Visible = Visible && Target.m_Enabled[int(Control)];
					CHECK(State.m_Visible[int(Control)] == Visible);
				}
				float Height;
				const auto Rects = Snapshot(Layout, State, Height);
				for(C Limit : CGeneralPresentation::Limits)
					CHECK(State.Interactive(Limit) == State.m_Visible[int(Limit)]);
				if(Layout.m_TwoColumns)
				{
					CHECK(Rects[int(Mode ? C::SKIP_START : C::DEFAULT_ZOOM)].x == CGeneralLayout::ContentRegion(Layout.m_Left).x);
					CHECK(Rects[int(Mode ? C::AUTO_CSV : C::BEST_RACE_DEMO)].x == CGeneralLayout::ContentRegion(Layout.m_Right).x);
				}
				if(Width == 956 && !Mode)
					CHECK(Height + 80 < 400);
				// A translated scroll viewport must preserve geometry and content height.
				const auto Scrolled = CGeneralLayout::Calculate({54, -83, Width, 500});
				float ScrolledHeight;
				const auto Shifted = Snapshot(Scrolled, State, ScrolledHeight);
				CHECK(std::abs(Height - ScrolledHeight) < .01f);
				for(int i = 0; i < GeneralControlCount; ++i)
					if(State.m_Visible[i])
						CHECK(Shifted[i].x == Rects[i].x && std::abs(Shifted[i].y - Rects[i].y + 123) < .01f);
				// Presentation takes no ownership of config values or parent state.
				Presentation.ToggleMode();
				Presentation.Update(Target, .01, false);
				CHECK(Target.m_Enabled == ValuesBefore.m_Enabled && Target.m_Visible == ValuesBefore.m_Visible);
			}
	// Both modes: all five conditional rows retain values/IDs while revealing,
	// collapsing and reversing. Only fully shown rows own interactive hitboxes.
	for(float Width : {240.f, 956.f})
		for(C Limit : CGeneralPresentation::Limits)
		{
			CGeneralState Target;
			for(C Item : CGeneralPresentation::Limits)
				Target.m_Enabled[int(Item)] = false;
			CGeneralPresentation Presentation;
			Presentation.m_Advanced = Limit != C::MAX_DEMOS;
			const auto Layout = CGeneralLayout::Calculate({0, 0, Width, 500});
			auto State = Presentation.Update(Target, 0, true);
			float Height;
			const auto Closed = Snapshot(Layout, State, Height);
			CHECK(!State.m_Visible[int(Limit)] && !State.Interactive(Limit));
			Target.m_Enabled[int(Limit)] = true;
			float Previous = 0;
			for(double Time : {.01, .04, .08, .14, .20})
			{
				State = Presentation.Update(Target, Time, true);
				Snapshot(Layout, State, Height);
				CHECK(State.m_Reveal[int(Limit)] >= Previous);
				CHECK(State.Interactive(Limit) == (State.m_Reveal[int(Limit)] == 1));
				Previous = State.m_Reveal[int(Limit)];
			}
			CHECK(State.Interactive(Limit));
			Target.m_Enabled[int(Limit)] = false;
			for(double Time : {.21, .24, .30, .40})
			{
				State = Presentation.Update(Target, Time, true);
				Snapshot(Layout, State, Height);
				CHECK(State.m_Reveal[int(Limit)] <= Previous && !State.Interactive(Limit));
				Previous = State.m_Reveal[int(Limit)];
			}
			CHECK(!State.m_Visible[int(Limit)]);
			const auto Again = Snapshot(Layout, State, Height);
			for(int i = 0; i < GeneralControlCount; ++i)
				CHECK(Again[i].y == Closed[i].y && Again[i].h == Closed[i].h);
			Target.m_Enabled[int(Limit)] = true;
			Presentation.Update(Target, .41, true);
			const float Partial = Presentation.Update(Target, .48, true).m_Reveal[int(Limit)];
			Target.m_Enabled[int(Limit)] = false;
			CHECK(Presentation.Update(Target, .48, true).m_Reveal[int(Limit)] == Partial);
			CHECK(Presentation.Update(Target, .49, false).m_Reveal[int(Limit)] == 0);
		}
	{
		CGeneralPresentation Presentation;
		CGeneralState Target;
		auto State = Presentation.Update(Target, 0, true);
		CHECK(!State.m_Advanced && State.Interactive(C::DEFAULT_ZOOM));
		Presentation.m_FileFocus = 2;
		Presentation.ToggleMode();
		CHECK(Presentation.m_FileFocus == -1);
		State = Presentation.Update(Target, .01, true);
		CHECK(!State.m_Advanced && !State.Interactive(C::DEFAULT_ZOOM));
		State = Presentation.Update(Target, .05, true);
		CHECK(!State.m_Advanced && State.m_ContentAlpha > 0 && State.m_ContentAlpha < 1);
		State = Presentation.Update(Target, .11, true);
		CHECK(State.m_Advanced && State.m_ContentAlpha == 0 && !State.Interactive(C::UPDATE_RATE));
		State = Presentation.Update(Target, .21, true);
		CHECK(State.m_Advanced && State.Interactive(C::UPDATE_RATE) && !State.m_Visible[int(C::DEFAULT_ZOOM)]);
		// Returning from another category snaps presentation, not the chosen mode.
		State = Presentation.Update(Target, 4, true);
		CHECK(Presentation.m_Advanced && State.m_Advanced && State.m_ContentAlpha == 1);
		Presentation.ToggleMode();
		State = Presentation.Update(Target, 4.01, false);
		CHECK(!State.m_Advanced && State.Interactive(C::DEFAULT_ZOOM));
		Presentation.ToggleMode();
		Presentation.Update(Target, 4.02, true);
		Presentation.Update(Target, 4.06, true);
		Presentation.ToggleMode();
		State = Presentation.Update(Target, 4.06, true);
		CHECK(!State.m_Advanced && State.m_ContentAlpha > 0);
		State = Presentation.Update(Target, 4.20, true);
		CHECK(!State.m_Advanced && State.m_ContentAlpha == 1);
	}
	// Header layout uses measured widths at the final rasterized size. Simulate
	// localization lengths and DPI quantization (not proportional font scaling).
	for(float Pixel : {.5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 3.f})
		for(float Width : {0.f, 40.f, 100.f, 160.f, 300.f, 480.f, 960.f})
			for(const auto Lengths : {std::array{4.f, 12.f}, std::array{5.f, 24.f}, std::array{10.f, 40.f}})
			{
				const auto MeasureHeader = [&](float Size) {
					const float Aligned = std::round(Size / Pixel) * Pixel;
					return CGeneralHeaderLayout::CMetrics{{Lengths[0] * Aligned * .55f, Lengths[1] * Aligned * .55f}, Aligned};
				};
				const CUIRect View = {13.25f, 29.5f, Width, 400};
				const auto Header = CGeneralHeaderLayout::Calculate(View, Pixel, MeasureHeader);
				CHECK(Header.m_FontSize >= 0 && Header.m_FontSize <= 15);
				CHECK(Header.m_Header.x == View.x && Header.m_Content.x == View.x);
				CHECK(Header.m_Content.y >= Header.m_Header.y + Header.m_Header.h + 7.99f);
				if(Width == 960)
				{
					CHECK(Header.m_FontSize == 15);
					CHECK(std::abs(Header.m_Modes[1].x - Header.m_Modes[0].x - Header.m_Modes[0].w - 18) < .001f);
					CHECK(Header.m_Header.h < 25);
				}
				CHECK(Header.m_Modes[0].y == Header.m_Modes[1].y);
				CHECK(Header.m_Modes[0].h == Header.m_Modes[1].h);
				CHECK(Header.m_Modes[0].x + Header.m_Modes[0].w <= Header.m_Modes[1].x + .001f);
				CHECK(Header.m_Modes[1].x + Header.m_Modes[1].w <= View.x + Width + .001f);
				for(int i = 0; i < 2; ++i)
				{
					CHECK(Header.m_Underlines[i].x == Header.m_Modes[i].x);
					CHECK(Header.m_Underlines[i].w == Header.m_Modes[i].w);
					CHECK(Header.m_Underlines[i].y == Header.m_Baseline + 2);
					CHECK(Header.m_Underlines[i].y + Header.m_Underlines[i].h <= Header.m_Header.y + Header.m_Header.h);
					if(Header.m_FontSize > 0)
					{
						const auto Metrics = MeasureHeader(Header.m_FontSize);
						CHECK(Header.m_Modes[i].w == Metrics.m_Widths[i]);
						CHECK(Header.m_Baseline == Header.m_TextY + Metrics.m_AlignedSize);
					}
				}
			}
	// Explicit header selection is idempotent, even during a fade. Leaving the
	// page clears transient presentation while preserving the session mode.
	{
		CGeneralPresentation Presentation;
		CGeneralState Target;
		Presentation.Update(Target, 0, true);
		CHECK(!Presentation.SelectMode(false));
		CHECK(Presentation.SelectMode(true));
		Presentation.Update(Target, .01, true);
		const auto Partial = Presentation.Update(Target, .04, true);
		const auto Start = Presentation.m_ModeReveal.m_Start;
		Presentation.m_FileFocus = 1;
		CHECK(!Presentation.SelectMode(true));
		CHECK(Presentation.m_FileFocus == 1);
		CHECK(Presentation.Update(Target, .04, true).m_ContentAlpha == Partial.m_ContentAlpha);
		CHECK(Presentation.m_ModeReveal.m_Start == Start);
		Presentation.FocusMode();
		CHECK(Presentation.m_FocusedAdvanced && Presentation.m_ModeFocused);
		for(int i = 0; i < 2; ++i)
		{
			Presentation.m_HeaderHover[i].Update(true, .04, false);
			Presentation.m_HeaderFocus[i].Update(true, .04, false);
			Presentation.m_HeaderSelection[i].Update(true, .04, false);
		}
		Presentation.Leave();
		CHECK(Presentation.m_Advanced && !Presentation.m_ModeFocused && Presentation.m_FileFocus == -1);
		for(int i = 0; i < 2; ++i)
			CHECK(Presentation.m_HeaderHover[i].m_Value == 0 && Presentation.m_HeaderFocus[i].m_Value == 0 && Presentation.m_HeaderSelection[i].m_Value == 0);
		const auto Returned = Presentation.Update(Target, .05, true);
		CHECK(Returned.m_Advanced && Returned.m_ContentAlpha == 1);
		CHECK(Presentation.m_HeaderSelection[0].m_Value == 0 && Presentation.m_HeaderSelection[1].m_Value == 1);
		// Underline transitions reverse from their current value, without jumps.
		auto &Underline = Presentation.m_HeaderSelection[1];
		Underline.Update(false, .06, true, LucentStyle::Motion::Selection);
		const float Mid = Underline.Update(false, .10, true, LucentStyle::Motion::Selection);
		CHECK(Mid > 0 && Mid < 1);
		CHECK(Underline.Update(true, .10, true, LucentStyle::Motion::Selection) == Mid);
		CHECK(Underline.Update(true, .40, true, LucentStyle::Motion::Selection) == 1);
	}
	// A mode is replaced only at zero opacity, including rapid reversals.
	for(double Fps : {60., 144., 240.})
	{
		CGeneralPresentation Presentation;
		CGeneralState Target;
		bool Shown = false;
		for(int Frame = 0; Frame < int(Fps); ++Frame)
		{
			const double Now = Frame / Fps;
			if(Frame % 7 == 1)
				Presentation.ToggleMode();
			const auto State = Presentation.Update(Target, Now, true);
			CHECK(State.m_ContentAlpha >= 0 && State.m_ContentAlpha <= 1);
			if(State.m_Advanced != Shown)
				CHECK(State.m_ContentAlpha == 0);
			Shown = State.m_Advanced;
			for(C Control : Main)
				if(State.m_Advanced)
					CHECK(!State.m_Visible[int(Control)]);
			for(C Control : Advanced)
				if(!State.m_Advanced)
					CHECK(!State.m_Visible[int(Control)]);
			if(State.m_ContentAlpha < 1)
				for(int i = 0; i < GeneralControlCount; ++i)
					CHECK(!State.Interactive(C(i)));
		}
		Presentation.FocusMode();
		CHECK(Presentation.m_ModeFocused && Presentation.m_FileFocus == -1);
		Presentation.ToggleMode();
		CHECK(Presentation.m_ModeFocused); // keyboard activation keeps real focus
		Presentation.ClearKeyboardFocus(); // pointer activation removes keyboard focus
		CHECK(!Presentation.m_ModeFocused && Presentation.m_FileFocus == -1);
		Presentation.ToggleMode();
		CHECK(!Presentation.m_ModeFocused); // selection never manufactures focus
	}
	// Lucent paint geometry: a single track/fill baseline and a distinct round knob.
	for(float Pixel : {.25f, .5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 3.f})
		for(float Width : {0.f, .1f, .5f, 1.f, 8.f, 33.f, 100.f, 156.f})
			for(float Value : {-1.f, 0.f, .001f, .1f, .5f, .999f, 1.f, 2.f})
			{
				const CUIRect Rail = {10.37f, 21.13f, Width, 18};
				const float ThumbWidth = std::min(33.f, Width);
				const CUIRect Handle = {Rail.x + (Width - ThumbWidth) * std::clamp(Value, 0.f, 1.f), Rail.y, ThumbWidth, Rail.h};
				const auto Geometry = LucentUi::CSliderGeometry::Calculate(Rail, Handle, Value, Pixel);
				const auto Coverage = LucentUi::CSliderGeometry::ThumbCoverageRadii(Geometry.m_Thumb, Pixel);
				const float Radius = LucentUi::CSliderGeometry::Radius(Geometry.m_Thumb);
				CHECK(Coverage.x > 0 && Coverage.x < Radius && Coverage.y > Radius);
				CHECK(std::abs((Coverage.x + Coverage.y) / 2 - Radius) < .001f);
				CHECK(Coverage.y - Coverage.x <= Pixel + .001f);
				if(Radius >= Pixel)
					CHECK(std::abs(Coverage.y - Coverage.x - Pixel) < .001f);
				if(Pixel == 1)
				{
					CHECK(Geometry.m_Track.h == 3);
					CHECK(Geometry.m_Thumb.w == 8 && Geometry.m_Thumb.h == 8);
				}
				if(Width >= 33)
				{
					// Rounded caps and the whole knob remain inside the row clip,
					// including the min/max endpoints and pixel snapping extremes.
					CHECK(Geometry.m_Track.x >= Rail.x);
					CHECK(Geometry.m_Track.x + Geometry.m_Track.w <= Rail.x + Rail.w);
					CHECK(Geometry.m_Thumb.x >= Rail.x);
					CHECK(Geometry.m_Thumb.x + Geometry.m_Thumb.w <= Rail.x + Rail.w);
					CHECK(Geometry.m_Thumb.y >= Rail.y && Geometry.m_Thumb.y + Geometry.m_Thumb.h <= Rail.y + Rail.h);
				}
				for(const CUIRect Rect : {Geometry.m_Track, Geometry.m_Fill, Geometry.m_Thumb})
				{
					CHECK(std::isfinite(Rect.x + Rect.y + Rect.w + Rect.h));
					CHECK(Rect.w >= 0 && Rect.h > 0);
					const float Radius = LucentUi::CSliderGeometry::Radius(Rect);
					// DDNet's rounded-rect tessellator does not clamp negative centers.
					CHECK(Rect.w - 2 * Radius >= -.001f && Rect.h - 2 * Radius >= -.001f);
				}
				CHECK(Geometry.m_Fill.x == Geometry.m_Track.x && Geometry.m_Fill.y == Geometry.m_Track.y);
				CHECK(Geometry.m_Fill.h == Geometry.m_Track.h);
				CHECK(Geometry.m_Fill.w <= Geometry.m_Track.w);
				CHECK(Geometry.m_Thumb.w == Geometry.m_Thumb.h);
				CHECK(Geometry.m_Thumb.h > Geometry.m_Track.h);
				CHECK(std::abs(Geometry.m_Thumb.y + Geometry.m_Thumb.h / 2 - Geometry.m_Track.y - Geometry.m_Track.h / 2) < .001f);
				const float KnobCenter = Geometry.m_Thumb.x + Geometry.m_Thumb.w / 2;
				CHECK(std::abs(KnobCenter - Geometry.m_Fill.x - Geometry.m_Fill.w) <= Pixel / 2 + .001f);
				if(Value <= 0)
					CHECK(std::abs(KnobCenter - Geometry.m_Track.x) < .001f);
				if(Value >= 1)
					CHECK(std::abs(KnobCenter - Geometry.m_Track.x - Geometry.m_Track.w) < .001f);
				CHECK(std::abs(Geometry.m_Track.y / Pixel - std::round(Geometry.m_Track.y / Pixel)) < .001f);
				CHECK(std::abs(Geometry.m_Fill.w - Geometry.m_Track.w * std::clamp(Value, 0.f, 1.f)) < .001f);
				if(Value <= 0)
					CHECK(Geometry.m_Fill.w == 0); // min/demo limit 1: no fill primitive
				if(Value >= 1)
					CHECK(Geometry.m_Fill.w == Geometry.m_Track.w); // max/infinity
				CUIRect Hovered = Handle;
				Hovered.y -= 1.5f;
				Hovered.h += 3;
				const auto Hot = LucentUi::CSliderGeometry::Calculate(Rail, Hovered, Value, Pixel);
				CHECK(Hot.m_Thumb.x == Geometry.m_Thumb.x && Hot.m_Thumb.y == Geometry.m_Thumb.y && Hot.m_Thumb.h == Geometry.m_Thumb.h);
			}
	// The stacked header reserves a stable value cell; values never shorten
	// the rail, even at minimum, maximum and the infinity endpoint.
	for(float Width : {40.f, 100.f, 280.f, 344.f, 400.f, 500.f})
		for(float Header : {18.f, 30.f, 56.f})
			for(float Pixel : {.5f, .75f, 1.f, 1.5f, 2.f, 3.f})
			{
				const CUIRect Block = {12, 24, Width, Header + CGeneralLayout::SliderHeight};
				CUIRect Caption, ValueRow, RailRow;
				CGeneralLayout::SliderRegions(Block, Caption, ValueRow, RailRow);
				CHECK(RailRow.x == Block.x && RailRow.w == Block.w);
				CHECK(ValueRow.y + ValueRow.h <= RailRow.y);
				CHECK(ValueRow.x + ValueRow.w == Block.x + Block.w);
				CHECK(Caption.x + Caption.w <= ValueRow.x);
				const CUIRect Rail = {RailRow.x, RailRow.y + 5, RailRow.w, RailRow.h - 10};
				const float HandleWidth = std::min(std::max(33.f, Rail.h), Rail.w / 3);
				for(float Value : {0.f, .001f, .5f, .999f, 1.f})
				{
					const CUIRect Handle = {Rail.x + (Rail.w - HandleWidth) * Value, Rail.y, HandleWidth, Rail.h};
					const auto Paint = LucentUi::CSliderGeometry::Calculate(Rail, Handle, Value, Pixel);
					CHECK(Paint.m_Track.x >= RailRow.x && Paint.m_Track.x + Paint.m_Track.w <= RailRow.x + RailRow.w);
					CHECK(Paint.m_Thumb.y >= RailRow.y && Paint.m_Thumb.y + Paint.m_Thumb.h <= RailRow.y + RailRow.h);
				}
			}
	// Value cells share a fixed left edge for all values, including infinity;
	// only paint moves, while the existing slider rectangle remains unchanged.
	for(float Pixel : {.25f, .5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 3.f})
		for(float Width : {40.f, 80.f, 100.f, 156.f})
		{
			const CUIRect Group = {10.37f, 21.13f, Width, 28};
			const float Reserved = std::min(56.f, Width * .4f);
			const CUIRect Slider = {Group.x, Group.y, Width - Reserved, Group.h};
			const auto Label = LucentUi::CSliderGeometry::ValueLabel(Group, Slider, Pixel);
			CHECK(Label.x >= Group.x && Label.x + Label.w <= Group.x + Group.w + .001f);
			CHECK(Label.y == Group.y && Label.h == Group.h);
			if(Width == 156)
				CHECK(Label.w >= 56); // four digits + localized Hz have the existing reserve
			const CUIRect Rail = {Slider.x, Slider.y + 5, Slider.w, Slider.h - 10};
			const float HandleWidth = std::min(std::max(33.f, Rail.h), Rail.w / 3);
			for(float Value : {0.f, .001f, .02f, .5f, .999f, 1.f})
			{
				const CUIRect Handle = {Rail.x + (Rail.w - HandleWidth) * Value, Rail.y, HandleWidth, Rail.h};
				const auto Paint = LucentUi::CSliderGeometry::Calculate(Rail, Handle, Value, Pixel);
				const float Gap = Label.x - Paint.m_Track.x - Paint.m_Track.w;
				CHECK(std::abs(Gap - 10.f) < .001f);
				CHECK(Paint.m_Thumb.x + Paint.m_Thumb.w < Label.x);
				CHECK(Paint.m_Fill.x + Paint.m_Fill.w <= Label.x);
				CHECK(Label.w > 0);
			}
		}
	for(vec2 Origin : {vec2(0, 0), vec2(10, 34)})
		for(float Width : {40.f, 180.f, 640.f, 1100.f})
			for(float Height : {30.f, 180.f, 420.f, 800.f})
			{
				const CUIRect View = {Origin.x, Origin.y, Width, Height};
				const auto Closed = CLayout::Calculate(View, 0);
				for(float Reveal : {0.f, .2f, .6f, 1.f})
				{
					const auto Layout = CLayout::Calculate(View, Reveal);
					CHECK(Layout.m_Sidebar.x > View.x && Layout.m_Sidebar.y > View.y);
					CHECK(Layout.m_Sidebar.y + Layout.m_Sidebar.h < View.y + View.h);
					CHECK(std::abs(Layout.m_Sidebar.y + Layout.m_Sidebar.h / 2 - View.y - View.h / 2) < .001f);
					CHECK(Layout.m_Sidebar.x == Closed.m_Sidebar.x && Layout.m_Sidebar.y == Closed.m_Sidebar.y);
					CHECK(Layout.m_Sidebar.h == Closed.m_Sidebar.h);
					CHECK(Layout.m_Content.y == View.y && Layout.m_Content.h == View.h);
					CHECK(Layout.m_Back.y + Layout.m_Back.h < Layout.m_Sidebar.y + Layout.m_Sidebar.h);
					CHECK(Layout.m_Back.h > 0 && Layout.m_Back.y > Layout.m_Viewport.y + Layout.m_Viewport.h);
					CHECK(Layout.m_Viewport.y + Layout.m_Viewport.h <= Layout.m_Back.y);
					CHECK(Layout.m_Content.x == Closed.m_Content.x && Layout.m_Content.w == Closed.m_Content.w);
					CHECK(Layout.m_Sidebar.w <= Width && Layout.m_Content.w >= 0);
					CNavigation Nav;
					for(int i = 0; i < CategoryCount; ++i)
					{
						const auto Idle = Layout.VisualRow(i, 0, 0);
						const auto Hovered = Layout.VisualRow(i, 0, 1);
						const auto Slot = Layout.Row(i, 0);
						CHECK(Hovered.h > Idle.h && Hovered.y >= Slot.y);
						CHECK(Hovered.y + Hovered.h <= Slot.y + Slot.h);
						CHECK(Idle.x == Hovered.x && Idle.w == Hovered.w);
						CHECK(Slot.x == Closed.Row(i, 0).x);
						Nav.Focus(i);
						Nav.RevealFocus(Layout);
						CHECK(Nav.m_Focus == i && Nav.m_Scroll >= 0 && Nav.m_Scroll <= Layout.MaxScroll());
						const auto Hit = Layout.Hitbox(i, Nav.m_Scroll);
						CHECK(Hit.h > 0 && Hit.y >= Layout.m_Viewport.y);
						CHECK(Hit.y + Hit.h <= Layout.m_Viewport.y + Layout.m_Viewport.h + .001f);
						Nav.Move(1);
						CHECK(Nav.m_Focus == (i + 1) % CategoryCount);
					}
					Nav.Scroll(10000, Layout);
					CHECK(Nav.m_Scroll == Layout.MaxScroll());
					Nav.Scroll(-10000, Layout);
					CHECK(Nav.m_Scroll == 0);
				}
			}
	const auto Normal = CLayout::Calculate({0, 0, 1000, 800}, 1);
	CHECK(Normal.m_Sidebar.w == ExpandedWidth && Normal.MaxScroll() == 0);
	CHECK(Normal.m_Sidebar.h == CategoryCount * RowHeight + RowHeight + FooterGap + 2 * Padding);
	for(float Size : {0.f, 1.f, 8.f})
	{
		const auto Tiny = CLayout::Calculate({0, 0, Size, Size}, 1);
		CHECK(Tiny.m_Sidebar.w >= 0 && Tiny.m_Sidebar.h >= 0 && Tiny.m_Viewport.h >= 0);
		CHECK(Tiny.m_Sidebar.x + Tiny.m_Sidebar.w <= Size);
		CHECK(Tiny.m_Back.x + Tiny.m_Back.w <= Tiny.m_Sidebar.x + Tiny.m_Sidebar.w);
		CHECK(Tiny.m_Back.y + Tiny.m_Back.h <= Tiny.m_Sidebar.y + Tiny.m_Sidebar.h);
	}
	CNavigation Nav;
	CHECK(Nav.m_Focus == -1 && Nav.m_Expansion.m_Value == 0);
	Nav.Focus(0);
	Nav.Move(-1);
	CHECK(Nav.m_Focus == CategoryCount - 1);
	Nav.UpdateExpansion(true, 0, true);
	const float Half = Nav.UpdateExpansion(true, .09, true);
	CHECK(Half > 0 && Half < 1);
	CHECK(Nav.UpdateExpansion(false, .09, true) == Half);
	CHECK(Nav.UpdateExpansion(false, 1, true) == 0);
	CHECK(Nav.UpdateExpansion(true, 2, false) == 1);
	CHECK(Nav.UpdateExpansion(false, 2, false) == 0);
	Nav.Reset();
	Nav.UpdateExpansion(true, 0, true);
	CHECK(Nav.UpdateExpansion(true, .181, true) == 1);
	const auto Expanded = CLayout::Calculate({0, 0, 1000, 800}, Nav.m_Expansion.m_Value);
	// The revealed surface extends over content; its whole width owns hover/input.
	CHECK(Expanded.m_Sidebar.x + Expanded.m_Sidebar.w > Expanded.m_Content.x);
	CHECK(Expanded.m_Viewport.w == Expanded.m_Sidebar.w);
	Nav.UpdateExpansion(false, .2, true);
	CHECK(Nav.UpdateExpansion(false, .381, true) == 0);
	Nav.Reset();
	CHECK(Nav.m_Focus == -1 && Nav.m_Scroll == 0 && Nav.m_Expansion.m_Value == 0);

	// Sound shares mode behavior and slider geometry, with independent page state.
	for(float Width : {320.f, 540.f, 800.f, 1200.f})
		for(bool AdvancedMode : {false, true})
		{
			const auto Layout = CSoundLayout::Calculate({13, 21, Width, 500});
			std::array<bool, SoundControlCount> Seen{};
			int Count = 0;
			const float Height = Layout.Compose(AdvancedMode, [](auto, float W) { return 13.f * std::ceil(210.f / std::max(1.f, W)); }, [&](ESoundControl Control, CUIRect Row) {
     CHECK(!Seen[int(Control)]); Seen[int(Control)] = true; ++Count;
     const auto Column = Layout.m_TwoColumns && Row.x == Layout.m_Right.x ? Layout.m_Right : Layout.m_Left;
     CHECK(Row.x == Column.x && Row.w == Column.w);
     CHECK(Row.x >= 13 && Row.x + Row.w <= 13 + Width + .001f);
     if(SoundSlider(Control)) {
      CUIRect Caption, Value, Rail;
      CGeneralLayout::SliderRegions(Row, Caption, Value, Rail);
      CHECK(Caption.x == Column.x && Rail.x == Column.x && Rail.w == Column.w);
      CHECK(std::abs(Value.x + Value.w - Column.x - Column.w) < .001f);
      CHECK(Value.y + Value.h <= Rail.y);
     } }, [](int, CUIRect) {});
			CHECK(Count == (AdvancedMode ? 6 : 9));
			CHECK(Seen[int(ESoundControl::MUTE)] == AdvancedMode);
			CHECK(Seen[int(ESoundControl::ENABLE)] != AdvancedMode);
			if(Width >= 540)
				CHECK(Height <= 350);
		}
	for(bool Sounds : {false, true})
		for(bool Game : {false, true})
			for(bool Music : {false, true})
			{
				const CSoundState State(Sounds, Game, Music);
				CHECK(State.m_Enabled[int(ESoundControl::ENABLE)]);
				CHECK(State.m_Enabled[int(ESoundControl::VOLUME)] == Sounds);
				CHECK(State.m_Enabled[int(ESoundControl::GAME_VOLUME)] == (Sounds && Game));
				CHECK(State.m_Enabled[int(ESoundControl::MAP_VOLUME)] == (Sounds && Game));
				CHECK(State.m_Enabled[int(ESoundControl::GUN)] == (Sounds && Game));
				CHECK(State.m_Enabled[int(ESoundControl::LONG_PAIN)] == (Sounds && Game));
				CHECK(State.m_Enabled[int(ESoundControl::MUSIC_VOLUME)] == (Sounds && Music));
				CHECK(State.m_Enabled[int(ESoundControl::CHAT_VOLUME)] == Sounds);
			}
	CModePresentation SoundMode, GeneralMode;
	SoundMode.UpdateMode(0, true);
	GeneralMode.UpdateMode(0, true);
	CHECK(SoundMode.ContentInteractive());
	CHECK(SoundMode.SelectMode(true));
	SoundMode.UpdateMode(.01, true);
	CHECK(!SoundMode.ContentInteractive() && !SoundMode.m_ShownAdvanced);
	SoundMode.UpdateMode(.04, true);
	CHECK(SoundMode.m_ModeReveal.m_Value > 0 && SoundMode.m_ModeReveal.m_Value < 1);
	SoundMode.UpdateMode(.2, true);
	CHECK(SoundMode.m_ShownAdvanced && !SoundMode.ContentInteractive());
	SoundMode.UpdateMode(.4, true);
	CHECK(SoundMode.ContentInteractive());
	CHECK(!GeneralMode.m_Advanced && !GeneralMode.m_ShownAdvanced);
	SoundMode.FocusMode();
	SoundMode.Leave();
	CHECK(SoundMode.m_Advanced && !SoundMode.m_ModeFocused);
	SoundMode.UpdateMode(1, true);
	CHECK(SoundMode.m_ShownAdvanced && SoundMode.ContentInteractive());
	SoundMode.SelectMode(false);
	SoundMode.UpdateMode(1.01, false);
	CHECK(!SoundMode.m_ShownAdvanced && SoundMode.ContentInteractive());
	std::puts("Sound modes, full-width anchors, compact geometry, dependencies, fade/input and session state passed");

	if(argc > 1)
	{
		// Source-level integration contracts supplement the standalone geometry
		// target: detect duplicate owners and wrong IDs/ranges/callback routing.
		const auto Source = [&](const char *pPath) {
			std::ifstream File(std::string(argv[1]) + "/" + pPath);
			CHECK(File.good());
			std::ostringstream Buffer;
			Buffer << File.rdbuf();
			std::string Result;
			for(unsigned char Ch : Buffer.str())
				if(!std::isspace(Ch))
					Result += char(Ch);
			return Result;
		};
		const auto General = Source("src/game/client/components/menus_settings_general.cpp");
		const auto Ddnet = Source("src/game/client/components/menus_settings_ddnet.cpp");
		for(const char *pSymbol : {"m_ClDefaultZoom", "m_ClAutoRaceRecord", "m_ClReplays", "m_ClReplayLength"})
		{
			CHECK(Ddnet.find(pSymbol) == std::string::npos);
			CHECK(General.find("&g_Config." + std::string(pSymbol)) != std::string::npos);
		}
		CHECK(General.find("DoScrollbarOption(&g_Config.m_ClDefaultZoom,") != std::string::npos);
		CHECK(General.find("Localize(\"Defaultzoom\"),0,20,") != std::string::npos);
		CHECK(General.find("m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom-10),g_Config.m_ClSmoothZoomTime,true)") != std::string::npos);
		CHECK(General.find("if(Client()->State()==IClient::STATE_ONLINE){Client()->DemoRecorder_UpdateReplayRecorder();}") != std::string::npos ||
			General.find("if(Client()->State()==IClient::STATE_ONLINE)Client()->DemoRecorder_UpdateReplayRecorder();") != std::string::npos);
		CHECK(General.find("DoScrollbarOption(&g_Config.m_ClReplayLength,") != std::string::npos);
		CHECK(General.find("Localize(\"Defaultlength\"),10,600,") != std::string::npos);
		const auto Replay = General.substr(General.find("caseC::REPLAY_LENGTH:"));
		CHECK(Replay.find("SCROLLBAR_OPTION_NOCLAMPVALUE") != std::string::npos);
		CHECK(Replay.find("SCROLLBAR_OPTION_INFINITE") == std::string::npos);
		CHECK(General.find("g_Config.m_ClAutoRaceRecord^=1") != std::string::npos);
		CHECK(General.find("g_Config.m_ClRefreshRate=g_Config.m_ClRefreshRate>480||g_Config.m_ClRefreshRate==0?480:0") != std::string::npos);
		CHECK(General.find("SCROLLBAR_OPTION_DELAYUPDATE") != std::string::npos && General.find("SCROLLBAR_OPTION_INFINITE") != std::string::npos);
		for(const char *pId : {"s_SettingsButtonId", "s_SavesButtonId", "s_ConfigButtonId"})
			CHECK(General.find("staticCButtonContainer" + std::string(pId) + ";") != std::string::npos);
		const auto Renderer = Source("src/game/client/lucent/settings/general.cpp");
		CHECK(Renderer.find("HOTKEY_ESCAPE") == std::string::npos);
		CHECK(Renderer.find("ClipEnable(&Visible)") != std::string::npos && Renderer.find("State.Interactive(Control)") != std::string::npos);
		CHECK(Renderer.find("ClearKeyboardFocus()") != std::string::npos && Source("src/game/client/lucent/settings/mode_render.h").find("Mode.DrawOutline(") != std::string::npos);
		CHECK(Source("src/game/client/lucent/settings/mode_render.h").find("m_HeaderHover[i].Update(pUi->Enabled()&&pUi->MouseHovered(&Mode)") != std::string::npos);
		CHECK(Source("src/game/client/lucent/settings/mode_render.h").find("m_HeaderFocus[i].Update(pUi->Enabled()&&Presentation.m_ModeFocused") != std::string::npos);
		CHECK(Renderer.find("s_ModeButton") == std::string::npos && Renderer.find("StackHeader") == std::string::npos);
		CHECK(Renderer.find("constboolVisualEnabled=pUi->Enabled();") < Renderer.find("pUi->ClipEnable(&Visible)"));
		CHECK(Renderer.find("ActivateFile,VisualEnabled,pValue)") != std::string::npos);

		const auto Sound = Source("src/game/client/components/menus_settings_sound.cpp");
		for(const char *pSymbol : {"SndEnable", "SndGame", "SndGun", "SndMusic", "SndVolume", "SndGameVolume", "SndChatVolume", "SndMapVolume", "SndBackgroundMusicVolume", "SndNonactiveMute", "SndLongPain", "SndServerMessage", "SndChat", "SndTeamChat", "SndHighlight"})
			CHECK(Sound.find("&g_Config.m_" + std::string(pSymbol)) != std::string::npos);
		CHECK(Sound.find("0,100,&CUi::ms_LogarithmicScrollbarScale") != std::string::npos);
		CHECK(Sound.find("Control==C::ENABLE||Control==C::MUSIC)UpdateMusicState()") != std::string::npos);
		CHECK(Sound.find("m_NeedRestartSound=g_Config.m_SndEnable&&!Sound()->IsSoundEnabled()") != std::string::npos);
		CHECK(Sound.find("Ui()->Enabled()?pConfig:&InactiveValue") != std::string::npos);
		const auto SoundRenderer = Source("src/game/client/lucent/settings/sound.cpp");
		CHECK(SoundRenderer.find("Mode.ContentInteractive()") != std::string::npos);
		CHECK(SoundRenderer.find("!Page.m_Scroll.RectClipped(Row)") != std::string::npos);
		CHECK(SoundRenderer.find("COpacityScopeFade(pTextRender,Mode.m_ModeReveal.m_Value)") != std::string::npos);
		CHECK(SoundRenderer.find("RenderModeHeader(") != std::string::npos);
		CHECK(Renderer.find("RenderModeHeader(") != std::string::npos);
		const auto Controls = Source("src/game/client/lucent/ui/controls.cpp");
		CHECK(Controls.find("returnCheckBox(pUi,pId,pText,pMark,Rect,Flags,ControlRight,pUi->Enabled())") != std::string::npos);
		CHECK(Controls.find("constfloatActive=VisualEnabled?Highlight(") != std::string::npos);
		CHECK(Controls.find("VisualEnabled?LucentStyle::Style::CheckAlpha") != std::string::npos);
		const auto Slider = Source("src/game/client/lucent/ui/scrollbar.cpp");
		CHECK(Slider.find("CSliderGeometry::Radius(Geometry.m_Fill)") != std::string::npos);
		std::puts("General transferred bindings/IDs, ranges, callbacks, navigation and clipping source contracts passed");
	}

	std::puts("Lucent settings layout/navigation checks passed");
}
