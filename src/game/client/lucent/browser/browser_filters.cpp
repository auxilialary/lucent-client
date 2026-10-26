#include "browser_layout.h"

#include <engine/shared/config.h>
#include <engine/shared/localization.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

void CMenus::RenderLucentBrowserFilterControls(CUIRect &View, CScrollRegion &Scroll, bool CountriesTab, bool LayoutFinished, float &GridHeight, CLineInput (&s_CompactInputs)[3], LucentStyle::Motion::CTransition (&s_aFloatingLabels)[3])
{
	const auto &Clock = LucentStyle::Motion::Clock();
	const int Counts[] = {4, 5};
	int *apValues[] = {&g_Config.m_BrFilterEmpty, &g_Config.m_BrFilterFull, &g_Config.m_BrFilterUnfinishedMap, &g_Config.m_BrFilterGametypeStrict,
		&g_Config.m_BrFilterPw, &g_Config.m_BrFilterLogin, &g_Config.m_BrFilterFriends, &g_Config.m_BrFilterConnectingPlayers, &g_Config.m_BrFilterSpectators};
	const char *apLabels[] = {Localizable("Has people playing"), Localizable("Server not full"), Localizable("Unfinished map"), Localizable("Strict gametype filter"),
		Localizable("No password"), Localizable("No login required"), Localizable("Show friends only"), Localizable("Filter connecting players"), Localizable("Count players only")};
	const char *apGroups[] = {Localizable("Servers and maps"), Localizable("Access and players")};
	float Minimum = 0;
	for(const char *pLabel : apLabels)
		Minimum = std::max(Minimum, TextRender()->TextWidth(11, Localize(pLabel)) + LucentStyle::Style::ControlSize + LucentStyle::Style::LabelGap + 8);
	const auto Measure = [&](float Width) {
		return CLucentBrowserFilterLayout::Measure(Width,
			ServerBrowser()->CommunityCache().SelectableCountries().size(),
			ServerBrowser()->CommunityCache().SelectableTypes().size(),
			CountriesTab,
			ServerBrowser()->CommunityCache().CountriesTypesFilterAvailable(),
			ServerBrowser()->CommunityCache().AnyRanksAvailable(), LayoutFinished, Minimum);
	};
	auto Layout = Measure(View.w);
	if(std::ceil(Layout.m_ContentHeight) > View.h)
		Layout = Measure(View.w - 8);
	GridHeight = Layout.m_GridHeight;
	CScrollRegionParams Params;
	Params.m_ScrollbarThickness = 8.0f;
	Params.m_ScrollbarMargin = 1.0f;
	Params.m_ManualScrollInput = true;
	Params.m_ScrollUnit = 3 * 21.0f;
	Scroll.Begin(&View, &Params, std::ceil(Layout.m_ContentHeight));
	View.h = Layout.m_ContentHeight;
	Scroll.AddRect(View);
	CUIRect Groups;
	View.HSplitTop(CLucentBrowserFilterLayout::CheckboxHeight(View.w, Minimum), &Groups, &View);
	int Offset = 0;
	for(int Group = 0; Group < 2; ++Group)
	{
		CUIRect Block = Groups;
		if(Layout.m_Wide)
			Block = CLucentBrowserFilterLayout::Column(Groups, Group);
		else
		{
			Groups.HSplitTop(CLucentBrowserFilterLayout::GroupHeight(Groups.w, Counts[Group], Minimum), &Block, &Groups);
			if(Group == 0)
				Groups.HSplitTop(8, nullptr, &Groups);
		}
		CUIRect Heading, Line, Options;
		Block.HSplitTop(18, &Heading, &Block);
		Ui()->DoLabel(&Heading, Localize(apGroups[Group]), 11, TEXTALIGN_MC);
		Block.HSplitTop(4, nullptr, &Block);
		Block.HSplitTop(.5f, &Line, &Block);
		LucentUi::Divider(Line);
		Block.HSplitTop(5.5f, nullptr, &Options);
		const int Columns = CLucentBrowserFilterLayout::QuickColumns(Options.w, Minimum);
		for(int i = 0; i < Counts[Group]; ++i)
		{
			const int Index = Offset + i;
			CUIRect Toggle = {Options.x + (i % Columns) * Options.w / Columns, Options.y + (i / Columns) * 25, Options.w / Columns - 8, 22};
			// Saved rank-lookup consent remains authoritative.
			LucentUi::CInteractionScope Interaction(Ui(), Index != 2 || (LayoutFinished && ServerBrowser()->CommunityCache().AnyRanksAvailable()));
			if(LucentUi::FilledCheckBox(Ui(), apValues[Index], Localize(apLabels[Index]), *apValues[Index] != 0, Toggle))
				*apValues[Index] ^= 1;
			if(Index == 2 && !LayoutFinished)
				Ui()->DoToolTip(apValues[Index], &Toggle, Localize("Enable map finish indication to filter unfinished maps."));
		}
		Offset += Counts[Group];
	}
	CUIRect TextHeading;
	View.HSplitTop(26, &TextHeading, &View);
	Ui()->DoLabel(&TextHeading, Localize("Text filters"), 11, TEXTALIGN_MC);
	CUIRect Compact;
	View.HSplitTop(CLucentBrowserFilterLayout::InputHeight(View.w), &Compact, &View);
	const char *apNames[] = {Localizable("Exclusions"), Localizable("Game types"), Localizable("Server address")};
	if(!Input()->HasComposition() && Ui()->Enabled())
		for(int i = 0; i < 3; ++i)
			if(s_CompactInputs[i].IsActive() && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))
			{
				const int Next = (i + (Input()->ShiftIsPressed() ? 2 : 1)) % 3;
				s_CompactInputs[i].Deactivate();
				Ui()->SetActiveItem(&s_CompactInputs[Next]);
				break;
			}
	for(int i = 0; i < 3; ++i)
	{
		CUIRect Edit = CLucentBrowserFilterLayout::ChipBounds(Compact, i);
		auto &Field = s_CompactInputs[i];
		const float Floating = s_aFloatingLabels[i].Update(!Field.IsEmpty(), Clock.m_Now, Clock.m_Animate, LucentStyle::Motion::FilterInput);
		Field.SetEmptyText(nullptr);
		if(Ui()->DoEditBox(&Field, &Edit, 11))
			Client()->ServerBrowserUpdate();
		// The label lives outside the edit box's text clip. Reserve its space in the row.
		CUIRect Label = {Edit.x + 5, Edit.y + 4 - 14 * Floating, Edit.w - 10, 14 - 4 * Floating};
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Props.m_EnableWidthCheck = false;
		LucentUi::CTextScope Text(TextRender(), Label.w, Floating, 0, Ui()->Enabled());
		LucentUi::COpacityScope Opacity(TextRender(), .55f + .45f * Floating);
		Ui()->DoLabel(&Label, Localize(apNames[i]), 11 - 2.5f * Floating, TEXTALIGN_ML, Props);
		if(Field.IsActive() && !Input()->HasComposition() &&
			(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
		{
			Field.Deactivate();
			Ui()->SetActiveItem(&s_aFloatingLabels);
			Ui()->SetActiveItem(nullptr);
		}
	}
}

void CMenus::RenderLucentBrowserFilterSection(CUIRect &View, const char *pText)
{
	CUIRect Heading;
	View.HSplitTop(8, nullptr, &View);
	View.HSplitTop(16, &Heading, &View);
	TextRender()->TextColor(LucentStyle::Style::MutedText);
	Ui()->DoLabel(&Heading, pText, 11, TEXTALIGN_MC);
	View.HSplitTop(CLucentBrowserFilterLayout::SectionDividerGap, nullptr, &View);
	CUIRect Line;
	View.HSplitTop(LucentStyle::Style::Hairline, &Line, &View);
	LucentUi::Divider(Line);
	View.HSplitTop(CLucentBrowserFilterLayout::SectionBottomGap, nullptr, &View);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

float CMenus::LucentBrowserFilterBodyOpacity() const
{
	return LucentStyle::Motion::Smooth(std::clamp((m_LucentBrowser.FiltersOpacity() - LucentStyle::Motion::FilterBodyDelay) / (1 - LucentStyle::Motion::FilterBodyDelay), 0.f, 1.f));
}

void CMenus::LayoutLucentBrowserFilterGrid(CUIRect &View, bool CountriesTab)
{
	float Minimum = std::max(TextRender()->TextWidth(11, Localize("Countries and types")),
		TextRender()->TextWidth(11, Localize("Player country:")) + LucentStyle::Style::ControlSize + LucentStyle::Style::LabelGap + 68);
	Minimum = std::max(Minimum, 2 * std::max(TextRender()->TextWidth(11, Localize("Countries")), TextRender()->TextWidth(11, Localize("Types"))) + 12);
	if(!CountriesTab)
		for(const auto *pType : ServerBrowser()->CommunityCache().SelectableTypes())
			Minimum = std::max(Minimum, 3 * (TextRender()->TextWidth(13 * CUi::ms_FontmodHeight, pType->Name()) + 4));
	const float Width = CLucentBrowserFilterLayout::GridWidth(View.w, ServerBrowser()->CommunityCache().SelectableCountries().size(), CountriesTab, std::max(240.f, Minimum));
	View.VMargin((View.w - Width) / 2, &View);
}

void CMenus::PopupLucentBrowserFilterCountry(const CUIRect &Flag, SPopupMenuId *pId, SPopupCountrySelectionContext *pContext)
{
	const CUIRect Screen = *Ui()->Screen();
	const float Width = std::min(280.f, Screen.w - 12);
	const float Height = std::min(240.f, Screen.h - 12);
	SPopupMenuProperties Props;
	Props.m_ClampToScreen = true;
	Props.m_BorderWidth = .5f;
	Props.m_BorderColor = LucentUi::Accent(.22f);
	Props.m_BackgroundColor = ColorRGBA(.025f, .021f, .035f, .99f);
	Ui()->OnReset();
	Ui()->DoPopupMenu(pId, std::clamp(Flag.x, Screen.x + 6, Screen.x + Screen.w - Width - 6), std::clamp(Flag.y + Flag.h + 4, Screen.y + 6, Screen.y + Screen.h - Height - 6), Width, Height, pContext, PopupLucentCountrySelection, Props);
}

float CMenus::LucentBrowserCommunityFilterOpacity(const void *pItemId, bool Active)
{
	return (Active ? 1.f : .4f + .4f * LucentUi::Highlight(pItemId, Ui()->HotItem() == pItemId));
}

void CMenus::RenderLucentBrowserCountryFilterItem(int ItemIndex, CUIRect Item, const void *pItemId, bool Active)
{
	Item.Margin(2.0f, &Item);
	Item.HMargin(1, &Item);
	const float OldWidth = Item.w;
	Item.w = Item.h * 2.0f;
	Item.x += (OldWidth - Item.w) / 2.0f;
	GameClient()->m_CountryFlags.Render(ServerBrowser()->CommunityCache().SelectableCountries()[ItemIndex]->FlagId(), ColorRGBA(1.0f, 1.0f, 1.0f, LucentUi::FilterItemOpacity(pItemId, Active, Ui()->HotItem() == pItemId) * CUIRect::DrawOpacity()), Item.x, Item.y, Item.w, Item.h);
}

void CMenus::RenderLucentBrowserTypeFilterItem(int ItemIndex, CUIRect Item, const void *pItemId, bool Active)
{
	Item.Margin(2.0f, &Item);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, LucentUi::FilterItemOpacity(pItemId, Active, Ui()->HotItem() == pItemId));
	Ui()->DoLabel(&Item, ServerBrowser()->CommunityCache().SelectableTypes()[ItemIndex]->Name(), Item.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
