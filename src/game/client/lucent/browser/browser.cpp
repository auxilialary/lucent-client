#include <game/client/lucent/browser/browser_ambient.h>
#include <game/client/lucent/browser/browser_layout.h>
#include <game/client/lucent/ui/scene_effects.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>

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
bool CMenus::LucentBrowserFiltersActive()
{
	return g_Config.m_BrFilterEmpty || g_Config.m_BrFilterFull || g_Config.m_BrFilterFriends ||
	       g_Config.m_BrFilterCountry || g_Config.m_BrFilterPw || g_Config.m_BrFilterUnfinishedMap ||
	       g_Config.m_BrFilterLogin != DefaultConfig::BrFilterLogin ||
	       g_Config.m_BrFilterGametype[0] || g_Config.m_BrFilterServerAddress[0] || g_Config.m_BrExcludeString[0] ||
	       !ServerBrowser()->CountriesFilter().Empty() || !ServerBrowser()->TypesFilter().Empty() || !ServerBrowser()->CommunitiesFilter().Empty();
}

const CServerInfo *CMenus::LucentSelectedServer()
{
	if(m_LucentBrowser.m_Primary.m_Target == CLucentBrowserState::EPrimary::COMMUNITIES)
		return nullptr;
	const CServerInfo *pServer = ServerBrowser()->SortedGet(m_SelectedIndex);
	if(pServer || m_LucentBrowser.m_Primary.m_Target != CLucentBrowserState::EPrimary::FRIENDS)
		return pServer;
	// Friends may be playing on a server excluded by the current list filters.
	const int Index = CLucentBrowserState::FindSelection(ServerBrowser()->NumServers(), g_Config.m_UiServerAddress, [this](int i) { return ServerBrowser()->Get(i)->m_aAddress; });
	return Index >= 0 ? ServerBrowser()->Get(Index) : nullptr;
}

