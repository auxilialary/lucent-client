#include "general.h"

#include "mode_render.h"

#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/keys.h>

#include <game/client/components/menu_background.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <string>
#include <type_traits>

namespace LucentSettings
{
	namespace
	{
		struct CGeneralViewState
		{
			CGeneralPresentation m_Presentation;
			std::array<CButtonContainer, 2> m_ModeButtons;
		};
		CGeneralViewState &GeneralViewState()
		{
			static CGeneralViewState s_State;
			return s_State;
		}
		constexpr float FontSize = 11;
		using C = EGeneralControl;
		const char *Label(C Control)
		{
			static const char *const s_apLabels[GeneralControlCount] = {
				"Dynamic Camera", "Smooth Dynamic Camera", "Switch weapon on pickup", "Switch weapon when out of ammo",
				"Skip the main menu", "Update Rate", "Save power by lowering update rate (higher input latency)",
				"Settings file", "Saves file", "Config directory", "Automatically record demos", "Max demos",
				"Automatically take game over screenshot", "Max Screenshots", "Automatically take statboard screenshot", "Max Screenshots",
				"Automatically create statboard csv", "Max CSVs",
				"Default zoom", "Save the best demo of each race", "Enable replays", "Default length"};
			return Localize(s_apLabels[int(Control)]);
		}
		void Text(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size, ColorRGBA Color, bool Elide = false)
		{
			const auto Previous = pTextRender->GetTextColor();
			pTextRender->TextColor(Color);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EnableWidthCheck = false;
			Props.m_EllipsisAtEnd = Elide;
			pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_ML, Props);
			pTextRender->TextColor(Previous);
		}
		void ThemeName(const CTheme &Theme, char *pName, size_t Size)
		{
			if(Theme.m_Name.empty())
				str_copy(pName, "(none)", Size);
			else if(Theme.m_Name == "auto")
				str_copy(pName, "(seasons)", Size);
			else if(Theme.m_Name == "rand")
				str_copy(pName, "(random)", Size);
			else if(Theme.m_HasDay && !Theme.m_HasNight)
				str_format(pName, Size, "%s (day)", Theme.m_Name.c_str());
			else if(!Theme.m_HasDay && Theme.m_HasNight)
				str_format(pName, Size, "%s (night)", Theme.m_Name.c_str());
			else
				str_copy(pName, Theme.m_Name.c_str(), Size);
		}
		void Thumbnail(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const CTheme *pTheme)
		{
			if(pTheme && pTheme->m_IconTexture.IsValid())
			{
				auto *pGraphics = pUi->Graphics();
				pGraphics->TextureSet(pTheme->m_IconTexture);
				pGraphics->QuadsBegin();
				pGraphics->SetColor(1, 1, 1, 1);
				IGraphics::CQuadItem Quad(Rect.x, Rect.y, Rect.w, Rect.h);
				pGraphics->QuadsDrawTL(&Quad, 1);
				pGraphics->QuadsEnd();
			}
			else
			{
				pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
				Text(pUi, pTextRender, Rect, FontIcon::IMAGE, 13, LucentStyle::Style::MutedText);
				pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
			}
		}
		struct CThemePopup : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			const std::vector<CTheme> *m_pThemes = nullptr;
			FSelectGeneralTheme m_Select;
			CListBox m_List;
			std::string m_ConfiguredName, m_HighlightName;
			bool m_HasConfiguredTheme = false;
		};
		CUi::EPopupMenuFunctionResult ThemePopup(void *pContext, CUIRect View, bool Active)
		{
			auto &Context = *static_cast<CThemePopup *>(pContext);
			auto *pUi = Context.m_pUi;
			auto *pTextRender = Context.m_pTextRender;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			// Store the live vector object, never a snapshot of entries or their indices.
			// Resource rescans may replace its entries while the popup remains open.
			const auto &vThemes = *Context.m_pThemes;
			int Selected = -1;
			for(int i = 0; i < int(vThemes.size()); ++i)
				if(vThemes[i].m_Name == Context.m_HighlightName)
					Selected = i;
			Context.m_List.SetColors(LucentUi::Accent(0), LucentUi::Accent(0));
			Context.m_List.SetScrollbarWidth(8);
			Context.m_List.SetScrollbarMargin(1);
			Context.m_List.SetActive(Active);
			Context.m_List.DoStart(32, vThemes.size(), 1, 3, Selected, &View, false);
			for(int i = 0; i < int(vThemes.size()); ++i)
			{
				const auto &Theme = vThemes[i];
				const auto Item = Context.m_List.DoNextItem(&Theme.m_Name, i == Selected, LucentUi::RADIUS);
				const float Hot = LucentUi::Highlight(&Theme.m_Name, pUi->MouseHovered(&Item.m_Rect));
				const float SelectedLight = LucentUi::Highlight(&Theme.m_Name, Context.m_HasConfiguredTheme && Theme.m_Name == Context.m_ConfiguredName, LucentUi::EChannel::ACTIVE, LucentStyle::Motion::Hover);
				if(!Item.m_Visible)
					continue;
				Item.m_Rect.Draw(LucentUi::Accent(LucentStyle::Style::Hover * Hot + LucentStyle::Style::Selected * SelectedLight), IGraphics::CORNER_ALL, LucentUi::RADIUS);
				if(Item.m_Selected && Active)
					LucentUi::Surface(Item.m_Rect, true, pUi->MouseHovered(&Item.m_Rect));
				CUIRect Marker = {Item.m_Rect.x, Item.m_Rect.y + 8, 2, 16};
				LucentUi::GradientFill(Marker, SelectedLight);
				Thumbnail(pUi, pTextRender, {Item.m_Rect.x + 8, Item.m_Rect.y + 7, 32, 18}, &Theme);
				char aName[128];
				ThemeName(Theme, aName, sizeof(aName));
				Text(pUi, pTextRender, {Item.m_Rect.x + 48, Item.m_Rect.y, std::max(0.f, Item.m_Rect.w - 54), 32}, aName, FontSize, LucentStyle::Style::Text, true);
				pUi->DoToolTip(&Theme.m_Name, &Item.m_Rect, Theme.m_Name.empty() ? "(none)" : Theme.m_Name.c_str());
			}
			const int NewSelected = Context.m_List.DoEnd();
			if(NewSelected >= 0 && NewSelected < int(vThemes.size()))
			{
				Context.m_HighlightName = vThemes[NewSelected].m_Name;
				if(Context.m_List.WasItemSelected() || Context.m_List.WasItemActivated())
				{
					if(!Context.m_HasConfiguredTheme || vThemes[NewSelected].m_Name != Context.m_ConfiguredName)
						Context.m_Select(vThemes[NewSelected]);
					return CUi::POPUP_CLOSE_CURRENT;
				}
			}
			if(vThemes.empty())
				Text(pUi, pTextRender, View, Localize("No results"), FontSize, LucentStyle::Style::MutedText);
			return CUi::POPUP_KEEP_OPEN;
		}
	}

	bool GeneralCheckbox(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pLabel, bool Checked, const CUIRect *pRect, bool VisualEnabled)
	{
		// Dim only the idle text RGB; shared hover/fill transitions retain the checked gradient.
		constexpr float IdleLabelBrightness = .87f;
		return LucentUi::FilledCheckBox(pUi, pId, pLabel, Checked, *pRect, VisualEnabled, true, IdleLabelBrightness);
	}

	bool GeneralFileButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pLabel, const CUIRect *pRect, bool Focused, bool Activate)
	{
		const bool Enabled = pUi->Enabled();
		if(!Enabled && pUi->CheckActiveItem(pId))
			pUi->SetActiveItem(nullptr);
		const bool Pressed = Enabled && (pUi->CheckActiveItem(pId) || Activate);
		const float Hot = LucentUi::Highlight(pId, Enabled && pUi->MouseHovered(pRect));
		const float Focus = LucentUi::Highlight(pId, Enabled && Focused, LucentUi::EChannel::FOCUS);
		pRect->Draw(LucentUi::Accent(LucentStyle::Style::Hover * Hot + LucentStyle::Style::Pressed * float(Pressed)), IGraphics::CORNER_ALL, LucentUi::RADIUS);
		if(Focus > 0)
		{
			const CUIRect Line = {pRect->x + 4, pRect->y + pRect->h - 2, std::max(0.f, pRect->w - 8), LucentStyle::Style::Hairline};
			Line.Draw(LucentUi::Accent(LucentStyle::Style::ButtonUnderline * Focus), 0, 0);
		}
		LucentUi::COpacityScope Opacity(pTextRender, Pressed ? LucentStyle::Style::PressedText : 1.f);
		return LucentUi::TextButton(pUi, pTextRender, pId, pLabel, *pRect, false, FontSize, TEXTALIGN_ML, false, FontIcon::FOLDER_OPEN) || (Enabled && Activate);
	}

	void LeaveGeneral(CUi *pUi)
	{
		auto &View = GeneralViewState();
		for(auto &Button : View.m_ModeButtons)
		{
			if(pUi->CheckActiveItem(&Button))
				pUi->SetActiveItem(nullptr);
			if(pUi->HotItem() == &Button)
				pUi->SetHotItem(nullptr);
		}
		View.m_Presentation.Leave();
	}

	void RenderGeneral(CUi *pUi, ITextRender *pTextRender, CUIRect View, const CGeneralState &Target, const FGeneralControl &RenderControl)
	{
		LucentUi::CScope Style(pUi);
		LucentUi::CInteractionScope PopupInput(pUi, !pUi->IsPopupOpen());
		static CScrollRegion s_Scroll;
		auto &HeaderState = GeneralViewState();
		auto &s_Presentation = HeaderState.m_Presentation;
		const auto &Clock = LucentStyle::Motion::Clock();
		auto State = s_Presentation.Update(Target, Clock.m_Now, Clock.m_Animate);
		const bool Keyboard = pUi->Enabled() && !pUi->ActiveItem() && !CLineInput::GetActiveInput();
		bool ScrollFocus = false;
		if(!pUi->Enabled() || CLineInput::GetActiveInput())
		{
			s_Presentation.m_FileFocus = -1;
			s_Presentation.m_ModeFocused = false;
		}
		else if(Keyboard)
		{
			// Rail owns Tab entry/exit. The gesture it releases to this page
			// focuses the selected heading; Escape remains owned by Settings navigation.
			if(pUi->Input()->KeyPress(KEY_TAB))
			{
				s_Presentation.FocusMode();
			}
			if(s_Presentation.m_ModeFocused)
			{
				if(pUi->ConsumeHotkey(CUi::HOTKEY_LEFT))
					s_Presentation.m_FocusedAdvanced = false;
				if(pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT))
					s_Presentation.m_FocusedAdvanced = true;
			}
			if(State.m_Advanced && State.m_ContentAlpha == 1 && (s_Presentation.m_ModeFocused || s_Presentation.m_FileFocus >= 0))
			{
				if(pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
				{
					if(s_Presentation.m_ModeFocused)
					{
						s_Presentation.m_ModeFocused = false;
						s_Presentation.m_FileFocus = 0;
					}
					else if(s_Presentation.m_FileFocus >= 0)
						s_Presentation.MoveFileFocus(1);
					ScrollFocus = s_Presentation.m_FileFocus >= 0;
				}
				if(pUi->ConsumeHotkey(CUi::HOTKEY_UP) && s_Presentation.m_FileFocus >= 0)
				{
					if(s_Presentation.m_FileFocus == 0)
					{
						s_Presentation.m_FileFocus = -1;
						s_Presentation.FocusMode();
					}
					else
						s_Presentation.MoveFileFocus(-1);
					ScrollFocus = s_Presentation.m_FileFocus >= 0;
				}
			}
		}
		if(pUi->Enabled() && pUi->MouseButtonClicked(0))
			s_Presentation.ClearKeyboardFocus();
		// Text-width hitboxes share the controls' content anchor. The header stays
		// outside the scroll/fade scope; only the existing content fade swaps modes.
		const auto Header = RenderModeHeader(pUi, pTextRender, CGeneralLayout::ContentRegion(View), s_Presentation, HeaderState.m_ModeButtons, Keyboard);
		State = s_Presentation.Update(Target, Clock.m_Now, Clock.m_Animate);
		// Keep the body's original horizontal anchors; only reserve header height.
		View.y = Header.m_Content.y;
		View.h = Header.m_Content.h;
		const bool ActivateFile = Keyboard && State.m_Advanced && State.m_ContentAlpha == 1 && s_Presentation.m_FileFocus >= 0 &&
					  (pUi->ConsumeHotkey(CUi::HOTKEY_ENTER) || pUi->Input()->KeyPress(KEY_SPACE));
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		Params.m_ScrollUnit = 54;
		Params.m_ForceShowScrollbar = true;
		bool Paint = false;
		const auto Height = [&](const char *pText, float Width, float Size = FontSize) {
			return pTextRender->TextBoundingBox(Size, pText, -1, std::max(1.f, Width)).m_H;
		};
		const auto Compose = [&]() {
			CUIRect Body = View;
			const auto Layout = CGeneralLayout::Calculate(Body);
			const auto SectionLabel = [](EGeneralSection Section) {
				switch(Section)
				{
				case EGeneralSection::GAME: return Localize("Game");
				case EGeneralSection::CLIENT: return Localize("Client");
				case EGeneralSection::CAPTURE: return Localize("Recordings & Results", "General settings");
				case EGeneralSection::FILES: return Localize("Files", "General settings");
				}
				return "";
			};
			const auto Measure = [&](auto Item, float Width) {
				if constexpr(std::is_same_v<decltype(Item), C>)
					return Height(Label(Item), Width);
				else
					return Height(SectionLabel(Item), Width);
			};
			const float BodyHeight = Layout.Compose(State, Measure, [&](C Control, CUIRect Row) {
				if(!Paint)
					return;
				const int FileIndex = int(Control) - int(C::SETTINGS_FILE);
				const bool File = FileIndex >= 0 && FileIndex < 3;
				if(File && Keyboard && pUi->MouseButtonClicked(0) && pUi->MouseHovered(&Row))
					s_Presentation.m_FileFocus = FileIndex;
				const bool Focused = File && s_Presentation.m_FileFocus == FileIndex;
				CUIRect Visible = Row;
				Visible.h *= State.m_Reveal[int(Control)];
				s_Scroll.AddRect(Visible, Focused && ScrollFocus);
				const bool IsSlider = Control == C::DEFAULT_ZOOM || Control == C::REPLAY_LENGTH || Control == C::UPDATE_RATE ||
					Control == C::MAX_DEMOS || Control == C::MAX_SCREENSHOTS || Control == C::MAX_STATBOARD || Control == C::MAX_CSV;
				CUIRect Caption, Value, Rail = Row;
				if(IsSlider)
					CGeneralLayout::SliderRegions(Row, Caption, Value, Rail);
				const CUIRect *pValue = IsSlider ? &Value : nullptr;
				if(s_Scroll.RectClipped(Visible))
				{
					// Keep active-item cleanup, using the viewport's existing clip.
					LucentUi::CInteractionScope Input(pUi, false);
					RenderControl(Control, Rail, false, false, false, pValue);
					return;
				}
				const bool VisualEnabled = pUi->Enabled();
				pUi->ClipEnable(&Visible);
				LucentUi::CInteractionScope Input(pUi, !s_Scroll.RectClipped(Visible) && State.Interactive(Control));
				LucentUi::COpacityScope Opacity(pTextRender, State.m_Reveal[int(Control)]);
				if(IsSlider)
					Text(pUi, pTextRender, Caption, Label(Control), FontSize, LucentStyle::Style::Text);
				RenderControl(Control, Rail, Focused, Focused && ActivateFile, VisualEnabled, pValue);
				pUi->ClipDisable(); }, [&](EGeneralSection Section, CUIRect Row) {
				if(Paint)
					Text(pUi, pTextRender, Row, SectionLabel(Section), FontSize, LucentStyle::Style::MutedText); });
			const float Bottom = Body.y + BodyHeight;
			if(Paint)
				s_Scroll.AddRect({View.x, Bottom, View.w, 1});
			return Bottom - View.y + 1;
		};
		const CUIRect Original = View;
		View.w = std::max(0.f, View.w - Params.m_ScrollbarThickness);
		const float ContentHeight = Compose();
		View = Original;
		s_Scroll.Begin(&View, &Params, ContentHeight);
		Paint = true;
		{
			LucentUi::COpacityScope ModeOpacity(pTextRender, State.m_ContentAlpha);
			Compose();
		}
		s_Scroll.End();
	}

	void GeneralThemes(CUi *pUi, ITextRender *pTextRender, CUIRect View, const std::vector<CTheme> &vThemes, int Selected, const FSelectGeneralTheme &SelectTheme)
	{
		static CButtonContainer s_Selector;
		static CThemePopup s_Popup;
		const CTheme *pTheme = Selected >= 0 && Selected < int(vThemes.size()) ? &vThemes[Selected] : nullptr;
		// Refresh the live source/callback even while open; never retain a stack lambda
		// or an entry pointer from a previous theme scan.
		s_Popup.m_pUi = pUi;
		s_Popup.m_pTextRender = pTextRender;
		s_Popup.m_pThemes = &vThemes;
		s_Popup.m_Select = SelectTheme;
		s_Popup.m_ConfiguredName = pTheme ? pTheme->m_Name : "";
		s_Popup.m_HasConfiguredTheme = pTheme != nullptr;
		const bool Open = pUi->IsPopupOpen(&s_Popup);
		const bool Hover = pUi->MouseHovered(&View);
		const bool Pressed = pUi->CheckActiveItem(&s_Selector);
		LucentUi::InputFrame(View, &s_Selector, false, Pressed || Open, Hover, true, IGraphics::CORNER_ALL);
		Thumbnail(pUi, pTextRender, {View.x + 9, View.y + (View.h - 18) / 2, 32, 18}, pTheme);
		char aName[128];
		if(pTheme)
			ThemeName(*pTheme, aName, sizeof(aName));
		else
			str_copy(aName, "(none)");
		Text(pUi, pTextRender, {View.x + 48, View.y, std::max(0.f, View.w - 76), View.h}, aName, FontSize, LucentStyle::Style::Text, true);
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		Text(pUi, pTextRender, {View.x + View.w - 22, View.y, 14, View.h}, FontIcon::CHEVRON_DOWN, 10, LucentStyle::Style::MutedText);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		if(pUi->DoButtonLogic(&s_Selector, 0, &View, BUTTONFLAG_LEFT) && !pUi->IsPopupOpen())
		{
			s_Popup.m_HighlightName = s_Popup.m_ConfiguredName;
			s_Popup.m_List.Reset();
			s_Popup.m_List.ScrollToSelected();
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			const float Width = std::min(380.f, pUi->Screen()->w - 12);
			const float Height = std::min(280.f, pUi->Screen()->h - 12);
			pUi->DoPopupMenu(&s_Popup, View.x, View.y + View.h + 4, Width, Height, &s_Popup, ThemePopup, Props);
		}
		if(pUi->Enabled())
			pUi->DoToolTip(&s_Selector, &View, pTheme && !pTheme->m_Name.empty() ? pTheme->m_Name.c_str() : Localize("Theme"));
	}
}
