#include "browser_ambient.h"
#include "lucent_menu_effects.h"
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
bool CMenus::LucentBrowserFiltersActive()
{
	return g_Config.m_BrFilterEmpty || g_Config.m_BrFilterFull || g_Config.m_BrFilterFriends ||
	       g_Config.m_BrFilterCountry || g_Config.m_BrFilterPw || g_Config.m_BrFilterUnfinishedMap ||
	       g_Config.m_BrFilterLogin != DefaultConfig::BrFilterLogin ||
	       g_Config.m_BrFilterGametype[0] || g_Config.m_BrFilterServerAddress[0] || g_Config.m_BrExcludeString[0] ||
	       !ServerBrowser()->CountriesFilter().Empty() || !ServerBrowser()->TypesFilter().Empty() || !ServerBrowser()->CommunitiesFilter().Empty();
}

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
	m_LucentBrowser.m_MapTitle.Update(Dt, Animate, .42f);
	m_LucentPreview.Update(pSelected, Now, Animate, Http(), Engine(), Graphics());
	Screen.Draw(ColorRGBA(.010f, .009f, .014f, 1), 0, 0);
	static CLucentBrowserAmbient s_Ambient;
	const vec2 Cursor((Ui()->MouseX() - Screen.x) / Screen.w * 2 - 1, (Ui()->MouseY() - Screen.y) / Screen.h * 2 - 1);
	s_Ambient.Update(Dt, Animate, Cursor);
	s_Ambient.Render(Graphics(), Screen, m_LucentBrowser.m_SelectionPulse, m_LucentBrowser.BrowserOpacity());
	m_LucentPreview.Render(Graphics(), Screen, Now, Animate, m_LucentBrowser.BrowserOpacity());
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
		if(i == 1 && (Click || (Keyboard && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R))))))
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
	Screen.VSplitLeft(Screen.w * .41f, &List, &Atmosphere);
	Atmosphere.VSplitLeft(38, nullptr, &Atmosphere);
	CUIRect OverlayBounds = {Screen.x, Top.y + Top.h + 4, Screen.w, 0};
	const auto VisibleOverlay = m_LucentBrowser.m_Overlay.m_Visible;
	const bool Workspace = m_LucentBrowser.FiltersWorkspace();
	const float BrowserAlpha = m_LucentBrowser.BrowserOpacity();
	const bool HasOverlay = m_LucentBrowser.m_Expansion > 0 && VisibleOverlay != Overlay::NONE;
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
			const vec2 From(List.x + List.w - 3, m_LucentBrowser.m_SelectionY);
			const vec2 To(Atmosphere.x + 42, mix(From.y, Atmosphere.y + Atmosphere.h * .45f, .2f));
			CLucentMenuEffects::SelectionTransfer(Graphics(), From, To, 1 - m_LucentBrowser.m_SelectionPulse, BrowserAlpha);
		}
		if(pSelected)
		{
			// The same map title travels between preview and context, never duplicated.
			const float Position = m_LucentBrowser.ScenePosition();
			// A spatial scrim subdues the same artwork beneath contextual text.
			Atmosphere.Draw4(ColorRGBA(.010f, .009f, .016f, .83f * Position), ColorRGBA(.010f, .009f, .016f, .57f * Position), ColorRGBA(.010f, .009f, .016f, .96f * Position), ColorRGBA(.010f, .009f, .016f, .76f * Position), 0, 0);
			const float PreviewY = Screen.y + Screen.h * .56f;
			CUIRect Title = {Atmosphere.x, PreviewY + (Atmosphere.y - PreviewY) * Position, Atmosphere.w, 38};
			SLabelProperties Props;
			Props.m_MaxWidth = Title.w;
			Props.m_StopAtEnd = true;
			{
				LucentUi::COpacityScope SelectionOpacity(TextRender(), m_LucentBrowser.m_Scene.m_Visible == Scene::PREVIEW ? m_LucentBrowser.m_MapTitle.Opacity() : 1.f);
				Ui()->DoLabel(&Title, m_LucentBrowser.m_Scene.m_Visible == Scene::PREVIEW ? m_LucentBrowser.m_MapTitle.m_Visible.c_str() : pSelected->m_aMap, 28 - 5 * Position, TEXTALIGN_ML, Props);
			}
			CUIRect Body = {Atmosphere.x, Title.y + Title.h + 8, Atmosphere.w, std::max(0.f, Atmosphere.y + Atmosphere.h - Title.y - Title.h - 8)};

			if(m_LucentBrowser.m_Scene.m_Visible == Scene::PREVIEW)
			{
				LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.m_Scene.Opacity());
				LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Scene.m_Target == Scene::PREVIEW && m_LucentBrowser.m_Scene.Opacity() > .95f);
				CUIRect Meta, Action, ConnectButton, Details, Players;
				Body.HSplitTop(22, &Meta, &Body);
				char aMeta[128];
				str_format(aMeta, sizeof(aMeta), "%s%s%d/%d %s · %s%d ms", pSelected->m_aCommunityCountry, pSelected->m_aCommunityCountry[0] ? " · " : "", pSelected->m_NumPlayers, pSelected->m_MaxPlayers, Localize("players"), pSelected->m_LatencyIsEstimated ? "~" : "", pSelected->m_Latency);
				TextRender()->TextColor(ColorRGBA(.77f, .71f, .76f, 1));
				Ui()->DoLabel(&Meta, aMeta, 12, TEXTALIGN_ML, Props);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				Body.HSplitTop(12, nullptr, &Body);
				Body.HSplitTop(30, &Action, &Body);
				Action.VSplitLeft(std::min(120.f, Action.w * .5f), &ConnectButton, nullptr);
				Body.HSplitTop(7, nullptr, &Body);
				Body.HSplitTop(22, &Action, &Body);
				Action.VSplitLeft(std::min(135.f, Action.w * .55f), &Details, &Players);
				static int s_Connect, s_Details, s_Players;
				if(LucentUi::SceneButton(Ui(), TextRender(), &s_Connect, Localize("Connect"), ConnectButton))
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
				if(LucentUi::TextButton(Ui(), TextRender(), &s_Details, Localize("Server details"), Details, m_LucentBrowser.m_Scene.m_Target == Scene::DETAILS, 10, TEXTALIGN_MC, false))
					m_LucentBrowser.m_Scene.m_Target = Scene::DETAILS;
				if(LucentUi::TextButton(Ui(), TextRender(), &s_Players, Localize("Players"), Players, m_LucentBrowser.m_Scene.m_Target == Scene::PLAYERS, 10, TEXTALIGN_MC, false))
					m_LucentBrowser.m_Scene.m_Target = Scene::PLAYERS;
				const CUIRect ActiveSceneTab = m_LucentBrowser.m_Scene.m_Target == Scene::PLAYERS ? Players : Details;
				m_LucentBrowser.m_DetailNavigation.Update(ActiveSceneTab.x + 5, ActiveSceneTab.w - 10, Dt, Animate);
				CUIRect TabLine = {m_LucentBrowser.m_DetailNavigation.X(), Tabs.y + Tabs.h - 1, m_LucentBrowser.m_DetailNavigation.Width(), .7f};
				TabLine.Draw(LucentUi::Accent(.7f), 0, 0);
				Body.HSplitTop(8, nullptr, &Body);
				LucentUi::COpacityScope Opacity(TextRender(), m_LucentBrowser.m_Scene.Opacity());
				LucentUi::CInteractionScope Interaction(Ui(), m_LucentBrowser.m_Scene.m_Target == m_LucentBrowser.m_Scene.m_Visible && m_LucentBrowser.m_Scene.Opacity() > .95f);
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
	CUIRect Advanced;
	Footer.VSplitRight(120, &Status, &Advanced);
	static int s_Legacy;
	if(Workspace && m_LucentBrowser.FiltersOpacity() == 1 && LucentUi::TextButton(Ui(), TextRender(), &s_Legacy, Localize("Legacy view"), Advanced, false, 9, TEXTALIGN_MR))
		m_LucentLegacyBrowser = true;
	char aStatus[192];
	str_format(aStatus, sizeof(aStatus), "%d %s · %d %s   %s", ServerBrowser()->NumSortedServers(), Localize("servers"), ServerBrowser()->NumSortedPlayers(), Localize("players"), ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist() ? Localize("Refreshing...") : "");
	TextRender()->TextColor(ColorRGBA(.63f, .62f, .69f, 1));
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
	const int Count = ServerBrowser()->NumSortedServers();
	bool MixedRegions = false;
	for(int i = 1; i < Count && !MixedRegions; ++i)
	{
		const auto *pFirst = ServerBrowser()->SortedGet(0);
		const auto *pNext = ServerBrowser()->SortedGet(i);
		MixedRegions = str_comp(pFirst->m_aCommunityCountry, pNext->m_aCommunityCountry) != 0 || pFirst->m_Location != pNext->m_Location;
	}
	const float RegionWidth = MixedRegions ? 30.f : 0.f;
	const float Fixed = (16 + 48 + RegionWidth + 28) * ColumnScale;
	const float MapWidth = std::max(0.f, Header.w - Fixed);
	const float aWidths[] = {16 * ColumnScale, MapWidth, 0, 0, RegionWidth * ColumnScale, 48 * ColumnScale, 28 * ColumnScale};
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
	auto &vpElements = m_avpServerBrowserUiElements[ServerBrowser()->GetCurrentType()];
	if(vpElements.size() < size_t(Count))
		vpElements.resize(Count, nullptr);
	const int Selected = CLucentBrowserState::FindSelection(Count, g_Config.m_UiServerAddress, [this](int i) { return ServerBrowser()->SortedGet(i)->m_aAddress; });
	// Grow stable item storage only when server count grows, never per frame.
	auto &vIds = m_vServerbrowserElementIds[ServerBrowser()->GetCurrentType()];
	if(vIds.size() < size_t(Count))
		vIds.resize(Count);
	s_List.SetActive(Ui()->Enabled() && !Ui()->IsPopupOpen() && m_LucentBrowser.m_Focus == 0 && CLineInput::GetActiveInput() == nullptr);
	if(m_ServerBrowserShouldRevealSelection)
	{
		s_List.ScrollToSelected();
		m_ServerBrowserShouldRevealSelection = false;
	}
	s_List.DoStart(18.f, Count, 1, 3, Selected, &View, false);
	m_LucentBrowser.m_SelectionY = -1;
	const char *apRegions[] = {"--", "AF", "AS", "AU", "EU", "NA", "SA", "CN"};
	for(int i = 0; i < Count; ++i)
	{
		const auto *pInfo = ServerBrowser()->SortedGet(i);
		const CListboxItem Item = s_List.DoNextItem(&vIds[i], i == Selected, 0);
		if(!Item.m_Visible)
			continue;
		if(Ui()->Enabled() && !Ui()->IsPopupOpen() && Ui()->MouseHovered(&Item.m_Rect) && Ui()->MouseButtonClicked(1))
		{
			static SPopupMenuId s_ContextId;
			static SLucentServerPopup s_Context;
			s_Context = {this, pInfo->m_aAddress};
			str_copy(g_Config.m_UiServerAddress, pInfo->m_aAddress);
			m_LucentBrowser.m_Focus = 0;
			if(auto *pInput = CLineInput::GetActiveInput())
				pInput->Deactivate();
			Ui()->DoPopupMenu(&s_ContextId, Ui()->MouseX(), Ui()->MouseY(), 180, 126, &s_Context, PopupLucentServer);
		}
		const float Hover = LucentUi::Highlight(pInfo, Item.m_Visible && Ui()->Enabled() && !Ui()->IsPopupOpen() && Ui()->MouseHovered(&Item.m_Rect));
		const float Selection = LucentUi::Highlight(pInfo->m_aAddress, i == Selected);
		if(Selection > .001f)
		{
			Item.m_Rect.Draw4(LucentUi::Accent(.025f * Selection), LucentUi::Accent(.07f * Selection), LucentUi::Accent(.025f * Selection), LucentUi::Accent(.015f * Selection), 0, 0);
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
		if(i == Selected)
			m_LucentBrowser.m_SelectionY = Item.m_Rect.y + Item.m_Rect.h * .5f;
		if(!vpElements[i])
			vpElements[i] = Ui()->GetNewUIElement(14);
		char aPlayers[32], aPing[16];
		str_format(aPlayers, sizeof(aPlayers), "%d/%d", pInfo->m_NumFilteredPlayers, ServerBrowser()->Max(*pInfo));
		if(pInfo->m_LatencyIsEstimated)
			str_copy(aPing, "~");
		else
			str_format(aPing, sizeof(aPing), "%d", pInfo->m_Latency);
		char aRegion[16];
		const char *pMode = pInfo->m_aCommunityType;
		const bool HasMode = str_comp(pInfo->m_aCommunityId, IServerBrowser::COMMUNITY_DDNET) == 0 && pMode[0] && str_comp(pMode, "None") != 0 && str_comp(pMode, "DDNet") != 0;

		const char *pRegion = pInfo->m_aCommunityCountry[0] && str_comp(pInfo->m_aCommunityCountry, IServerBrowser::COMMUNITY_COUNTRY_NONE) != 0 ? pInfo->m_aCommunityCountry : apRegions[std::clamp(pInfo->m_Location, 0, 7)];
		str_copy(aRegion, str_comp(pRegion, "RUS") == 0 ? "RU" : str_comp(pRegion, "GER") == 0 ? "DE" :
													 pRegion);
		const char *apText[] = {pInfo->m_Favorite != TRISTATE::NONE ? FontIcon::STAR : "", pInfo->m_aMap, pInfo->m_aCommunityId, "", aRegion, aPlayers, aPing};
		for(int c = 0; c < 7; ++c)
		{
			if(aWidths[c] <= 0)
				continue;
			CUIRect Cell = {aCols[c].x + 4, Item.m_Rect.y, std::max(0.f, aCols[c].w - 8), Item.m_Rect.h};
			if(c == 1 && HasMode)
			{
				const float ModeWidth = TextRender()->TextWidth(9, pMode) + 10;
				if(TextRender()->TextWidth(11, pInfo->m_aMap) + ModeWidth + 18 < Cell.w)
				{
					CUIRect Mode;
					Cell.VSplitRight(ModeWidth, &Cell, &Mode);
					TextRender()->TextColor(ColorRGBA(.64f, .58f, .64f, 1));
					Ui()->DoLabel(&Mode, pMode, 9, TEXTALIGN_MR);
				}
			}
			SLabelProperties Props;
			Props.m_MaxWidth = Cell.w;
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			TextRender()->TextColor(c == 1 ? ColorRGBA(.86f + .12f * std::max(Hover, Selection), .84f + .12f * std::max(Hover, Selection), .88f + .10f * std::max(Hover, Selection), 1) : ColorRGBA(.58f + .14f * Hover, .57f + .12f * Hover, .64f + .12f * Hover, 1));
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
		Ui()->DoToolTip(&vIds[i], &Item.m_Rect, pInfo->m_aAddress);
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