void CMenus::RenderLucentBrowser(CUIRect Screen)
{
	LucentUi::CScope Style(Ui());
	using P = CLucentBrowserState::EPanel;
	using Primary = CLucentBrowserState::EPrimary;
	using Scene = CLucentBrowserState::EScene;
	using Overlay = CLucentBrowserState::EOverlay;
	Screen = *Ui()->Screen();
	if(!Ui()->IsPopupOpen())
		m_LucentCountrySearch.Deactivate();
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
	if(const char *pAddress = m_LucentBrowser.PrimarySelection(g_Config.m_UiServerAddress))
	{
		str_copy(g_Config.m_UiServerAddress, pAddress);
		m_ServerBrowserShouldRevealSelection = false;
	}
	m_SelectedIndex = CLucentBrowserState::FindSelection(ServerBrowser()->NumSortedServers(), g_Config.m_UiServerAddress, [this](int i) { return ServerBrowser()->SortedGet(i)->m_aAddress; });
	const CServerInfo *pSelected = LucentSelectedServer();
	char aPreviewIdentity[256] = "";
	if(pSelected)
		str_format(aPreviewIdentity, sizeof(aPreviewIdentity), "%s|%08x|%d", pSelected->m_aMap, unsigned(pSelected->m_MapCrc), pSelected->m_MapSize);
	m_LucentBrowser.Select(pSelected ? pSelected->m_aAddress : "", Animate, aPreviewIdentity);
	const char *pMapTitle = pSelected ? pSelected->m_aMap : "";
	if(m_LucentBrowser.m_MapTitle.m_Target != pMapTitle)
		m_LucentBrowser.m_MapTitle.m_Target = pMapTitle;
	m_LucentBrowser.m_MapTitle.Update(Dt, Animate, LucentStyle::Motion::MapTitle);
	m_LucentPreview.Update(pSelected, Now, Animate, Http(), Engine(), Graphics());
	Screen.Draw(LucentStyle::Style::BrowserBackground, 0, 0);
	static CLucentBrowserAmbient s_Ambient;
	const vec2 Cursor((Ui()->MouseX() - Screen.x) / Screen.w * 2 - 1, (Ui()->MouseY() - Screen.y) / Screen.h * 2 - 1);
	s_Ambient.Update(Dt, Animate, Cursor);
	s_Ambient.Render(Graphics(), Screen, m_LucentBrowser.m_SelectionPulse, m_LucentBrowser.BrowserOpacity());
	m_LucentPreview.Render(Graphics(), Screen, Now, Animate, m_LucentBrowser.BrowserOpacity(), m_LucentBrowser.ScenePosition(), m_LucentBrowser.PlayersPosition());
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
	const bool Activate = Keyboard && m_LucentBrowser.m_Overlay.m_Target == Overlay::NONE && m_LucentBrowser.m_Focus != 0 && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
	if(Activate && m_LucentBrowser.m_Focus == 1)
	{
		s_Search.Deactivate();
		Ui()->SetActiveItem(nullptr);
		m_LucentBrowser.m_Focus = 0;
	}
	s_Search.SetEmptyText(Localize("Search maps or players"));
	if(Keyboard && m_LucentBrowser.m_Overlay.m_Target == Overlay::NONE && !m_LucentBrowser.FiltersWorkspace() && Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
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
	const auto PreviousOverlay = m_LucentBrowser.m_Overlay.m_Target;
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
	const float FiltersProgress = m_LucentBrowser.FiltersExpansion();
	const CUIRect FilterTitle = Nav;
	Nav.y -= LucentStyle::Motion::FilterNavTravel * FiltersProgress;
	{
		LucentUi::COpacityScope Opacity(TextRender(), 1 - FiltersProgress);
		LucentUi::CInteractionScope Interaction(Ui(), !m_LucentBrowser.FiltersWorkspace());
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
				if(auto *pActive = CLineInput::GetActiveInput())
					pActive->Deactivate();
				Ui()->SetActiveItem(nullptr);
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
	}
	if(FiltersProgress > 0)
	{
		LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.FiltersOpacity());
		CUIRect Title = FilterTitle;
		Title.y += LucentStyle::Motion::Slide(LucentStyle::Motion::FilterRise, FiltersProgress);
		Title.x = Screen.x;
		Title.w = Screen.w;
		Ui()->DoLabel(&Title, Localize("Filters"), 18, TEXTALIGN_MC);
		const float Width = CLucentBrowserFilterLayout::WorkspaceWidth(std::max(0.f, Screen.w - 24));
		CUIRect Line = {Screen.x + (Screen.w - Width) / 2, Title.y + Title.h + 3, Width, LucentStyle::Style::Hairline};
		LucentUi::Divider(Line);
	}
	const float DirectAnchor = Tools.x + 84;
	static int s_aTools[4];
	const char *apIcons[] = {FontIcon::LIST_UL, FontIcon::ARROWS_ROTATE, FontIcon::CHEVRON_RIGHT, FontIcon::HOUSE};
	const char *apHints[] = {"Filters", "Refresh", "Direct connect", "Main menu"};
	for(int i = 0; i < 4; ++i)
	{
		CUIRect Button;
		Tools.VSplitLeft(28, &Button, &Tools);
		Button.Margin(2, &Button);
		const bool Click = LucentUi::IconButton(Ui(), TextRender(), &s_aTools[i], apIcons[i], Localize(apHints[i]), Button, i == 0 && m_LucentBrowser.m_Overlay.m_Target == Overlay::FILTERS, m_LucentBrowser.TooltipsAllowed());
		if(i == 0 && LucentBrowserFiltersActive())
		{
			CUIRect Dot = {Button.x + Button.w - 4, Button.y + 3, 2.5f, 2.5f};
			Dot.Draw(LucentUi::Accent(.8f), IGraphics::CORNER_ALL, 2);
		}
		if(i == 0 && (Click || (Activate && m_LucentBrowser.m_Focus == 2)))
		{
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::FILTERS);
			if(m_LucentBrowser.m_Overlay.m_Target == Overlay::NONE)
			{
				if(auto *pActive = CLineInput::GetActiveInput())
					pActive->Deactivate();
				Ui()->SetActiveItem(nullptr);
			}
		}
		if(i == 1 && (Click || (Keyboard && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && !Input()->ShiftIsPressed() && Input()->KeyPress(KEY_R))))))
			RefreshBrowserTab(true);
		if(i == 2 && (Click || (Activate && m_LucentBrowser.m_Focus >= 5)))
		{
			if(m_LucentDirectAddress.IsEmpty())
				m_LucentDirectAddress.Set(g_Config.m_UiServerAddress);
			m_LucentDirectAddress.SelectAll();
			Ui()->OnReset();
			m_LucentBrowser.Toggle(CLucentBrowserState::EPanel::DIRECT_CONNECT);
			m_LucentBrowser.m_DirectFocusPending = m_LucentBrowser.m_Overlay.m_Target == Overlay::DIRECT_CONNECT;
			Ui()->SetActiveItem(nullptr);
		}
		if(i == 3 && Click)
			SetShowStart(true);
	}
	if(PreviousOverlay != Overlay::NONE && m_LucentBrowser.m_Overlay.m_Target == Overlay::NONE)
	{
		if(auto *pInput = CLineInput::GetActiveInput(); pInput && pInput != &s_Search)
		{
			pInput->Deactivate();
			Ui()->SetActiveItem(nullptr);
		}
	}
	// Fixed regions share the background, with independent contextual state.
	CUIRect Atmosphere;
	Screen.VSplitLeft(CLucentBrowserSceneLayout::ListWidth(*Ui()->Screen()), &List, nullptr);
	Atmosphere = CLucentBrowserSceneLayout::SceneBounds(*Ui()->Screen());
	Atmosphere.y = Screen.y;
	Atmosphere.h = Screen.h;
	CUIRect OverlayBounds = {Screen.x, Top.y + Top.h + 4, Screen.w, 0};
	const auto VisibleOverlay = m_LucentBrowser.m_Overlay.m_Visible;
	const bool Workspace = m_LucentBrowser.FiltersWorkspace();
	const float BrowserAlpha = m_LucentBrowser.BrowserOpacity();
	const bool HasOverlay = (m_LucentBrowser.m_Expansion > 0 || m_LucentBrowser.m_FiltersExpansion > 0) && VisibleOverlay != Overlay::NONE;
	if(HasOverlay)
	{
		OverlayBounds.h = VisibleOverlay == Overlay::FILTERS ? Screen.y + Screen.h - OverlayBounds.y : 98.f * m_LucentBrowser.Expansion();
		if(VisibleOverlay == Overlay::DIRECT_CONNECT)
		{
			OverlayBounds.w = std::min(250.f, Screen.w);
			OverlayBounds.x = std::clamp(DirectAnchor - OverlayBounds.w, Screen.x, Screen.x + Screen.w - OverlayBounds.w);
		}
		if(!Workspace && Keyboard && Ui()->MouseButtonClicked(0) && !Ui()->MouseHovered(&OverlayBounds) && m_LucentBrowser.m_Overlay.m_Target == PreviousOverlay)
		{
			m_LucentBrowser.m_Overlay.m_Target = Overlay::NONE;
			if(auto *pInput = CLineInput::GetActiveInput(); pInput && pInput != &s_Search)
			{
				pInput->Deactivate();
				Ui()->SetActiveItem(nullptr);
			}
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
	const bool BlockContent = Workspace || HasOverlay || m_LucentBrowser.m_Overlay.m_Target != Overlay::NONE;
	const bool ContentEnabled = Ui()->Enabled();
	Ui()->SetEnabled(ContentEnabled && !BlockContent);
	bool Activated = false;
	if(BrowserAlpha > 0)
	{
		LucentUi::COpacityScope BrowserOpacity(TextRender(), BrowserAlpha);
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
		if(m_LucentBrowser.m_Primary.m_Visible == Primary::SERVERS && m_LucentBrowser.m_SelectionY > 0)
		{
			m_LucentBrowser.m_RowIndicator.Update(m_LucentBrowser.m_SelectionY, 12, Dt, Animate);
			const float MarkerY = std::clamp(m_LucentBrowser.m_RowIndicator.X(), List.y + 25, List.y + List.h - 6);
			CUIRect Marker = {List.x, MarkerY - 6, 1, 12};
			Marker.Draw(LucentUi::Accent(.85f), 0, 0);
			const vec2 From(List.x + List.w - 3, MarkerY);
			const vec2 To(Atmosphere.x + 12, Ui()->Screen()->y + Ui()->Screen()->h * .40f);
			CLucentMenuEffects::SelectionTransfer(Graphics(), From, To, 1 - m_LucentBrowser.m_SelectionPulse, BrowserAlpha);
		}
		if(pSelected)
		{
			// Artwork and the complete header share fixed anchors across all modes.
			const float Position = m_LucentBrowser.ScenePosition();
			const auto SceneLayout = CLucentBrowserSceneLayout::Measure(*Ui()->Screen(), Position, m_LucentBrowser.PlayersPosition());
			CUIRect Title = {Atmosphere.x, SceneLayout.m_TitleY, Atmosphere.w, 34};
			SLabelProperties Props;
			Props.m_MaxWidth = Title.w;
			Props.m_StopAtEnd = true;
			{
				LucentUi::COpacityScope SelectionOpacity(TextRender(), m_LucentBrowser.m_MapTitle.Opacity());
				Ui()->DoLabel(&Title, m_LucentBrowser.m_MapTitle.m_Visible.c_str(), 25, TEXTALIGN_ML, Props);
				Ui()->DoToolTip(&m_LucentBrowser.m_MapTitle, &Title, m_LucentBrowser.m_MapTitle.m_Visible.c_str());
			}
			CUIRect Body = {Atmosphere.x, Title.y + Title.h + 8, Atmosphere.w, std::max(0.f, Atmosphere.y + Atmosphere.h - Title.y - Title.h - 8)};

			CUIRect Meta, Action, ConnectButton, Details, Players;
			Body.HSplitTop(22, &Meta, &Body);
			char aMeta[128];
			str_format(aMeta, sizeof(aMeta), "%s%s%d/%d %s · %s%d ms", pSelected->m_aCommunityCountry, pSelected->m_aCommunityCountry[0] ? " · " : "", pSelected->m_NumPlayers, pSelected->m_MaxPlayers, Localize("players"), pSelected->m_LatencyIsEstimated ? "~" : "", pSelected->m_Latency);
			TextRender()->TextColor(LucentStyle::Style::BrowserHint);
			Ui()->DoLabel(&Meta, aMeta, 11, TEXTALIGN_ML, Props);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Body.HSplitTop(6, nullptr, &Body);
			Body.HSplitTop(32, &Action, &Body);
			const float ConnectWidth = std::max(130.f, TextRender()->TextWidth(12, Localize("Connect")) + 24);
			Action.VSplitLeft(std::min(ConnectWidth, Action.w), &ConnectButton, nullptr);
			static int s_Connect, s_Details, s_Players;
			if(LucentUi::SceneButton(Ui(), TextRender(), &s_Connect, Localize("Connect"), ConnectButton))
				Connect(pSelected->m_aAddress);
			Body.HSplitTop(9, nullptr, &Body);
			Body.HSplitTop(25, &Action, &Body);
			const float DetailsWidth = TextRender()->TextWidth(10, Localize("Server details")) + 18;
			Action.VSplitLeft(std::min(DetailsWidth, Action.w * .65f), &Details, &Players);
			Players.w = std::min(Players.w, TextRender()->TextWidth(10, Localize("Players")) + 18);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Details, Localize("Server details"), Details, m_LucentBrowser.m_Scene.m_Target == Scene::DETAILS, 10, TEXTALIGN_ML, false) || (Activate && m_LucentBrowser.m_Focus == 3))
				m_LucentBrowser.Toggle(P::DETAILS);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Players, Localize("Players"), Players, m_LucentBrowser.m_Scene.m_Target == Scene::PLAYERS, 10, TEXTALIGN_ML, false) || (Activate && m_LucentBrowser.m_Focus == 4))
				m_LucentBrowser.Toggle(P::PLAYERS);
			if(m_LucentBrowser.m_Scene.m_Target != Scene::PREVIEW)
			{
				const CUIRect ActiveTab = m_LucentBrowser.m_Scene.m_Target == Scene::PLAYERS ? Players : Details;
				m_LucentBrowser.m_DetailNavigation.Update(ActiveTab.x, ActiveTab.w, Dt, Animate);
				CUIRect Line = {m_LucentBrowser.m_DetailNavigation.X(), Action.y + Action.h - 1, m_LucentBrowser.m_DetailNavigation.Width(), LucentStyle::Style::Hairline};
				LucentUi::Divider(Line, .7f);
			}
			Body.HSplitTop(8, nullptr, &Body);
			if(m_LucentBrowser.m_Scene.m_Visible != Scene::PREVIEW && Body.h > 0)
			{
				const float Opacity = m_LucentBrowser.m_Scene.Opacity();
				const float Offset = LucentStyle::Motion::Slide(4, Opacity);
				Body.y += Offset;
				Body.h = std::max(0.f, Body.h - Offset);
				LucentUi::COpacityScope ContentOpacity(TextRender(), Opacity);
				LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Scene.m_Target == m_LucentBrowser.m_Scene.m_Visible && Opacity > .95f);
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
	}
	Ui()->SetEnabled(ContentEnabled);
	if(HasOverlay)
	{
		// Header-anchored reveal, clipped once; no competing list scroll input.
		LucentUi::COpacityScope OverlayOpacity(TextRender(), VisibleOverlay == Overlay::FILTERS ? m_LucentBrowser.FiltersOpacity() : m_LucentBrowser.m_Overlay.Opacity());
		if(VisibleOverlay != Overlay::FILTERS)
			LucentUi::Surface(OverlayBounds);
		Ui()->ClipEnable(&OverlayBounds);
		CUIRect Content;
		OverlayBounds.Margin(12, &Content);
		if(VisibleOverlay == Overlay::FILTERS && Content.w > 820)
			Content.VMargin((Content.w - 820) / 2, &Content);
		if(Content.h > 30 && (VisibleOverlay != Overlay::FILTERS || m_LucentBrowser.FiltersOpacity() > 0))
		{
			LucentUi::COpacityScope Opacity(TextRender(), VisibleOverlay == Overlay::FILTERS ? 1.f : m_LucentBrowser.Expansion());
			LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Overlay.m_Target == VisibleOverlay && (VisibleOverlay == Overlay::FILTERS ? m_LucentBrowser.FiltersOpacity() : m_LucentBrowser.Expansion()) > .95f);
			m_LucentPanelRendering = true;
			RenderLucentContext(Content, VisibleOverlay == Overlay::FILTERS ? P::FILTERS : P::DIRECT_CONNECT);
			m_LucentPanelRendering = false;
		}
		Ui()->ClipDisable();
	}
	if(!m_LucentBrowser.TooltipsAllowed() || Ui()->IsPopupOpen())
		Ui()->OnReset();
	Footer.y += LucentStyle::Motion::FilterFooterTravel * FiltersProgress;
	Status = Footer;
	LucentUi::COpacityScope FooterOpacity(TextRender(), 1 - FiltersProgress);
	char aStatus[192];
	str_format(aStatus, sizeof(aStatus), "%d %s · %d %s   %s", ServerBrowser()->NumSortedServers(), Localize("servers"), ServerBrowser()->NumSortedPlayers(), Localize("players"), ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist() ? Localize("Refreshing...") : "");
	TextRender()->TextColor(LucentStyle::Style::SecondaryText);
	SLabelProperties StatusProps;
	StatusProps.m_MaxWidth = Status.w;
	StatusProps.m_StopAtEnd = true;
	Ui()->DoLabel(&Status, aStatus, 10, TEXTALIGN_ML, StatusProps);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	if(!BlockContent && Keyboard && !CLineInput::GetActiveInput() && m_LucentBrowser.m_Focus == 0 && pSelected && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER))
		Activated = true;
	if(Activated && g_Config.m_UiServerAddress[0])
		Connect(g_Config.m_UiServerAddress);
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && (m_LucentBrowser.HasLocalState() || CLineInput::GetActiveInput()) && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		auto *pActive = CLineInput::GetActiveInput();
		if(!pActive || m_LucentBrowser.m_Overlay.m_Target != Overlay::NONE)
			m_LucentBrowser.Escape();
		if(pActive)
			pActive->Deactivate();
		Ui()->SetActiveItem(nullptr);
		m_LucentBrowser.m_Focus = 0;
	}
}
