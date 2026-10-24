#include <base/time.h>

#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>

bool CMenus::LucentBrowserButton(const void *pId, const char *pText, CUIRect Rect, bool Selected)
{
	const bool Hover = Ui()->MouseInside(&Rect);
	if(Selected || Hover)
		Rect.Draw(ColorRGBA(1, .85f, .93f, Selected ? .12f : .055f), IGraphics::CORNER_ALL, 3);
	TextRender()->TextColor(Selected ? ColorRGBA(1, .86f, .93f, 1) : ColorRGBA(.8f, .79f, .84f, 1));
	const char *pLabel = Localize(pText);
	const float Size = std::min(11.f, 11.f * std::max(1.f, Rect.w - 6) / std::max(1.f, TextRender()->TextWidth(11, pLabel)));
	Ui()->DoLabel(&Rect, pLabel, Size, TEXTALIGN_MC);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	return Ui()->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
}

void CMenus::RenderLucentBrowser(CUIRect Screen)
{
	Screen = *Ui()->Screen();
	UpdateCommunityCache(false);
	if(g_Config.m_UiPage == PAGE_LAN && m_ForceRefreshLanPage)
	{
		RefreshBrowserTab(true);
		m_ForceRefreshLanPage = false;
	}
	const double Now = double(time_get()) / time_freq();
	const float Dt = m_LucentBrowserTime > 0 ? float(std::min(.1, Now - m_LucentBrowserTime)) : 0;
	m_LucentBrowserTime = Now;
	const bool Animate = g_Config.m_ClLucentLogoAnimation != 0;
	m_LucentBrowser.Update(Dt, Animate);
	m_SelectedIndex = CLucentBrowserState::FindSelection(ServerBrowser()->NumSortedServers(), g_Config.m_UiServerAddress, [this](int i) { return ServerBrowser()->SortedGet(i)->m_aAddress; });
	const CServerInfo *pSelected = ServerBrowser()->SortedGet(m_SelectedIndex);
	m_LucentPreview.Update(pSelected, Now, Animate, Http(), Engine(), Graphics());
	m_LucentPreview.Render(Graphics(), Screen, Now, Animate);
	Ui()->MapScreen();
	if(m_LucentLegacyBrowser)
	{
		CUIRect Back;
		Screen.Margin(10, &Screen);
		Screen.HSplitTop(24, &Back, &Screen);
		static int s_Back;
		if(LucentBrowserButton(&s_Back, "Return to Lucent browser", Back))
			m_LucentLegacyBrowser = false;
		Screen.HSplitTop(24, &Back, &Screen);
		RenderMenubar(Back, IClient::STATE_OFFLINE);
		RenderServerbrowser(Screen);
		return;
	}
	Screen.Margin(20, &Screen);
	CUIRect Top, Nav, Tools, Footer, Status, List;
	Screen.HSplitTop(30, &Top, &Screen);
	Screen.HSplitTop(29, &Nav, &Screen);
	Screen.HSplitTop(26, &Tools, &Screen);
	Screen.HSplitBottom(48, &Screen, &Footer);
	CUIRect Brand, Search, Refresh;
	Top.VSplitLeft(std::min(180.f, Top.w * .35f), &Brand, &Top);
	Ui()->DoLabel(&Brand, "L U C E N T   /   SERVERS", 12, TEXTALIGN_ML);
	Top.VSplitRight(65, &Search, &Refresh);
	Search.Margin(3, &Search);
	static CLineInput s_Search(g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString));
	static CLineInput s_Address(g_Config.m_UiServerAddress, sizeof(g_Config.m_UiServerAddress));
	if(s_Search.IsActive())
		m_LucentBrowser.m_Focus = 1;
	if(s_Address.IsActive())
		m_LucentBrowser.m_Focus = 5;
	const bool Keyboard = Ui()->Enabled() && !Ui()->IsPopupOpen();
	if(Keyboard && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))
	{
		m_LucentBrowser.Tab(Input()->ShiftIsPressed());
		if(auto *pActive = CLineInput::GetActiveInput())
			pActive->Deactivate();
		Ui()->SetActiveItem(m_LucentBrowser.m_Focus == 1 ? &s_Search : m_LucentBrowser.m_Focus == 5 ? &s_Address :
													      nullptr);
	}
	const bool Activate = Keyboard && m_LucentBrowser.m_Focus != 0 && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
	if(Activate && m_LucentBrowser.m_Focus == 1)
	{
		s_Search.Deactivate();
		Ui()->SetActiveItem(nullptr);
		m_LucentBrowser.m_Focus = 0;
	}

	s_Search.SetEmptyText(Localize("Search maps, servers, players"));
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
	{
		Ui()->SetActiveItem(&s_Search);
		s_Search.SelectAll();
		m_LucentBrowser.m_Focus = 1;
	}
	if(Ui()->DoClearableEditBox(&s_Search, &Search, 12))
		Client()->ServerBrowserUpdate();
	static int s_Refresh;
	if(LucentBrowserButton(&s_Refresh, "Refresh", Refresh) || (Ui()->Enabled() && !Ui()->IsPopupOpen() && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R)))))
		RefreshBrowserTab(true);
	static int s_aNav[7];
	const char *apNav[] = {"All servers", "Favorites", "Friends", "Communities", "LAN", "Settings", "Home"};
	const float aWidths[] = {85, 75, 65, 95, 45, 65, 55};
	const float NavScale = std::min(1.f, Nav.w / 485);
	for(int i = 0; i < 7; ++i)
	{
		CUIRect Tab;
		Nav.VSplitLeft(aWidths[i] * NavScale, &Tab, &Nav);
		const bool Selected = (i == 0 && m_MenuPage == PAGE_INTERNET) || (i == 1 && m_MenuPage == PAGE_FAVORITES) || (i == 2 && m_LucentBrowser.m_Panel == CLucentBrowserState::EPanel::FRIENDS) || (i == 3 && m_LucentBrowser.m_Panel == CLucentBrowserState::EPanel::COMMUNITIES) || (i == 4 && m_MenuPage == PAGE_LAN);
		if(!LucentBrowserButton(&s_aNav[i], apNav[i], Tab, Selected))
			continue;
		if(i == 0 || i == 1 || i == 4)
		{
			SetMenuPage(i == 0 ? PAGE_INTERNET : i == 1 ? PAGE_FAVORITES :
								      PAGE_LAN);
			RefreshBrowserTab(false);
		}
		if(i == 2)
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::FRIENDS);
		if(i == 3)
		{
			if(m_MenuPage != PAGE_INTERNET && m_MenuPage != PAGE_FAVORITES)
			{
				SetMenuPage(PAGE_INTERNET);
				RefreshBrowserTab(false);
			}
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::COMMUNITIES);
		}
		if(i == 5)
			SetMenuPage(PAGE_SETTINGS);
		if(i == 6)
			SetShowStart(true);
	}
	static int s_aTools[4];
	const char *apTools[] = {"Filters", "Server details", "Players", "Legacy view"};
	const CLucentBrowserState::EPanel aPanels[] = {CLucentBrowserState::EPanel::FILTERS, CLucentBrowserState::EPanel::DETAILS, CLucentBrowserState::EPanel::PLAYERS};
	for(int i = 0; i < 4; ++i)
	{
		CUIRect Tab;
		Tools.VSplitLeft(i == 1 ? 100 : 77, &Tab, &Tools);
		if(LucentBrowserButton(&s_aTools[i], apTools[i], Tab, i < 3 && (m_LucentBrowser.m_Panel == aPanels[i] || m_LucentBrowser.m_Focus == i + 2)) || (i < 3 && Activate && m_LucentBrowser.m_Focus == i + 2))
		{
			if(i < 3)
				m_LucentBrowser.Toggle(aPanels[i]);
			else
				m_LucentLegacyBrowser = true;
		}
	}
	CUIRect Rule = {Screen.x, Screen.y, Screen.w, .5f};
	Rule.Draw(ColorRGBA(1, .85f, .93f, .1f), 0, 0);
	Screen.HSplitTop(5, nullptr, &Screen);
	List = Screen;
	const float PanelWidth = m_LucentBrowser.PanelWidth(Screen.w);
	if(PanelWidth > .5f)
	{
		CUIRect Panel;
		Screen.VSplitRight(PanelWidth, &List, &Panel);
		List.VSplitRight(12 * m_LucentBrowser.m_Open, &List, nullptr);
		// Render only at a usable width. Drawing and input share the animated rectangle.
		if(Panel.w > 150 && m_LucentBrowser.m_Panel != CLucentBrowserState::EPanel::NONE)
		{
			m_LucentPanelRendering = true;
			RenderLucentPanel(Panel);
			m_LucentPanelRendering = false;
		}
	}
	bool Activated = false;
	RenderLucentServerList(List, Activated);
	Footer.HSplitTop(20, &Status, &Footer);
	char aStatus[192];
	str_format(aStatus, sizeof(aStatus), "%d / %d %s   /   %d %s%s", ServerBrowser()->NumSortedServers(), ServerBrowser()->NumServers(), Localize("servers"), ServerBrowser()->NumSortedPlayers(), Localize("players"), ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist() ? "   /   Refreshing..." : "");
	TextRender()->TextColor(ColorRGBA(.63f, .62f, .69f, 1));
	Ui()->DoLabel(&Status, aStatus, 10, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	CUIRect Label, Address, ConnectButton;
	Footer.VSplitLeft(95, &Label, &Footer);
	Footer.VSplitRight(95, &Address, &ConnectButton);
	Address.Margin(2, &Address);
	Ui()->DoLabel(&Label, Localize("Direct connect"), 11, TEXTALIGN_ML);
	if(Ui()->DoClearableEditBox(&s_Address, &Address, 11))
		m_ServerBrowserShouldRevealSelection = true;
	static int s_Connect;
	if((LucentBrowserButton(&s_Connect, "Connect", ConnectButton, true) || Activated || (Activate && (m_LucentBrowser.m_Focus == 5 || m_LucentBrowser.m_Focus == 6))) && g_Config.m_UiServerAddress[0])
		Connect(g_Config.m_UiServerAddress);
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && m_LucentBrowser.m_Panel != CLucentBrowserState::EPanel::NONE && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
		m_LucentBrowser.Escape();
}

