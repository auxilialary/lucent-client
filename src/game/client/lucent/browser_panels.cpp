#include "ui.h"

#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <array>

// Content is rendered in its owning region; no generic sliding surface.
void CMenus::RenderLucentContext(CUIRect View, CLucentBrowserState::EPanel Mode)
{
	using EPanel = CLucentBrowserState::EPanel;
	if(Mode == EPanel::DIRECT_CONNECT)
	{
		CUIRect Address, Action;
		View.HSplitTop(26, &Address, &View);
		if(m_LucentBrowser.m_DirectFocusPending && Ui()->Enabled() && m_LucentBrowser.m_Overlay.m_Target == CLucentBrowserState::EOverlay::DIRECT_CONNECT)
		{
			Ui()->SetActiveItem(&m_LucentDirectAddress);
			m_LucentBrowser.m_DirectFocusPending = false;
		}
		m_LucentDirectAddress.SetEmptyText(Localize("Server address"));
		Ui()->DoClearableEditBox(&m_LucentDirectAddress, &Address, 11);
		View.HSplitTop(8, nullptr, &View);
		View.HSplitTop(30, &Action, &View);
		Action.VSplitLeft(std::min(150.f, Action.w), &Action, nullptr);
		static int s_DirectConnect;
		const bool Valid = !m_LucentDirectAddress.IsEmpty();
		if(LucentUi::SceneButton(Ui(), TextRender(), &s_DirectConnect, Localize("Connect"), Action, Valid) || (Ui()->Enabled() && Valid && m_LucentDirectAddress.IsActive() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
		{
			m_LucentBrowser.Escape();
			m_LucentDirectAddress.Deactivate();
			Connect(m_LucentDirectAddress.GetString());
		}
	}
	else if(Mode == EPanel::FILTERS)
	{
		CUIRect Heading, Back, Reset, Actions;
		View.HSplitTop(32, &Heading, &View);
		Ui()->DoLabel(&Heading, Localize("Filters"), 18, TEXTALIGN_MC);
		// A separate action row keeps the title centered at narrow widths too.
		View.HSplitTop(22, &Actions, &View);
		Actions.VSplitRight(182, nullptr, &Actions);
		Actions.VSplitLeft(110, &Reset, &Back);
		CUIRect ActionRule = {Back.x, Back.y + 5, LucentStyle::Style::Hairline, Back.h - 10};
		ActionRule.Draw(LucentUi::Accent(.18f), 0, 0);
		static int s_CloseFilters, s_ResetFilters;
		if(LucentUi::TextButton(Ui(), TextRender(), &s_ResetFilters, Localize("Reset filter"), Reset, false, 11))
			ResetServerbrowserFilters();
		if(LucentUi::TextButton(Ui(), TextRender(), &s_CloseFilters, Localize("Back"), Back, false, 11))
		{
			m_LucentBrowser.m_Overlay.m_Target = CLucentBrowserState::EOverlay::NONE;
			if(auto *pActive = CLineInput::GetActiveInput())
				pActive->Deactivate();
			Ui()->SetActiveItem(nullptr);
		}
		CUIRect Rule;
		View.HSplitTop(1, &Rule, &View);
		LucentUi::Divider(Rule, .35f, true);
		View.HSplitTop(12, nullptr, &View);
		static CScrollRegion s_Scroll;
		// Consume over the whole panel before the server list sees the wheel,
		// including the fixed header and padding on the first hover frame.
		if(m_LucentFilterWheel != 0)
			s_Scroll.ScrollRelative(m_LucentFilterWheel < 0 ? CScrollRegion::SCROLLRELATIVE_UP : CScrollRegion::SCROLLRELATIVE_DOWN);
		m_LucentFiltersRendering = true;
		// The filter renderer measures its complete layout before beginning scrolling.
		RenderServerbrowserFilters(View, &s_Scroll);
		m_LucentFiltersRendering = false;
	}
	else if(Mode == EPanel::FRIENDS)
		RenderServerbrowserFriends(View);
	else if(Mode == EPanel::COMMUNITIES)
		RenderServerbrowserCommunitiesFilter(View);
	else
	{
		const CServerInfo *pServer = LucentSelectedServer();
		if(!pServer)
		{
			Ui()->DoLabel(&View, Localize("No server selected"), 12, TEXTALIGN_MC);
			return;
		}
		if(Mode == EPanel::PLAYERS)
		{
			View.w = std::min(View.w, 360.f);
			RenderServerbrowserInfoScoreboard(View, pServer);
			return;
		}
		static CScrollRegion s_DetailsScroll;
		CScrollRegionParams DetailsParams;
		DetailsParams.m_ScrollbarThickness = 8;
		DetailsParams.m_ScrollbarMargin = 1;
		s_DetailsScroll.Begin(&View, &DetailsParams);
		CUIRect Content = {View.x, View.y, View.w, std::max(305.f, View.h)};
		s_DetailsScroll.AddRect(Content);
		View = Content;
		// Complete identity remains available even when the list emphasizes the map.
		CUIRect Row;
		const auto Label = [&](const char *pText, float Height, float Size) {
			View.HSplitTop(Height, &Row, &View);
			SLabelProperties Props;
			Props.m_MaxWidth = Row.w;
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			Ui()->DoLabel(&Row, pText, Size, TEXTALIGN_ML, Props);
		};
		Label(pServer->m_aName, 26, 13);
		Ui()->DoToolTip(&pServer->m_aName, &Row, pServer->m_aName);

		char aMeta[128];
		const char *apRegions[] = {"Unknown region", "Africa", "Asia", "Australia", "Europe", "North America", "South America", "China"};
		const char *pCategory = pServer->m_aCommunityType[0] && str_comp(pServer->m_aCommunityType, IServerBrowser::COMMUNITY_TYPE_NONE) != 0 ? pServer->m_aCommunityType : pServer->m_aGameType;
		str_format(aMeta, sizeof(aMeta), "%s · %s · %d/%d %s", Localize(apRegions[std::clamp(pServer->m_Location, 0, 7)]), pCategory, pServer->m_NumPlayers, pServer->m_MaxPlayers, Localize("players"));
		Label(aMeta, 24, 11);
		View.HSplitTop(14, nullptr, &View);
		CUIRect ConnectButton;
		View.HSplitTop(30, &ConnectButton, &View);
		ConnectButton.VSplitLeft(std::min(150.f, ConnectButton.w), &ConnectButton, nullptr);
		static int s_Connect;
		if(LucentUi::SceneButton(Ui(), TextRender(), &s_Connect, Localize("Connect"), ConnectButton))
			Connect(pServer->m_aAddress);
		View.HSplitTop(12, nullptr, &View);
		TextRender()->TextColor(LucentStyle::Style::DetailText);
		Label(pServer->m_aAddress, 22, 10);
		Ui()->DoToolTip(&pServer->m_aAddress, &Row, pServer->m_aAddress);
		str_format(aMeta, sizeof(aMeta), "%s: %s", Localize("Version"), pServer->m_aVersion);
		Label(aMeta, 20, 10);
		if(pServer->m_LatencyIsEstimated)
			str_format(aMeta, sizeof(aMeta), "%s: %s", Localize("Ping"), Localize("Estimated"));
		else
			str_format(aMeta, sizeof(aMeta), "%s: %d ms", Localize("Ping"), pServer->m_Latency);
		Label(aMeta, 20, 10);
		if(pServer->m_Flags & SERVER_FLAG_PASSWORD)
			Label(Localize("Password required"), 20, 10);
		if(pServer->m_RequiresLogin)
			Label(Localize("Login required"), 20, 10);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		View.HSplitTop(14, nullptr, &View);
		CUIRect Actions, CopyAction;
		View.HSplitTop(28, &Actions, &View);
		Actions.w = std::min(Actions.w, 240.f);
		Actions.VSplitMid(&Row, &CopyAction, 8);
		static int s_Favorite;
		if(LucentUi::TextButton(Ui(), TextRender(), &s_Favorite, Localize("Favorite"), Row, pServer->m_Favorite != TRISTATE::NONE, 10, TEXTALIGN_ML, true, FontIcon::STAR))
		{
			if(pServer->m_Favorite != TRISTATE::NONE)
				Favorites()->Remove(pServer->m_aAddresses, pServer->m_NumAddresses);
			else
			{
				Favorites()->Add(pServer->m_aAddresses, pServer->m_NumAddresses);
				if(g_Config.m_UiPage == PAGE_LAN)
					Favorites()->AllowPing(pServer->m_aAddresses, pServer->m_NumAddresses, true);
			}
			Client()->ServerBrowserUpdate();
		}
		if(pServer->m_Favorite != TRISTATE::NONE)
		{
			View.HSplitTop(22, &Row, &View);
			static int s_Ping;
			if(DoButton_CheckBox_Tristate(&s_Ping, Localize("Leak IP"), pServer->m_FavoriteAllowPing, &Row))
			{
				Favorites()->AllowPing(pServer->m_aAddresses, pServer->m_NumAddresses, pServer->m_FavoriteAllowPing == TRISTATE::NONE);
				Client()->ServerBrowserUpdate();
			}
		}
		static int s_Copy;
		if(LucentUi::TextButton(Ui(), TextRender(), &s_Copy, Localize("Copy info"), CopyAction, false, 10, TEXTALIGN_ML, false, FontIcon::FILE))
		{
			char aInfo[1024];
			str_format(aInfo, sizeof(aInfo), "%s\n%s: ddnet://%s\n%s: %s\n%s: %s\n%s: %s\n%s: %d/%d\n", pServer->m_aName, Localize("Server address"), pServer->m_aAddress, Localize("Map"), pServer->m_aMap, Localize("Type"), pServer->m_aGameType, Localize("Version"), pServer->m_aVersion, Localize("Players"), pServer->m_NumPlayers, pServer->m_MaxPlayers);
			Input()->SetClipboardText(aInfo);
		}
		View.HSplitTop(8, nullptr, &View);
		Label(Localize("Current DDNet map artwork; revisions may differ."), 24, 8);
		s_DetailsScroll.End();
	}
}

// Lucent presentation adapter: the country database and committed filter stay shared.
CUi::EPopupMenuFunctionResult CMenus::PopupLucentCountrySelection(void *pContext, CUIRect View, bool Active)
{
	auto &Context = *static_cast<SPopupCountrySelectionContext *>(pContext);
	auto *pMenus = Context.m_pMenus;
	auto *pUi = pMenus->Ui();
	LucentUi::CScope Style(pUi);
	LucentUi::CInteractionScope Interaction(pUi, Active);
	static CListBox s_List;
	static CLucentCountryPickerState s_State;
	if(Context.m_New)
	{
		Context.m_New = false;
		pMenus->m_LucentCountrySearch.Clear();
		pUi->SetActiveItem(&pMenus->m_LucentCountrySearch);
		s_List.Reset();
		s_List.ScrollToSelected();
		s_State = {};
	}
	CUIRect Search;
	View.HSplitTop(24, &Search, &View);
	pMenus->m_LucentCountrySearch.SetEmptyText(Localize("Search countries by code"));
	const bool Changed = pUi->DoClearableEditBox(&pMenus->m_LucentCountrySearch, &Search, 11);
	View.HSplitTop(8, nullptr, &View);
	// Country codes have a bounded protocol range; avoid per-frame allocation.
	std::array<int, CountryCode::MAXIMUM - CountryCode::MINIMUM + 1> aMatches;
	int Count = 0, Selected = -1;
	for(size_t i = 0; i < pMenus->GameClient()->m_CountryFlags.Num(); ++i)
	{
		const auto &Entry = pMenus->GameClient()->m_CountryFlags.GetByIndex(i);
		const char *pLabel = Entry.m_CountryCode == CountryCode::DEFAULT ? Localize("Unknown country") : Entry.m_aCountryCodeString;
		if(!str_utf8_find_nocase(pLabel, pMenus->m_LucentCountrySearch.GetString()))
			continue;
		if(Entry.m_CountryCode == Context.m_Selection)
			Selected = Count;
		aMatches[Count++] = i;
	}
	if(s_State.Reconcile(Count, Selected, Changed))
		s_List.ScrollToSelected();
	if(Active && (pMenus->Input()->KeyPress(KEY_DOWN) || pMenus->Input()->KeyPress(KEY_UP) || pMenus->Input()->KeyPress(KEY_TAB) || pMenus->Input()->KeyPress(KEY_RETURN)))
	{
		pMenus->m_LucentCountrySearch.Deactivate();
		pUi->SetActiveItem(nullptr);
	}
	s_List.SetActive(Active && !pMenus->m_LucentCountrySearch.IsActive());
	s_List.SetScrollbarWidth(6);
	s_List.SetScrollbarMargin(1);
	s_List.SetColors(LucentUi::Accent(.07f), ColorRGBA(0, 0, 0, 0));
	s_List.DoStart(28, Count, 3, 3, s_State.m_Cursor, &View, false);
	for(int i = 0; i < Count; ++i)
	{
		const auto &Entry = pMenus->GameClient()->m_CountryFlags.GetByIndex(aMatches[i]);
		const auto Item = s_List.DoNextItem(&Entry, false, 2);
		if(!Item.m_Visible)
			continue;
		const float Hover = LucentUi::Highlight(&Entry, Active && pUi->MouseHovered(&Item.m_Rect));
		Item.m_Rect.Draw(LucentUi::Accent(.05f * Hover), IGraphics::CORNER_ALL, 2);
		CUIRect Flag, Label = Item.m_Rect;
		Label.VSplitLeft(30, &Flag, &Label);
		Flag.Margin(3, &Flag);
		Flag.HMargin((Flag.h - 12) / 2, &Flag);
		pMenus->GameClient()->m_CountryFlags.Render(Entry.m_CountryCode, ColorRGBA(1, 1, 1, 1), Flag.x, Flag.y, Flag.w, Flag.h);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = true;
		pUi->DoLabel(&Label, Entry.m_CountryCode == CountryCode::DEFAULT ? Localize("Unknown country") : Entry.m_aCountryCodeString, 10, TEXTALIGN_ML, Props);
		if(Entry.m_CountryCode == Context.m_Selection)
		{
			CUIRect Mark = {Item.m_Rect.x + 2, Item.m_Rect.y + Item.m_Rect.h - 2, Item.m_Rect.w - 4, .6f};
			Mark.Draw(LucentUi::Accent(.85f), 0, 0);
		}
	}
	s_State.m_Cursor = s_List.DoEnd();
	if(!Count)
		pUi->DoLabel(&View, Localize("No countries found"), 11, TEXTALIGN_MC);
	if(s_State.CanCommit(Count, s_List.WasItemSelected(), s_List.WasItemActivated()))
	{
		Context.m_Selection = pMenus->GameClient()->m_CountryFlags.GetByIndex(aMatches[s_State.m_Cursor]).m_CountryCode;
		g_Config.m_BrFilterCountryIndex = Context.m_Selection;
		g_Config.m_BrFilterCountry = 1;
		pMenus->Client()->ServerBrowserUpdate();
		pMenus->m_LucentCountrySearch.Deactivate();
		return CUi::POPUP_CLOSE_CURRENT;
	}
	return CUi::POPUP_KEEP_OPEN;
}
