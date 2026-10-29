#include "player.h"

#include "mode_render.h"
#include "player_countries.h"

#include <base/str.h>

#include <engine/font_icons.h>

#include <game/client/components/countryflags.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <vector>

namespace LucentSettings
{
	namespace
	{
		struct CPlayerView : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			CCountryFlags *m_pFlags = nullptr;
			CPlayerBindings m_Bindings;
			CModePresentation m_Mode;
			std::array<CButtonContainer, 2> m_Modes;
			CButtonContainer m_CountryButton, m_ResetButton;
			std::array<CLineInput, 2> m_Names, m_Clans;
			CLineInputBuffered<128> m_Search;
			CListBox m_List;
			CScrollRegion m_Scroll;
			int m_FieldFocus = -1, m_PopupProfile = 0, m_HighlightCountry = -1;
			float m_ListWidth = -1, m_ListHeight = -1;
			bool m_PrepareList = true;
			bool m_PopupWasOpen = false, m_FocusSearch = false, m_GridFocus = false;
		};
		CPlayerView &PlayerView()
		{
			static CPlayerView s_View;
			return s_View;
		}
		const char *CountryName(const CCountryFlags::CCountryFlag &Flag)
		{
			const char *pName = PlayerCountryName(Flag.m_aCountryCodeString);
			return pName ? Localize(pName) : Flag.m_aCountryCodeString;
		}
		const CCountryFlags::CCountryFlag &CurrentFlag(CPlayerView &Page, int Profile)
		{
			for(size_t i = 0; i < Page.m_pFlags->Num(); ++i)
				if(Page.m_pFlags->GetByIndex(i).m_CountryCode == *Page.m_Bindings.m_Profiles[Profile].m_pCountry)
					return Page.m_pFlags->GetByIndex(i);
			return Page.m_pFlags->GetByCountryCode(CountryCode::DEFAULT);
		}
		void Text(CPlayerView &Page, CUIRect Rect, const char *pText, ColorRGBA Color, int Align = TEXTALIGN_ML, bool Elide = false)
		{
			const auto Previous = Page.m_pTextRender->GetTextColor();
			Page.m_pTextRender->TextColor(Color);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EnableWidthCheck = false;
			Props.m_StopAtEnd = Props.m_EllipsisAtEnd = Elide;
			Page.m_pUi->DoLabel(&Rect, pText, 11, Align, Props);
			Page.m_pTextRender->TextColor(Previous);
		}
		void FlushInputs(CPlayerView &Page)
		{
			for(int Profile = 0; Profile < 2; ++Profile)
			{
				const bool NameChanged = Page.m_Names[Profile].WasChanged();
				const bool ClanChanged = Page.m_Clans[Profile].WasChanged();
				if((NameChanged || ClanChanged) && Page.m_Bindings.m_Changed)
					Page.m_Bindings.m_Changed(bool(Profile));
			}
		}
		void BlurInputs(CPlayerView &Page)
		{
			for(auto &Input : Page.m_Names)
				Input.Deactivate();
			for(auto &Input : Page.m_Clans)
				Input.Deactivate();
		}
		void FocusField(CPlayerView &Page, int Field, int Profile)
		{
			BlurInputs(Page);
			Page.m_Mode.ClearKeyboardFocus();
			Page.m_FieldFocus = Field;
			const void *pFocus = &Page.m_CountryButton;
			if(Field == 0)
				pFocus = &Page.m_Names[Profile];
			if(Field == 1)
				pFocus = &Page.m_Clans[Profile];
			Page.m_pUi->SetActiveItem(pFocus);
			Page.m_pUi->SetActiveItem(nullptr);
			if(Field < 2)
				(Field == 0 ? Page.m_Names[Profile] : Page.m_Clans[Profile]).Activate(EInputPriority::UI);
		}
		void DrawFlag(CPlayerView &Page, const CCountryFlags::CCountryFlag &Flag, CUIRect Region)
		{
			const auto Rect = CPlayerLayout::FitFlag(Region);
			Page.m_pFlags->Render(Flag, ColorRGBA(1, 1, 1, CUIRect::DrawOpacity()), Rect.x, Rect.y, Rect.w, Rect.h);
		}
		void RenderPreview(CPlayerView &Page, const CPlayerLayout &Layout, int Profile)
		{
			Text(Page, Layout.m_PreviewHeading, Localize("Scoreboard preview"), LucentStyle::Style::MutedText);
			const auto Row = Layout.m_PreviewRow;
			Row.Draw(LucentStyle::Style::UtilitySurface.WithAlpha(.35f), IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
			Row.Draw(LucentStyle::Style::Text.WithAlpha(.12f), IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
			const auto Parts = CPlayerPreviewLayout::Calculate(Row);
			const auto &Identity = Page.m_Bindings.m_Profiles[Profile];
			if(Page.m_Bindings.m_RenderTee)
				Page.m_Bindings.m_RenderTee(bool(Profile), Parts.m_Tee, CUIRect::DrawOpacity());
			Text(Page, Parts.m_Name, Identity.DisplayName(), LucentStyle::Style::Text, TEXTALIGN_ML, true);
			Text(Page, Parts.m_Clan, Identity.m_pClan, LucentStyle::Style::Text, TEXTALIGN_MC, true);
			DrawFlag(Page, CurrentFlag(Page, Profile), Parts.m_Flag);
		}
		CUi::EPopupMenuFunctionResult FlagPopup(void *pContext, CUIRect View, bool Active)
		{
			auto &Page = *static_cast<CPlayerView *>(pContext);
			auto *pUi = Page.m_pUi;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);

			if(Active && pUi->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
			{
				Page.m_Search.Deactivate();
				return CUi::POPUP_CLOSE_CURRENT;
			}
			const auto Layout = CPlayerGalleryLayout::Calculate(View);
			Text(Page, Layout.m_Title, Localize("Country selection"), LucentStyle::Style::Text);
			if(Page.m_FocusSearch && Active)
			{
				pUi->SetActiveItem(&Page.m_Search);
				pUi->SetActiveItem(nullptr);
				Page.m_Search.Activate(EInputPriority::UI);
				Page.m_FocusSearch = false;
			}
			if(Active && !pUi->Input()->HasComposition() && !pUi->Input()->GetCandidateCount())
			{
				if(pUi->ConsumeHotkey(CUi::HOTKEY_TAB) || (Page.m_Search.IsActive() && pUi->ConsumeHotkey(CUi::HOTKEY_DOWN)))
				{
					Page.m_GridFocus = !Page.m_GridFocus;
					if(Page.m_GridFocus)
					{
						Page.m_Search.Deactivate();
						pUi->SetActiveItem(&Page.m_CountryButton);
						pUi->SetActiveItem(nullptr);
					}
					else
					{
						pUi->SetActiveItem(&Page.m_Search);
						pUi->SetActiveItem(nullptr);
						Page.m_Search.Activate(EInputPriority::UI);
					}
				}
			}
			bool Changed;
			{
				LucentUi::CInteractionScope SearchInput(pUi, !Page.m_GridFocus);
				Changed = pUi->DoEditBox_Search(&Page.m_Search, &Layout.m_Search, 11, Active);
			}
			// Clicking the search field or Ctrl+F returns ownership from the grid.
			if(Active && ((pUi->MouseButtonClicked(0) && pUi->MouseHovered(&Layout.m_Search)) ||
					     (pUi->Input()->ModifierIsPressed() && pUi->Input()->KeyPress(KEY_F))))
			{
				Page.m_GridFocus = false;
				pUi->SetActiveItem(&Page.m_Search);
				pUi->SetActiveItem(nullptr);
				Page.m_Search.Activate(EInputPriority::UI);
			}
			if(Changed)
			{
				Page.m_PrepareList = true;
				Page.m_List.Reset();
				Page.m_List.ScrollToSelected();
				Page.m_HighlightCountry = *Page.m_Bindings.m_Profiles[Page.m_PopupProfile].m_pCountry;
			}
			Text(Page, Layout.m_Hint, Localize("Search by country name or code"), LucentStyle::Style::MutedText, TEXTALIGN_ML, true);
			std::vector<size_t> vFlags;
			int Selected = -1;
			for(size_t i = 0; i < Page.m_pFlags->Num(); ++i)
			{
				const auto &Flag = Page.m_pFlags->GetByIndex(i);
				const char *pName = PlayerCountryName(Flag.m_aCountryCodeString);
				char aNumeric[16];
				str_format(aNumeric, sizeof(aNumeric), "%d", Flag.m_CountryCode);
				if(!PlayerCountryMatches(Flag.m_aCountryCodeString, pName ? pName : Flag.m_aCountryCodeString,
					   CountryName(Flag), aNumeric, Page.m_Search.GetString(), [](const char *pText, const char *pQuery) { return str_utf8_find_nocase(pText, pQuery) != nullptr; }))
					continue;
				vFlags.push_back(i);
			}

			if(Page.m_Search.IsEmpty())
				PlayerDefaultFirst(vFlags, [&](size_t Index) { return Page.m_pFlags->GetByIndex(Index).m_CountryCode == CountryCode::DEFAULT; });
			// Selection and scroll positioning must use the displayed order.
			for(size_t i = 0; i < vFlags.size(); ++i)
				if(Page.m_pFlags->GetByIndex(vFlags[i]).m_CountryCode == Page.m_HighlightCountry)
					Selected = int(i);

			if(Active && Page.m_Search.IsActive() && !pUi->Input()->HasComposition() && !pUi->Input()->GetCandidateCount() && pUi->ConsumeHotkey(CUi::HOTKEY_ENTER) && !vFlags.empty())
			{
				const int Index = Selected >= 0 ? Selected : 0;
				Page.m_Bindings.SelectCountry(bool(Page.m_PopupProfile), Page.m_pFlags->GetByIndex(vFlags[Index]).m_CountryCode);
				Page.m_Search.Deactivate();
				return CUi::POPUP_CLOSE_CURRENT;
			}
			if(Page.m_PrepareList || Page.m_ListWidth != Layout.m_Grid.w || Page.m_ListHeight != Layout.m_Grid.h)
			{
				Page.m_List.Reset();
				Page.m_List.SetColors(LucentUi::Accent(0), LucentUi::Accent(0));
				Page.m_List.SetScrollbarWidth(8);
				Page.m_List.SetScrollbarMargin(1);
				Page.m_List.ScrollToSelected();
				Page.m_List.SetActive(false);
				// Resolve the list's real content size and selected-row offset before painting.
				LucentUi::CInteractionScope NoInput(pUi, false);
				LucentUi::COpacityScope NoPaint(Page.m_pTextRender, 0);
				Page.m_List.DoStart(CPlayerGalleryLayout::CellHeight, int(vFlags.size()), Layout.m_Columns, 3, Selected, &Layout.m_Grid, false);
				for(size_t i = 0; i < vFlags.size(); ++i)
					Page.m_List.DoNextItem(&Page.m_pFlags->GetByIndex(vFlags[i]).m_CountryCode, int(i) == Selected, LucentUi::RADIUS);
				Page.m_List.DoEnd();
				Page.m_PrepareList = false;
				Page.m_ListWidth = Layout.m_Grid.w;
				Page.m_ListHeight = Layout.m_Grid.h;
			}
			Page.m_List.SetColors(LucentUi::Accent(0), LucentUi::Accent(0));
			Page.m_List.SetScrollbarWidth(8);
			Page.m_List.SetScrollbarMargin(1);
			Page.m_List.SetActive(Active && Page.m_GridFocus);
			Page.m_List.DoStart(CPlayerGalleryLayout::CellHeight, int(vFlags.size()), Layout.m_Columns, 3, Selected, &Layout.m_Grid, false);
			for(size_t i = 0; i < vFlags.size(); ++i)
			{
				const auto &Flag = Page.m_pFlags->GetByIndex(vFlags[i]);
				const auto Item = Page.m_List.DoNextItem(&Flag.m_CountryCode, int(i) == Selected, LucentUi::RADIUS);
				const float Hot = LucentUi::Highlight(&Flag.m_CountryCode, Active && pUi->MouseHovered(&Item.m_Rect));
				const float Chosen = LucentUi::Highlight(&Flag.m_CountryCode, Flag.m_CountryCode == *Page.m_Bindings.m_Profiles[Page.m_PopupProfile].m_pCountry, LucentUi::EChannel::ACTIVE);
				if(!Item.m_Visible)
					continue;
				Item.m_Rect.Draw(LucentUi::Accent(LucentStyle::Style::Hover * Hot + LucentStyle::Style::Selected * Chosen), IGraphics::CORNER_ALL, LucentUi::RADIUS);
				if(Item.m_Selected && Page.m_GridFocus)
					LucentUi::Surface(Item.m_Rect, true, Active && pUi->MouseHovered(&Item.m_Rect));
				const auto Parts = CPlayerCountryListRowLayout::Calculate(Item.m_Rect);
				DrawFlag(Page, Flag, Parts.m_Flag);
				Text(Page, Parts.m_Name, CountryName(Flag), LucentStyle::Style::Text, TEXTALIGN_ML, true);
				Text(Page, Parts.m_Code, Flag.m_aCountryCodeString, LucentStyle::Style::MutedText, TEXTALIGN_MR, true);
				pUi->DoToolTip(&Flag.m_CountryCode, &Item.m_Rect, CountryName(Flag));
			}
			const int NewSelected = Page.m_List.DoEnd();
			if(NewSelected >= 0 && NewSelected < int(vFlags.size()))
			{
				const int Country = Page.m_pFlags->GetByIndex(vFlags[NewSelected]).m_CountryCode;
				Page.m_HighlightCountry = Country;
				if(Active && (Page.m_List.WasItemSelected() || Page.m_List.WasItemActivated()))
				{
					Page.m_Bindings.SelectCountry(bool(Page.m_PopupProfile), Country);
					Page.m_Search.Deactivate();
					return CUi::POPUP_CLOSE_CURRENT;
				}
			}
			if(vFlags.empty())
			{
				CUIRect Message, Reset = Layout.m_Grid;
				Reset.HSplitTop(std::min(48.f, Reset.h), &Message, &Reset);
				Reset.HSplitTop(std::min(28.f, Reset.h), &Reset, nullptr);
				Text(Page, Message, Localize("No country flags match your filter criteria"), LucentStyle::Style::MutedText, TEXTALIGN_MC);
				if(LucentUi::TextButton(pUi, Page.m_pTextRender, &Page.m_ResetButton, Localize("Reset filter"), Reset))
				{
					Page.m_Search.Clear();
					Page.m_PrepareList = true;
					Page.m_List.Reset();
					Page.m_FocusSearch = true;
				}
			}
			return CUi::POPUP_KEEP_OPEN;
		}
		void OpenFlags(CPlayerView &Page, CUIRect /*Anchor*/, int Profile)
		{
			auto *pUi = Page.m_pUi;
			if(!pUi->Enabled() || pUi->IsPopupOpen())
				return;
			FlushInputs(Page);
			BlurInputs(Page);
			Page.m_PopupProfile = Profile;
			Page.m_PrepareList = true;
			Page.m_HighlightCountry = *Page.m_Bindings.m_Profiles[Profile].m_pCountry;
			Page.m_List.Reset();
			Page.m_List.ScrollToSelected();
			Page.m_FocusSearch = true;
			Page.m_GridFocus = false;
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			const auto Popup = CPlayerGalleryLayout::Popup(*pUi->Screen());
			pUi->DoPopupMenu(&Page, Popup.x, Popup.y, Popup.w, Popup.h, &Page, FlagPopup, Props);
			Page.m_PopupWasOpen = true;
		}
	}
	void LeavePlayer(CUi *pUi)
	{
		auto &Page = PlayerView();
		FlushInputs(Page);
		BlurInputs(Page);
		Page.m_Search.Deactivate();
		if(pUi->IsPopupOpen(&Page))
			pUi->ClosePopupMenu(&Page, true);
		Page.m_PopupWasOpen = false;
		Page.m_FieldFocus = -1;
		Page.m_Mode.Leave();
		for(auto &Button : Page.m_Modes)
		{
			if(pUi->CheckActiveItem(&Button))
				pUi->SetActiveItem(nullptr);
			if(pUi->HotItem() == &Button)
				pUi->SetHotItem(nullptr);
		}
	}
	void RenderPlayer(CUi *pUi, ITextRender *pTextRender, CUIRect View, CCountryFlags *pFlags, bool &Dummy, const CPlayerBindings &Bindings, bool ConsoleActive)
	{
		auto &Page = PlayerView();
		Page.m_pUi = pUi;
		Page.m_pTextRender = pTextRender;
		Page.m_pFlags = pFlags;
		// Copy durable config pointers and the owner callback, never a stack-captured popup lambda.
		Page.m_Bindings = Bindings;
		for(int i = 0; i < 2; ++i)
		{
			Page.m_Names[i].SetBuffer(Bindings.m_Profiles[i].m_pName, Bindings.m_Profiles[i].m_NameSize);
			Page.m_Names[i].SetEmptyText(Bindings.m_Profiles[i].m_pEmptyName);
			Page.m_Clans[i].SetBuffer(Bindings.m_Profiles[i].m_pClan, Bindings.m_Profiles[i].m_ClanSize);
		}
		LucentUi::CScope Style(pUi);
		const bool PopupOpen = pUi->IsPopupOpen(&Page);
		if(Page.m_PopupWasOpen && !PopupOpen)
		{
			Page.m_Search.Deactivate();
			FocusField(Page, 2, int(Dummy));
			Page.m_PopupWasOpen = false;
		}
		LucentUi::CInteractionScope PopupInput(pUi, !pUi->IsPopupOpen() && !ConsoleActive);
		auto &Mode = Page.m_Mode;
		const auto &Clock = LucentStyle::Motion::Clock();
		if(Mode.m_LastFrame < 0)
			Mode.m_Advanced = Dummy;
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		if(!pUi->Enabled())
			Mode.ClearKeyboardFocus();
		else if(!pUi->Input()->HasComposition() && !pUi->Input()->GetCandidateCount())
		{
			if(pUi->Input()->KeyPress(KEY_TAB))
			{
				if(Page.m_Names[int(Dummy)].IsActive())
				{
					if(pUi->Input()->ShiftIsPressed())
					{
						BlurInputs(Page);
						pUi->SetActiveItem(&Page.m_Modes[0]);
						pUi->SetActiveItem(nullptr);
						Mode.FocusMode();
						Page.m_FieldFocus = -1;
					}
					else
						FocusField(Page, 1, int(Dummy));
				}
				else if(Page.m_Clans[int(Dummy)].IsActive())
					FocusField(Page, pUi->Input()->ShiftIsPressed() ? 0 : 2, int(Dummy));
				else
				{
					Page.m_FieldFocus = -1;
					Mode.FocusMode();
				}
			}
			if(Mode.m_ModeFocused)
			{
				if(pUi->ConsumeHotkey(CUi::HOTKEY_LEFT))
					Mode.m_FocusedAdvanced = false;
				if(pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT))
					Mode.m_FocusedAdvanced = true;
				if(Mode.ContentInteractive() && pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
					FocusField(Page, 0, int(Dummy));
			}
		}
		if(pUi->Enabled() && pUi->MouseButtonClicked(0))
		{
			Mode.ClearKeyboardFocus();
			Page.m_FieldFocus = -1;
		}
		const bool PreviousTarget = Mode.m_Advanced;
		const auto Header = RenderModeHeader(pUi, pTextRender, CPlayerLayout::ContentRegion(View), Mode, Page.m_Modes,
			pUi->Enabled() && !pUi->ActiveItem() && !CLineInput::GetActiveInput(), Localize("Player"), Localize("Dummy"));
		if(PreviousTarget != Mode.m_Advanced)
		{
			FlushInputs(Page);
			BlurInputs(Page);
			Page.m_FieldFocus = -1;
		}
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		Dummy = Mode.m_ShownAdvanced;
		const int Profile = int(Dummy);
		const auto &Flag = CurrentFlag(Page, Profile);
		View.y = Header.m_Content.y;
		View.h = Header.m_Content.h;
		const char *apLabels[] = {Localize("Name"), Localize("Clan"), Localize("Country"), Bindings.m_Profiles[Profile].m_Cooldown.c_str(), Localize("Scoreboard preview")};
		const auto Calculate = [&]() { return CPlayerLayout::Calculate(View, [&](int Id, float Width) { return pTextRender->TextBoundingBox(11, apLabels[Id], -1, Width).m_H; }); };
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		Params.m_ScrollUnit = 54;
		const CUIRect Original = View;
		View.w = std::max(0.f, View.w - Params.m_ScrollbarThickness);
		const float Height = Calculate().m_Height;
		View = Original;
		Page.m_Scroll.Begin(&View, &Params, Height);
		const auto Layout = Calculate();
		Page.m_Scroll.AddRect({View.x, View.y + Layout.m_Height, View.w, 1});
		LucentUi::COpacityScope Fade(pTextRender, Mode.m_ModeReveal.m_Value);
		LucentUi::CInteractionScope BodyInput(pUi, Mode.ContentInteractive());
		Text(Page, Layout.m_NameLabel, apLabels[0], LucentStyle::Style::MutedText);
		Text(Page, Layout.m_ClanLabel, apLabels[1], LucentStyle::Style::MutedText);
		Text(Page, Layout.m_CountryLabel, apLabels[2], LucentStyle::Style::MutedText);
		const auto Edit = [&](CLineInput &Input, CUIRect Rect, int Field) {
			if(Input.IsActive())
				Page.m_FieldFocus = Field;
			Page.m_Scroll.AddRect(Rect, Input.IsActive());
			LucentUi::CInteractionScope Visible(pUi, !Page.m_Scroll.RectClipped(Rect));
			if(pUi->DoEditBox(&Input, &Rect, 11))
				Bindings.m_Changed(Dummy);
		};
		Edit(Page.m_Names[Profile], Layout.m_Name, 0);
		Edit(Page.m_Clans[Profile], Layout.m_Clan, 1);
		Text(Page, Layout.m_Cooldown, apLabels[3], LucentStyle::Style::MutedText);
		RenderPreview(Page, Layout, Profile);
		const auto CountryRow = CPlayerCountryRowLayout::Calculate(Layout.m_Country);
		LucentUi::InputFrame(Layout.m_Country, &Page.m_CountryButton, true, Page.m_FieldFocus == 2 || PopupOpen, pUi->MouseHovered(&Layout.m_Country), pUi->Enabled(), IGraphics::CORNER_ALL);
		DrawFlag(Page, Flag, CountryRow.m_Flag);
		Text(Page, CountryRow.m_Name, CountryName(Flag), LucentStyle::Style::Text, TEXTALIGN_ML, true);
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		Text(Page, CountryRow.m_Arrow, FontIcon::CHEVRON_DOWN, LucentStyle::Style::MutedText, TEXTALIGN_MC);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		if(pUi->Enabled())
			pUi->DoToolTip(&Page.m_CountryButton, &Layout.m_Country, CountryName(Flag));
		bool Open = false;
		{
			LucentUi::CInteractionScope Visible(pUi, !Page.m_Scroll.RectClipped(Layout.m_Country));
			Open = pUi->DoButtonLogic(&Page.m_CountryButton, 0, &Layout.m_Country, BUTTONFLAG_LEFT);
			if(pUi->Enabled() && Page.m_FieldFocus == 2)
			{
				Page.m_Scroll.AddRect(Layout.m_Country, true);
				Open |= pUi->ConsumeHotkey(CUi::HOTKEY_ENTER) || pUi->Input()->KeyPress(KEY_SPACE);
			}
		}
		const bool CanOpen = pUi->Enabled();
		Page.m_Scroll.End();
		if(Open && CanOpen)
			OpenFlags(Page, Layout.m_Country, Profile);
	}
}