void CMenus::RenderLucentServerList(CUIRect View, bool &Activated)
{
	static CListBox s_List;
	s_List.SetColors(ColorRGBA(1, .8f, .9f, .13f), ColorRGBA(1, .9f, .95f, 0));
	s_List.SetScrollbarWidth(10);
	s_List.SetScrollbarMargin(2);
	CUIRect Header;
	View.HSplitTop(19, &Header, &View);
	Header.VSplitRight(s_List.ScrollbarWidthMax(), &Header, nullptr);
	// Fixed metadata columns, flexible map and identity columns.
	const float ColumnScale = std::min(1.f, Header.w / 400);
	const float Fixed = (30 + 43 + 50 + 42 + 70) * ColumnScale;
	const float MapWidth = std::max(0.f, (Header.w - Fixed) * .58f);
	const float NameWidth = std::max(0.f, Header.w - Fixed - MapWidth);
	const float aWidths[] = {30 * ColumnScale, MapWidth, NameWidth, 70 * ColumnScale, 43 * ColumnScale, 50 * ColumnScale, 42 * ColumnScale};
	const char *apTitles[] = {"", "Map", "Server", "Type", "Region", "Players", "Ping"};
	const int aSort[] = {IServerBrowser::SORT_FAVORITES, IServerBrowser::SORT_MAP, IServerBrowser::SORT_NAME, IServerBrowser::SORT_GAMETYPE, -1, IServerBrowser::SORT_NUMPLAYERS, IServerBrowser::SORT_PING};
	static int s_aSortIds[7];
	CUIRect aCols[7];
	for(int c = 0; c < 7; ++c)
	{
		Header.VSplitLeft(aWidths[c], &aCols[c], &Header);
		if(LucentBrowserButton(&s_aSortIds[c], apTitles[c], aCols[c], g_Config.m_BrSort == aSort[c]) && aSort[c] >= 0)
		{
			CLucentBrowserState::Sort(aSort[c], g_Config.m_BrSort, g_Config.m_BrSortOrder, aSort[c] == IServerBrowser::SORT_PING || aSort[c] == IServerBrowser::SORT_NUMPLAYERS);
			Client()->ServerBrowserUpdate();
		}
		if(c == 0 || g_Config.m_BrSort == aSort[c])
		{
			CUIRect Icon = aCols[c];
			if(c != 0)
			{
				Icon.x += Icon.w - 8;
				Icon.w = 6;
			}
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			Ui()->DoLabel(&Icon, c == 0 ? FontIcon::STAR : g_Config.m_BrSortOrder == 2 ? FontIcon::ARROWS_UP_DOWN :
							       g_Config.m_BrSortOrder == 1         ? FontIcon::CHEVRON_UP :
												     FontIcon::CHEVRON_DOWN,
				7, TEXTALIGN_MC);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
	}
	const int Count = ServerBrowser()->NumSortedServers();
	auto &vpElements = m_avpServerBrowserUiElements[ServerBrowser()->GetCurrentType()];
	if(vpElements.size() < size_t(Count))
		vpElements.resize(Count, nullptr);
	const int Selected = CLucentBrowserState::FindSelection(Count, g_Config.m_UiServerAddress, [this](int i) { return ServerBrowser()->SortedGet(i)->m_aAddress; });
	// Grow stable item storage only when server count grows, never per frame.
	auto &vIds = m_vServerbrowserElementIds[ServerBrowser()->GetCurrentType()];
	if(vIds.size() < size_t(Count))
		vIds.resize(Count);
	if(m_vLucentRowHover.size() < size_t(Count))
		m_vLucentRowHover.resize(Count, 0);
	s_List.SetActive(!Ui()->IsPopupOpen() && m_LucentBrowser.m_Focus == 0 && CLineInput::GetActiveInput() == nullptr);
	if(m_ServerBrowserShouldRevealSelection)
	{
		s_List.ScrollToSelected();
		m_ServerBrowserShouldRevealSelection = false;
	}
	s_List.DoStart(15.5f, Count, 1, 3, Selected, &View, false);
	const char *apRegions[] = {"--", "AFR", "ASI", "AUS", "EUR", "NA", "SA", "CHN"};
	for(int i = 0; i < Count; ++i)
	{
		const auto *pInfo = ServerBrowser()->SortedGet(i);
		const CListboxItem Item = s_List.DoNextItem(&vIds[i], i == Selected, 2);
		const float Hover = Item.m_Visible && Ui()->MouseHovered(&Item.m_Rect) ? 1.f : 0.f;
		m_vLucentRowHover[i] += (Hover - m_vLucentRowHover[i]) * (g_Config.m_ClLucentLogoAnimation ? 1 - std::exp(-Client()->RenderFrameTime() * 18) : 1);
		if(!Item.m_Visible)
			continue;
		Item.m_Rect.Draw(ColorRGBA(1, .9f, .95f, .045f * m_vLucentRowHover[i]), IGraphics::CORNER_ALL, 2);
		if(!vpElements[i])
			vpElements[i] = Ui()->GetNewUIElement(14);
		char aPlayers[32], aPing[16];
		str_format(aPlayers, sizeof(aPlayers), "%d/%d", pInfo->m_NumFilteredPlayers, ServerBrowser()->Max(*pInfo));
		if(pInfo->m_LatencyIsEstimated)
			str_copy(aPing, "~");
		else
			str_format(aPing, sizeof(aPing), "%d", pInfo->m_Latency);
		const char *apText[] = {pInfo->m_Favorite != TRISTATE::NONE ? FontIcon::STAR : "", pInfo->m_aMap, pInfo->m_aAddress, pInfo->m_aGameType, pInfo->m_aCommunityCountry[0] ? pInfo->m_aCommunityCountry : apRegions[std::clamp(pInfo->m_Location, 0, 7)], aPlayers, aPing};
		for(int c = 0; c < 7; ++c)
		{
			CUIRect Cell = {aCols[c].x + 4, Item.m_Rect.y, std::max(0.f, aCols[c].w - 8), Item.m_Rect.h};
			SLabelProperties Props;
			Props.m_MaxWidth = Cell.w;
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			TextRender()->TextColor(c == 1 ? ColorRGBA(.94f, .92f, .95f, 1) : ColorRGBA(.58f, .57f, .64f, 1));
			if(c == 0)
			{
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->TextColor(ColorRGBA(.95f, .79f, .87f, 1));
			}
			Ui()->DoLabelStreamed(*vpElements[i]->Rect(c), &Cell, apText[c], c == 1 ? 11.f : 9.f, c >= 4 ? TEXTALIGN_MR : TEXTALIGN_ML, Props);
			if(c == 0)
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		Ui()->DoToolTip(&vIds[i], &Item.m_Rect, pInfo->m_aName);
	}
	m_SelectedIndex = s_List.DoEnd();
	if(m_SelectedIndex >= 0 && m_SelectedIndex != Selected)
	{
		str_copy(g_Config.m_UiServerAddress, ServerBrowser()->SortedGet(m_SelectedIndex)->m_aAddress);
		m_ServerBrowserShouldRevealSelection = true;
	}
	if(s_List.WasItemSelected())
	{
		m_LucentBrowser.m_Focus = 0;
		if(auto *pActive = CLineInput::GetActiveInput())
			pActive->Deactivate();
	}
	Activated = s_List.WasItemActivated();
	if(!Count)
	{
		Ui()->DoLabel(&View, Localize(ServerBrowser()->IsGettingServerlist() ? "Getting server list from master server" : "No servers match your filter criteria"), 13, TEXTALIGN_MC);
		CUIRect Reset = {View.x + View.w / 2 - 60, View.y + View.h / 2 + 20, 120, 22};
		static int s_Reset;
		if(LucentBrowserButton(&s_Reset, "Reset filter", Reset))
			ResetServerbrowserFilters();
	}
}
