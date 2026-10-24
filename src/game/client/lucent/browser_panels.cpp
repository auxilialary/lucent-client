#include "ui.h"

#include <engine/favorites.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>

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
		View.HSplitTop(26, &Action, &View);
		static int s_DirectConnect;
		const bool Valid = !m_LucentDirectAddress.IsEmpty();
		if(LucentUi::Button(Ui(), TextRender(), &s_DirectConnect, Localize("Connect"), Action, true, Valid) || (Ui()->Enabled() && Valid && m_LucentDirectAddress.IsActive() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
		{
			m_LucentBrowser.Escape();
			m_LucentDirectAddress.Deactivate();
			Connect(m_LucentDirectAddress.GetString());
		}
	}
	else if(Mode == EPanel::FILTERS)
	{
		CUIRect Heading, Back;
		View.HSplitTop(32, &Heading, &View);
		Heading.VSplitRight(72, &Heading, &Back);
		Ui()->DoLabel(&Heading, Localize("Filters"), 18, TEXTALIGN_ML);
		static int s_CloseFilters;
		if(LucentUi::TextButton(Ui(), TextRender(), &s_CloseFilters, Localize("Back"), Back, false, 11))
		{
			m_LucentBrowser.m_Overlay.m_Target = CLucentBrowserState::EOverlay::NONE;
			if(auto *pActive = CLineInput::GetActiveInput())
				pActive->Deactivate();
			Ui()->SetActiveItem(nullptr);
		}
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
			RenderServerbrowserInfoScoreboard(View, pServer);
			return;
		}
		static CScrollRegion s_DetailsScroll;
		CScrollRegionParams DetailsParams;
		DetailsParams.m_ScrollbarThickness = 8;
		DetailsParams.m_ScrollbarMargin = 1;
		s_DetailsScroll.Begin(&View, &DetailsParams);
		CUIRect Content = {View.x, View.y, View.w, std::max(460.f, View.h)};
		s_DetailsScroll.AddRect(Content);
		View = Content;
		// Complete identity remains available even when the list emphasizes the map.
		CUIRect Row;
		const auto Label = [&](const char *pText, float Height, float Size) {
			View.HSplitTop(Height, &Row, &View);
			SLabelProperties Props;
			Props.m_MaxWidth = Row.w;
			Props.m_StopAtEnd = true;
			Ui()->DoLabel(&Row, pText, Size, TEXTALIGN_ML, Props);
		};
		Label(pServer->m_aName, 26, 12);
		Ui()->DoToolTip(&pServer->m_aName, &Row, pServer->m_aName);
		Label(pServer->m_aAddress, 20, 10);

		char aMeta[128];
		const char *apRegions[] = {"Unknown region", "Africa", "Asia", "Australia", "Europe", "North America", "South America", "China"};
		str_format(aMeta, sizeof(aMeta), "%s   /   %d/%d %s", Localize(apRegions[std::clamp(pServer->m_Location, 0, 7)]), pServer->m_NumPlayers, pServer->m_MaxPlayers, Localize("players"));
		Label(aMeta, 20, 10);
		CUIRect ConnectButton;
		View.HSplitTop(24, &ConnectButton, &View);
		static int s_Connect;
		if(LucentBrowserButton(&s_Connect, "Connect", ConnectButton, true))
			Connect(pServer->m_aAddress);
		View.HSplitTop(8, nullptr, &View);
		Label(pServer->m_aGameType, 20, 11);
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
		View.HSplitTop(26, &Row, &View);
		static int s_Favorite;
		if(LucentBrowserButton(&s_Favorite, "Favorite", Row, pServer->m_Favorite != TRISTATE::NONE))
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
		View.HSplitTop(26, &Row, &View);
		static int s_Copy;
		if(LucentBrowserButton(&s_Copy, "Copy info", Row))
		{
			char aInfo[1024];
			str_format(aInfo, sizeof(aInfo), "%s\nAddress: ddnet://%s\nMap: %s\nType: %s\nVersion: %s\nPlayers: %d/%d\n", pServer->m_aName, pServer->m_aAddress, pServer->m_aMap, pServer->m_aGameType, pServer->m_aVersion, pServer->m_NumPlayers, pServer->m_MaxPlayers);
			Input()->SetClipboardText(aInfo);
		}
		View.HSplitTop(8, nullptr, &View);
		Label(Localize("Current DDNet map artwork; revisions may differ."), 24, 8);
		s_DetailsScroll.End();
	}
}
