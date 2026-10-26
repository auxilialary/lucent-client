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
void CMenus::RenderLucentServerList(CUIRect View, bool &Activated)
{
	static CListBox s_List;
	s_List.SetColors(LucentUi::Accent(0), LucentUi::Accent(0));
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
	const float RegionWidth = MixedRegions ? std::max(44.f, (TextRender()->TextWidth(8, Localize("Region")) + 8) / ColumnScale) : 0.f;
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
			Item.m_Rect.Draw(LucentStyle::Style::ServerSelection.WithAlpha(LucentStyle::Style::ServerSelection.a * Selection), 0, 0);
		if(i != Selected)
		{
			CUIRect HoverLight = Item.m_Rect;
			HoverLight.w *= .72f;
			HoverLight.Draw4(LucentUi::Accent(.045f * Hover * (1 - Selection)), LucentUi::Accent(0), LucentUi::Accent(.045f * Hover * (1 - Selection)), LucentUi::Accent(0), 0, 0);
		}
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
					TextRender()->TextColor(LucentStyle::Style::UnavailableText);
					Ui()->DoLabel(&Mode, pMode, 9, TEXTALIGN_MR);
				}
			}
			SLabelProperties Props;
			Props.m_MaxWidth = Cell.w;
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			TextRender()->TextColor(c == 1 ? LucentStyle::Gradient::Blend(LucentStyle::Style::ServerText, LucentStyle::Style::ServerHover, std::max(Hover, Selection)) : LucentStyle::Gradient::Blend(LucentStyle::Style::MetadataText, LucentStyle::Style::MetadataHover, Hover));
			if(c == 0)
			{
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->TextColor(LucentStyle::Style::FavoriteText);
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
