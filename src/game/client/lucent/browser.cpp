#include "ui.h"

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
	return LucentUi::Button(Ui(), TextRender(), pId, Localize(pText), Rect, Selected);
}
void CMenus::RenderLucentBrowser(CUIRect Screen)
{
	LucentUi::CScope Style(Ui());
	using P = CLucentBrowserState::EPanel;
	using Primary = CLucentBrowserState::EPrimary;
	using Scene = CLucentBrowserState::EScene;
	using Overlay = CLucentBrowserState::EOverlay;
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
	m_LucentBrowser.Select(pSelected ? pSelected->m_aAddress : "", Animate);
	m_LucentPreview.Update(pSelected, Now, Animate, Http(), Engine(), Graphics());
	m_LucentPreview.Render(Graphics(), Screen, Now, Animate, m_LucentBrowser.m_SelectionPulse);
	Ui()->MapScreen();
	if(m_LucentLegacyBrowser)
	{
		Ui()->SetLucentStyle(false);
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
	// A single field of content and map atmosphere beneath restrained navigation.
	Screen.Margin(18, &Screen);
	CUIRect Top, Nav, Tools, Footer, Status, List;
	Screen.HSplitTop(32, &Top, &Screen);
	Screen.HSplitTop(22, nullptr, &Screen);
	Screen.HSplitBottom(26, &Screen, &Footer);
	const CUIRect HeaderBounds = Top;
	CUIRect Search;
	Top.VSplitRight(112, &Top, &Tools);
	Top.VSplitRight(std::clamp(Top.w * .34f, 105.f, 205.f), &Nav, &Search);
	Search.Margin(4, &Search);
	Nav.VSplitRight(18, &Nav, nullptr);
	static CLineInput s_Search(g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString));
	if(s_Search.IsActive())
		m_LucentBrowser.m_Focus = 1;
	if(m_LucentBrowser.m_Overlay.m_Target != Overlay::DIRECT_CONNECT)
		m_LucentDirectAddress.Deactivate();
	const bool Keyboard = Ui()->Enabled() && !Ui()->IsPopupOpen();
	if(Keyboard && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))
	{
		m_LucentBrowser.Tab(Input()->ShiftIsPressed());
		if(auto *pActive = CLineInput::GetActiveInput())
			pActive->Deactivate();
		Ui()->SetActiveItem(m_LucentBrowser.m_Focus == 1 ? &s_Search : nullptr);
	}
	const bool Activate = Keyboard && m_LucentBrowser.m_Overlay.m_Target != Overlay::DIRECT_CONNECT && m_LucentBrowser.m_Focus != 0 && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
	if(Activate && m_LucentBrowser.m_Focus == 1)
	{
		s_Search.Deactivate();
		Ui()->SetActiveItem(nullptr);
		m_LucentBrowser.m_Focus = 0;
	}
	s_Search.SetEmptyText(Localize("Search maps or players"));
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
	{
		Ui()->SetActiveItem(&s_Search);
		s_Search.SelectAll();
		m_LucentBrowser.m_Focus = 1;
	}
	CUIRect SearchIcon;
	Search.VSplitLeft(22, &SearchIcon, &Search);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	Ui()->DoLabel(&SearchIcon, FontIcon::MAGNIFYING_GLASS, 10, TEXTALIGN_MC);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	if(Ui()->DoClearableEditBox(&s_Search, &Search, 12))
		Client()->ServerBrowserUpdate();
	static int s_aNav[5];
	const char *apNav[] = {"All servers", "Favorites", "Friends", "Communities", "LAN"};
	float aWidths[5];
	float NavWidth = 0;
	for(int i = 0; i < 5; ++i)
	{
		aWidths[i] = TextRender()->TextWidth(11, Localize(apNav[i])) + 24;
		NavWidth += aWidths[i];
	}
	const float NavScale = std::min(1.f, Nav.w / NavWidth);
	CUIRect ActiveTab = {};
	const bool ContextNav = m_LucentBrowser.m_Primary.m_Target == Primary::FRIENDS || m_LucentBrowser.m_Primary.m_Target == Primary::COMMUNITIES;
	for(int i = 0; i < 5; ++i)
	{
		CUIRect Tab;
		Nav.VSplitLeft(aWidths[i] * NavScale, &Tab, &Nav);
		const bool Selected = (i == 0 && !ContextNav && m_MenuPage == PAGE_INTERNET) || (i == 1 && !ContextNav && m_MenuPage == PAGE_FAVORITES) || (i == 2 && m_LucentBrowser.m_Primary.m_Target == Primary::FRIENDS) || (i == 3 && m_LucentBrowser.m_Primary.m_Target == Primary::COMMUNITIES) || (i == 4 && !ContextNav && m_MenuPage == PAGE_LAN);
		if(Selected)
			ActiveTab = Tab;
		if(!LucentUi::TextButton(Ui(), TextRender(), &s_aNav[i], Localize(apNav[i]), Tab, Selected, 11 * NavScale, TEXTALIGN_MC, false))
			continue;
		if(i == 0 || i == 1 || i == 4)
		{
			m_LucentBrowser.Servers();
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
	}
	m_LucentBrowser.m_Navigation.Update(ActiveTab.x + 8 * NavScale, std::max(0.f, ActiveTab.w - 16 * NavScale), Dt, Animate);
	CUIRect Indicator = {m_LucentBrowser.m_Navigation.X(), ActiveTab.y + ActiveTab.h - 2, m_LucentBrowser.m_Navigation.Width(), 1};
	Indicator.Draw(LucentUi::Accent(.7f), 0, 0);
	static int s_aTools[4];
	const char *apIcons[] = {FontIcon::LIST_UL, FontIcon::ARROWS_ROTATE, FontIcon::CHEVRON_RIGHT, FontIcon::HOUSE};
	const char *apHints[] = {"Filters", "Refresh", "Direct connect", "Main menu"};
	for(int i = 0; i < 4; ++i)
	{
		CUIRect Button;
		Tools.VSplitLeft(28, &Button, &Tools);
		Button.Margin(2, &Button);
		const bool Click = LucentUi::IconButton(Ui(), TextRender(), &s_aTools[i], apIcons[i], Localize(apHints[i]), Button, i == 0 && m_LucentBrowser.m_Overlay.m_Target == Overlay::FILTERS);
		if(i == 0 && (Click || (Activate && m_LucentBrowser.m_Focus == 2)))
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::FILTERS);
		if(i == 1 && (Click || (Keyboard && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R))))))
			RefreshBrowserTab(true);
		if(i == 2 && (Click || (Activate && m_LucentBrowser.m_Focus >= 5)))
		{
			if(m_LucentDirectAddress.IsEmpty())
				m_LucentDirectAddress.Set(g_Config.m_UiServerAddress);
			m_LucentDirectAddress.SelectAll();
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::DIRECT_CONNECT);
			m_LucentBrowser.m_DirectFocusPending = m_LucentBrowser.m_Overlay.m_Target == Overlay::DIRECT_CONNECT;
			Ui()->SetActiveItem(nullptr);
		}
		if(i == 3 && Click)
			SetShowStart(true);
	}
	// Fixed regions share the background, with independent contextual state.
	CUIRect Atmosphere;
	Screen.VSplitLeft(Screen.w * .46f, &List, &Atmosphere);
	Atmosphere.VSplitLeft(28, nullptr, &Atmosphere);
	CUIRect OverlayBounds = {Screen.x, Top.y + Top.h + 4, Screen.w, 0};
	const auto VisibleOverlay = m_LucentBrowser.m_Overlay.m_Visible;
	const bool HasOverlay = m_LucentBrowser.m_Expansion > 0 && VisibleOverlay != Overlay::NONE;
	if(HasOverlay)
	{
		OverlayBounds.h = (VisibleOverlay == Overlay::FILTERS ? std::min(260.f, Screen.h * .55f) : 106.f) * m_LucentBrowser.Expansion();
		if(VisibleOverlay == Overlay::DIRECT_CONNECT)
		{
			OverlayBounds.w = std::min(310.f, Screen.w);
			OverlayBounds.x = Screen.x + Screen.w - OverlayBounds.w;
		}
		if(Keyboard && Ui()->MouseButtonClicked(0) && !Ui()->MouseHovered(&OverlayBounds) && !Ui()->MouseHovered(&HeaderBounds))
		{
			m_LucentBrowser.m_Overlay.m_Target = Overlay::NONE;
			if(auto *pInput = CLineInput::GetActiveInput())
				pInput->Deactivate();
			Ui()->SetActiveItem(nullptr);
		}
	}
	m_LucentFilterWheel = 0;
	if(HasOverlay && Keyboard && VisibleOverlay == Overlay::FILTERS && Ui()->MouseHovered(&OverlayBounds))
	{
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP))
			m_LucentFilterWheel = -1;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN))
			m_LucentFilterWheel = 1;
	}
	const bool BlockContent = HasOverlay && Ui()->MouseHovered(&OverlayBounds);
	const bool ContentEnabled = Ui()->Enabled();
	Ui()->SetEnabled(ContentEnabled && !BlockContent);
	bool Activated = false;
	{
		LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.m_Primary.Opacity());
		LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Primary.m_Target == m_LucentBrowser.m_Primary.m_Visible);
		CUIRect Content = List;
		Content.y += 5 * (1 - m_LucentBrowser.m_Primary.Opacity());
		Content.h -= 5 * (1 - m_LucentBrowser.m_Primary.Opacity());
		m_LucentPanelRendering = true;
		if(m_LucentBrowser.m_Primary.m_Visible == Primary::FRIENDS)
			RenderLucentContext(Content, P::FRIENDS);
		else if(m_LucentBrowser.m_Primary.m_Visible == Primary::COMMUNITIES)
		{
			CUIRect Back;
			Content.HSplitBottom(26, &Content, &Back);
			static int s_ShowServers;
			if(LucentUi::TextButton(Ui(), TextRender(), &s_ShowServers, Localize("All servers"), Back))
				m_LucentBrowser.Servers();
			RenderLucentContext(Content, P::COMMUNITIES);
		}
		else
			RenderLucentServerList(Content, Activated);
		m_LucentPanelRendering = false;
	}
	if(pSelected)
	{
		// The same map title travels between preview and context, never duplicated.
		const float Position = m_LucentBrowser.ScenePosition();
		const float PreviewY = Atmosphere.y + Atmosphere.h - std::min(142.f, Atmosphere.h * .42f);
		CUIRect Title = {Atmosphere.x, PreviewY + (Atmosphere.y - PreviewY) * Position, Atmosphere.w, 38};
		SLabelProperties Props;
		Props.m_MaxWidth = Title.w;
		Props.m_StopAtEnd = true;
		Ui()->DoLabel(&Title, pSelected->m_aMap, 28 - 5 * Position, TEXTALIGN_ML, Props);
		CUIRect Body = {Atmosphere.x, Title.y + Title.h + 4, Atmosphere.w, std::max(0.f, Atmosphere.y + Atmosphere.h - Title.y - Title.h - 4)};
		LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.m_Scene.Opacity());
		LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Scene.m_Target == m_LucentBrowser.m_Scene.m_Visible);
		if(m_LucentBrowser.m_Scene.m_Visible == Scene::PREVIEW)
		{
			CUIRect Meta, Action, ConnectButton, Details, Players;
			Body.HSplitTop(22, &Meta, &Body);
			char aMeta[128];
			str_format(aMeta, sizeof(aMeta), "%s · %d/%d %s", pSelected->m_aCommunityCountry, pSelected->m_NumPlayers, pSelected->m_MaxPlayers, Localize("players"));
			TextRender()->TextColor(ColorRGBA(.77f, .71f, .76f, 1));
			Ui()->DoLabel(&Meta, aMeta, 12, TEXTALIGN_ML, Props);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Body.HSplitTop(8, nullptr, &Body);
			Body.HSplitTop(28, &Action, &Body);
			Action.VSplitLeft(std::min(120.f, Action.w * .5f), &ConnectButton, nullptr);
			Body.HSplitTop(7, nullptr, &Body);
			Body.HSplitTop(22, &Action, &Body);
			Action.VSplitMid(&Details, &Players);
			static int s_Connect, s_Details, s_Players;
			if(LucentBrowserButton(&s_Connect, "Connect", ConnectButton, true))
				Connect(pSelected->m_aAddress);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Details, Localize("Server details"), Details, false, 10, TEXTALIGN_ML) || (Activate && m_LucentBrowser.m_Focus == 3))
				m_LucentBrowser.Toggle(P::DETAILS);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Players, Localize("Players"), Players, false, 10, TEXTALIGN_ML) || (Activate && m_LucentBrowser.m_Focus == 4))
				m_LucentBrowser.Toggle(P::PLAYERS);
		}
		else
		{
			CUIRect Tabs, Back, Details, Players;
			Body.HSplitTop(25, &Tabs, &Body);
			Tabs.VSplitLeft(28, &Back, &Tabs);
			Tabs.VSplitMid(&Details, &Players);
			static int s_Back, s_Details, s_Players;
			if(LucentUi::IconButton(Ui(), TextRender(), &s_Back, FontIcon::CHEVRON_LEFT, Localize("Back"), Back))
				m_LucentBrowser.m_Scene.m_Target = Scene::PREVIEW;
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Details, Localize("Server details"), Details, m_LucentBrowser.m_Scene.m_Target == Scene::DETAILS, 10))
				m_LucentBrowser.m_Scene.m_Target = Scene::DETAILS;
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Players, Localize("Players"), Players, m_LucentBrowser.m_Scene.m_Target == Scene::PLAYERS, 10))
				m_LucentBrowser.m_Scene.m_Target = Scene::PLAYERS;
			Body.HSplitTop(8, nullptr, &Body);
			m_LucentPanelRendering = true;
			RenderLucentContext(Body, m_LucentBrowser.m_Scene.m_Visible == Scene::PLAYERS ? P::PLAYERS : P::DETAILS);
			m_LucentPanelRendering = false;
		}
	}
	else
	{
		CUIRect Hint;
		Atmosphere.HSplitBottom(52, nullptr, &Hint);
		Ui()->DoLabel(&Hint, Localize("Select a server to explore its map"), 12, TEXTALIGN_ML);
	}
	Ui()->SetEnabled(ContentEnabled);
	if(HasOverlay)
	{
		// Header-anchored reveal, clipped once; no competing list scroll input.
		OverlayBounds.Draw4(ColorRGBA(.025f, .021f, .035f, .97f), ColorRGBA(.025f, .021f, .035f, .93f), ColorRGBA(.025f, .021f, .035f, .88f), ColorRGBA(.025f, .021f, .035f, .65f), 0, 0);
		Ui()->ClipEnable(&OverlayBounds);
		CUIRect Content;
		OverlayBounds.Margin(9, &Content);
		if(Content.h > 30)
		{
			LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.Expansion() * m_LucentBrowser.m_Overlay.Opacity());
			LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Overlay.m_Target == VisibleOverlay);
			m_LucentPanelRendering = true;
			RenderLucentContext(Content, VisibleOverlay == Overlay::FILTERS ? P::FILTERS : P::DIRECT_CONNECT);
			m_LucentPanelRendering = false;
		}
		Ui()->ClipDisable();
	}
	CUIRect Advanced;
	Footer.VSplitRight(120, &Status, &Advanced);
	static int s_Legacy;
	if(LucentUi::TextButton(Ui(), TextRender(), &s_Legacy, Localize("Legacy view"), Advanced, false, 9, TEXTALIGN_MR))
		m_LucentLegacyBrowser = true;
	char aStatus[192];
	str_format(aStatus, sizeof(aStatus), "%d %s · %d %s   %s", ServerBrowser()->NumSortedServers(), Localize("servers"), ServerBrowser()->NumSortedPlayers(), Localize("players"), ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist() ? Localize("Refreshing...") : "");
	TextRender()->TextColor(ColorRGBA(.63f, .62f, .69f, 1));
	SLabelProperties StatusProps;
	StatusProps.m_MaxWidth = Status.w;
	StatusProps.m_StopAtEnd = true;
	Ui()->DoLabel(&Status, aStatus, 10, TEXTALIGN_ML, StatusProps);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	if(Activated && g_Config.m_UiServerAddress[0])
		Connect(g_Config.m_UiServerAddress);
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && (m_LucentBrowser.HasLocalState() || CLineInput::GetActiveInput()) && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		auto *pActive = CLineInput::GetActiveInput();
		if(!pActive || m_LucentBrowser.m_Overlay.m_Target == Overlay::DIRECT_CONNECT)
			m_LucentBrowser.Escape();
		if(pActive)
			pActive->Deactivate();
		Ui()->SetActiveItem(nullptr);
		m_LucentBrowser.m_Focus = 0;
	}
}
void CMenus::RenderLucentServerList(CUIRect View, bool &Activated)
{
	static CListBox s_List;
	s_List.SetColors(ColorRGBA(1, .8f, .9f, 0), ColorRGBA(1, .9f, .95f, 0));
	s_List.SetScrollbarWidth(10);
	s_List.SetScrollbarMargin(2);
	CUIRect Header;
	View.HSplitTop(19, &Header, &View);
	Header.VSplitRight(s_List.ScrollbarWidthMax(), &Header, nullptr);
	const float ColumnScale = std::min(1.f, Header.w / 350);
	const float Fixed = (20 + 90 + 54 + 40 + 30) * ColumnScale;
	const float MapWidth = std::max(0.f, Header.w - Fixed);
	const float aWidths[] = {20 * ColumnScale, MapWidth, 0, 90 * ColumnScale, 40 * ColumnScale, 54 * ColumnScale, 30 * ColumnScale};
	const char *apTitles[] = {"", "Map", "", "Type", "Region", "Players", "Ping"};
	const int aSort[] = {IServerBrowser::SORT_FAVORITES, IServerBrowser::SORT_MAP, -1, -1, -1, IServerBrowser::SORT_NUMPLAYERS, IServerBrowser::SORT_PING};
	static int s_aSortIds[7];
	CUIRect aCols[7];
	for(int c = 0; c < 7; ++c)
	{
		Header.VSplitLeft(aWidths[c], &aCols[c], &Header);
		if(aWidths[c] <= 0)
			continue;
		CUIRect Label = aCols[c];
		if(c != 0 && g_Config.m_BrSort == aSort[c])
			Label.VSplitRight(9, &Label, nullptr);
		if(LucentUi::TextButton(Ui(), TextRender(), &s_aSortIds[c], Localize(apTitles[c]), Label, false, 8, c >= 4 ? TEXTALIGN_MR : TEXTALIGN_ML) && aSort[c] >= 0)
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
	s_List.SetActive(!Ui()->IsPopupOpen() && m_LucentBrowser.m_Focus == 0 && CLineInput::GetActiveInput() == nullptr);
	if(m_ServerBrowserShouldRevealSelection)
	{
		s_List.ScrollToSelected();
		m_ServerBrowserShouldRevealSelection = false;
	}
	s_List.DoStart(17.f, Count, 1, 3, Selected, &View, false);
	const char *apRegions[] = {"--", "AF", "AS", "AU", "EU", "NA", "SA", "CN"};
	for(int i = 0; i < Count; ++i)
	{
		const auto *pInfo = ServerBrowser()->SortedGet(i);
		const CListboxItem Item = s_List.DoNextItem(&vIds[i], i == Selected, 0);
		if(!Item.m_Visible)
			continue;
		const float Hover = LucentUi::Highlight(pInfo, Item.m_Visible && Ui()->Enabled() && !Ui()->IsPopupOpen() && Ui()->MouseHovered(&Item.m_Rect));
		const float Selection = LucentUi::Highlight(pInfo->m_aAddress, i == Selected);
		if(Selection > .001f)
		{
			Item.m_Rect.Draw(LucentUi::Accent(.065f * Selection), 0, 0);
			CUIRect Marker = {Item.m_Rect.x, Item.m_Rect.y + 3, 1, Item.m_Rect.h - 6};
			Marker.Draw(LucentUi::Accent(.85f * Selection), 0, 0);
			if(i == Selected && m_LucentBrowser.m_SelectionPulse > 0)
			{
				CUIRect Echo = {Item.m_Rect.x + Item.m_Rect.w - 18, Item.m_Rect.y + 3, 18, Item.m_Rect.h - 6};
				const float Pulse = std::sin(m_LucentBrowser.m_SelectionPulse * 3.14159265f);
				Echo.Draw4(LucentUi::Accent(0), LucentUi::Accent(.09f * Pulse), LucentUi::Accent(0), LucentUi::Accent(.09f * Pulse), 0, 0);
			}
		}
		Item.m_Rect.Draw(ColorRGBA(1, .9f, .95f, .045f * Hover), IGraphics::CORNER_ALL, 2);
		if(!vpElements[i])
			vpElements[i] = Ui()->GetNewUIElement(14);
		char aPlayers[32], aPing[16];
		str_format(aPlayers, sizeof(aPlayers), "%d/%d", pInfo->m_NumFilteredPlayers, ServerBrowser()->Max(*pInfo));
		if(pInfo->m_LatencyIsEstimated)
			str_copy(aPing, "~");
		else
			str_format(aPing, sizeof(aPing), "%d", pInfo->m_Latency);
		char aDescriptor[128], aRegion[16];
		const auto Descriptor = CLucentBrowserState::Descriptor(pInfo->m_aName, str_comp(pInfo->m_aCommunityId, IServerBrowser::COMMUNITY_DDNET) == 0 ? pInfo->m_aCommunityType : "");
		str_copy(aDescriptor, Descriptor.data());
		const char *pRegion = pInfo->m_aCommunityCountry[0] && str_comp(pInfo->m_aCommunityCountry, IServerBrowser::COMMUNITY_COUNTRY_NONE) != 0 ? pInfo->m_aCommunityCountry : apRegions[std::clamp(pInfo->m_Location, 0, 7)];
		str_copy(aRegion, str_comp(pRegion, "RUS") == 0 ? "RU" : str_comp(pRegion, "GER") == 0 ? "DE" :
													 pRegion);
		const char *apText[] = {pInfo->m_Favorite != TRISTATE::NONE ? FontIcon::STAR : "", pInfo->m_aMap, pInfo->m_aCommunityId, aDescriptor, aRegion, aPlayers, aPing};
		for(int c = 0; c < 7; ++c)
		{
			if(aWidths[c] <= 0)
				continue;
			CUIRect Cell = {aCols[c].x + 4, Item.m_Rect.y, std::max(0.f, aCols[c].w - 8), Item.m_Rect.h};
			SLabelProperties Props;
			Props.m_MaxWidth = Cell.w;
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			TextRender()->TextColor(c == 1 ? ColorRGBA(.94f, .92f, .95f, 1) : ColorRGBA(.58f + .14f * Hover, .57f + .12f * Hover, .64f + .12f * Hover, 1));
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
