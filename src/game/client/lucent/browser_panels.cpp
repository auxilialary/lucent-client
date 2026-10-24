#include "ui.h"

#include <engine/favorites.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>

void CMenus::RenderLucentPanel(CUIRect View)
{
	using EPanel = CLucentBrowserState::EPanel;
	const auto Mode = m_LucentBrowser.VisiblePanel();
	// Translate the entire surface, content and hitboxes together.
	View.x += (1 - m_LucentBrowser.m_Open) * 12;
	const CUIRect PanelBounds = View;
	const float aX[] = {View.x - 36, View.x, View.x + View.w};
	const float aY[] = {View.y - 16, View.y, View.y + View.h, View.y + View.h + 16};
	const float aHorizontal[] = {0, 1, 1}, aVertical[] = {0, 1, 1, 0};
	for(int y = 0; y < 3; ++y)
		for(int x = 0; x < 2; ++x)
		{
			const auto Color = [&](int X, int Y) { return ColorRGBA(.025f, .021f, .035f, .97f * m_LucentBrowser.m_Open * aHorizontal[X] * aVertical[Y]); };
			CUIRect Band = {aX[x], aY[y], aX[x + 1] - aX[x], aY[y + 1] - aY[y]};
			Band.Draw4(Color(x, y), Color(x + 1, y), Color(x, y + 1), Color(x + 1, y + 1), 0, 0);
		}
	// Fade labels, cached text and shared controls with the same panel clock.
	LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.m_Open * m_LucentBrowser.m_Content);
	View.Margin(10, &View);
	CUIRect Title, Close;
	View.HSplitTop(24, &Title, &View);
	Title.VSplitRight(24, &Title, &Close);
	const char *apTitles[] = {"", "Filters", "Server details", "Players", "Friends", "Communities"};
	Ui()->DoLabel(&Title, Localize(apTitles[int(Mode)]), 13, TEXTALIGN_ML);
	static int s_Close;
	if(LucentBrowserButton(&s_Close, "x", Close))
	{
		m_LucentBrowser.Escape();
		return;
	}
	if(Mode == EPanel::DETAILS || Mode == EPanel::PLAYERS)
	{
		CUIRect Tabs, Details, Players;
		View.HSplitTop(24, &Tabs, &View);
		Tabs.VSplitMid(&Details, &Players);
		static int s_Details, s_Players;
		if(LucentBrowserButton(&s_Details, "Server details", Details, Mode == EPanel::DETAILS) && Mode != EPanel::DETAILS)
			m_LucentBrowser.Toggle(EPanel::DETAILS);
		if(LucentBrowserButton(&s_Players, "Players", Players, Mode == EPanel::PLAYERS) && Mode != EPanel::PLAYERS)
			m_LucentBrowser.Toggle(EPanel::PLAYERS);
	}
	View.HSplitTop(5, nullptr, &View);
	// Restrained content slide, geometry shared by drawing and hitboxes.
	View.x += (1 - m_LucentBrowser.m_Content) * 12;
	View.w -= (1 - m_LucentBrowser.m_Content) * 12;
	if(Mode == EPanel::FILTERS)
	{
		static CScrollRegion s_Scroll;
		// Consume over the whole panel before the server list sees the wheel,
		// including the fixed header and padding on the first hover frame.
		if(Ui()->Enabled() && Ui()->MouseHovered(&PanelBounds))
		{
			if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP))
				s_Scroll.ScrollRelative(CScrollRegion::SCROLLRELATIVE_UP);
			else if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN))
				s_Scroll.ScrollRelative(CScrollRegion::SCROLLRELATIVE_DOWN);
		}
		// The filter renderer measures its complete layout before beginning scrolling.
		RenderServerbrowserFilters(View, &s_Scroll);
	}
	else if(Mode == EPanel::FRIENDS)
		RenderServerbrowserFriends(View);
	else if(Mode == EPanel::COMMUNITIES)
		RenderServerbrowserCommunitiesFilter(View);
	else
	{
		const CServerInfo *pServer = ServerBrowser()->SortedGet(m_SelectedIndex);
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
		Label(pServer->m_aMap, 30, 18);
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
