#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>

#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>
#include <array>

CUi::EPopupMenuFunctionResult CMenus::PopupLucentServer(void *pContext, CUIRect View, bool Active)
{
	auto &Context = *static_cast<SLucentServerPopup *>(pContext);
	auto *pMenus = Context.m_pMenus;
	LucentUi::CScope Style(pMenus->Ui());
	LucentUi::CInteractionScope Interaction(pMenus->Ui(), Active);
	const int Index = CLucentBrowserState::FindSelection(pMenus->ServerBrowser()->NumServers(), Context.m_Address, [pMenus](int i) { return pMenus->ServerBrowser()->Get(i)->m_aAddress; });
	if(Index < 0)
		return CUi::POPUP_CLOSE_CURRENT;
	const auto *pServer = pMenus->ServerBrowser()->Get(Index);
	const char *apActions[] = {"Connect", pServer->m_Favorite != TRISTATE::NONE ? "Remove from favorites" : "Add to favorites", "Copy address", "Server details", "Players"};
	static int s_aActions[5];
	for(int i = 0; i < 5; ++i)
	{
		CUIRect Row;
		View.HSplitTop(24, &Row, &View);
		if(!LucentUi::TextButton(pMenus->Ui(), pMenus->TextRender(), &s_aActions[i], Localize(apActions[i]), Row, false, 11, TEXTALIGN_ML))
			continue;
		str_copy(g_Config.m_UiServerAddress, Context.m_Address.c_str());
		pMenus->m_ServerBrowserShouldRevealSelection = true;
		if(i == 0)
			pMenus->Connect(Context.m_Address.c_str());
		else if(i == 1)
		{
			if(pServer->m_Favorite != TRISTATE::NONE)
				pMenus->Favorites()->Remove(pServer->m_aAddresses, pServer->m_NumAddresses);
			else
			{
				pMenus->Favorites()->Add(pServer->m_aAddresses, pServer->m_NumAddresses);
				if(g_Config.m_UiPage == PAGE_LAN)
					pMenus->Favorites()->AllowPing(pServer->m_aAddresses, pServer->m_NumAddresses, true);
			}
			pMenus->Client()->ServerBrowserUpdate();
		}
		else if(i == 2)
			pMenus->Input()->SetClipboardText(Context.m_Address.c_str());
		else
			pMenus->m_LucentBrowser.m_Scene.m_Target = i == 3 ? CLucentBrowserState::EScene::DETAILS : CLucentBrowserState::EScene::PLAYERS;
		return CUi::POPUP_CLOSE_CURRENT;
	}
	return CUi::POPUP_KEEP_OPEN;
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
