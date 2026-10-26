#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>

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
		const float Reveal = m_LucentBrowser.FiltersOpacity();
		View.y += LucentStyle::Motion::Slide(LucentStyle::Motion::FilterRise, Reveal);
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
			RenderServerbrowserInfoScoreboard(View, pServer);
			return;
		}
		static CScrollRegion s_DetailsScroll;
		CScrollRegionParams DetailsParams;
		DetailsParams.m_ScrollbarThickness = 8;
		DetailsParams.m_ScrollbarMargin = 1;
		const float ContentHeight = 26 + 22 + 20 + 20 + 14 + 28 +
			((pServer->m_Flags & SERVER_FLAG_PASSWORD) ? 20 : 0) +
			(pServer->m_RequiresLogin ? 20 : 0) +
			(pServer->m_Favorite != TRISTATE::NONE ? 22 : 0);
		s_DetailsScroll.Begin(&View, &DetailsParams, ContentHeight);
		CUIRect Content = {View.x, View.y, View.w, ContentHeight};
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
		TextRender()->TextColor(LucentStyle::Style::DetailText);
		CUIRect AddressRow, CopyAction;
		View.HSplitTop(22, &AddressRow, &View);
		const float AddressWidth = std::min(TextRender()->TextWidth(10, pServer->m_aAddress) + 6, std::max(0.f, AddressRow.w - 24));
		AddressRow.VSplitLeft(AddressWidth, &Row, &CopyAction);
		CopyAction.w = 24;
		SLabelProperties AddressProps;
		AddressProps.m_MaxWidth = Row.w;
		AddressProps.m_StopAtEnd = true;
		Ui()->DoLabel(&Row, pServer->m_aAddress, 10, TEXTALIGN_ML, AddressProps);
		Ui()->DoToolTip(&pServer->m_aAddress, &Row, pServer->m_aAddress);
		static int s_Copy;
		if(LucentUi::IconButton(Ui(), TextRender(), &s_Copy, FontIcon::COPY, Localize("Copy address"), CopyAction))
			Input()->SetClipboardText(pServer->m_aAddress);
		str_format(aMeta, sizeof(aMeta), "%s: %s", Localize("Game type"), pServer->m_aGameType);
		Label(aMeta, 20, 10);
		str_format(aMeta, sizeof(aMeta), "%s: %s", Localize("Version"), pServer->m_aVersion);
		Label(aMeta, 20, 10);
		if(pServer->m_Flags & SERVER_FLAG_PASSWORD)
			Label(Localize("Password required"), 20, 10);
		if(pServer->m_RequiresLogin)
			Label(Localize("Login required"), 20, 10);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		View.HSplitTop(14, nullptr, &View);
		View.HSplitTop(28, &Row, &View);
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
		s_DetailsScroll.End();
	}
}
