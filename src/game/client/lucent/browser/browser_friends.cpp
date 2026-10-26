#include <engine/font_icons.h>
#include <engine/friends.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

float CMenus::LayoutLucentBrowserFriends(CUIRect &View, CUIRect &List, CUIRect &ServerFriends, bool &ShowAdd, CLineInput &NameInput, CLineInput &ClanInput, const void *pAddAction)
{
	float AddReveal = 1;

	CUIRect Heading, Action;
	View.HSplitTop(28, &Heading, &View);
	Heading.VSplitRight(100, &Heading, &Action);
	char aHeading[96];
	str_format(aHeading, sizeof(aHeading), "%s | %d", Localize("Friends"), GameClient()->Friends()->NumFriends());
	Ui()->DoLabel(&Heading, aHeading, 14, TEXTALIGN_ML);
	if(LucentUi::TextButton(Ui(), TextRender(), pAddAction, Localize("Add friend"), Action, ShowAdd, 10))
	{
		ShowAdd = !ShowAdd;
		if(!ShowAdd)
		{
			NameInput.Deactivate();
			ClanInput.Deactivate();
		}
	}
	AddReveal = LucentUi::Highlight(&ShowAdd, ShowAdd);
	View.HSplitTop(86.f * AddReveal, &ServerFriends, &List);
	return AddReveal;
}

void CMenus::ConfigureLucentBrowserFriends(CScrollRegionParams &Params)
{
	Params.m_ScrollbarThickness = 8.0f;
	Params.m_ScrollbarMargin = 1.0f;
	Params.m_ForceShowScrollbar = false;
}

void CMenus::RenderLucentBrowserFriendGroupHeader(CUIRect Header, const void *pId, bool Extended, CUIRect &GroupLabel)
{
	Header.Draw(ColorRGBA(1, .82f, .91f, Ui()->HotItem() == pId ? .06f : 0.f), 0, 0);
	CUIRect GroupIcon;
	Header.VSplitLeft(Header.h, &GroupIcon, &GroupLabel);
	GroupIcon.Margin(2.0f, &GroupIcon);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->TextColor(Ui()->HotItem() == pId ? TextRender()->DefaultTextColor() : ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
	Ui()->DoLabel(&GroupIcon, Extended ? FontIcon::CHEVRON_DOWN : FontIcon::CHEVRON_RIGHT, 8.f, TEXTALIGN_MC);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::RenderLucentBrowserFriendGroupLabel(CUIRect GroupLabel, size_t FriendType, char (&aBuf)[256])
{
	const float FontSize = 10.0f;

	CUIRect Counter;
	GroupLabel.VSplitRight(26, &GroupLabel, &Counter);
	const char *apGroups[] = {"Online friends", "Online clanmates", "Offline"};
	SLabelProperties Props;
	Props.m_MaxWidth = GroupLabel.w;
	Props.m_StopAtEnd = true;
	Ui()->DoLabel(&GroupLabel, Localize(apGroups[FriendType]), FontSize, TEXTALIGN_ML, Props);
	str_format(aBuf, sizeof(aBuf), "%d", int(m_avFriends[FriendType].size()));
	TextRender()->TextColor(LucentStyle::Style::MutedText);
	Ui()->DoLabel(&Counter, aBuf, 9, TEXTALIGN_MR);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CMenus::RenderLucentBrowserFriendBackground(const CUIRect &Rect, bool InSelectedServer, bool Inside)
{
	Rect.Draw(ColorRGBA(1, .82f, .91f, InSelectedServer ? .09f : Inside ? .05f : 0.f), 0, 5.0f);
}

void CMenus::LayoutLucentBrowserFriendForm(CUIRect &ServerFriends, CUIRect &Button, CLineInput &NameInput, CLineInput &ClanInput, char (&aBuf)[256])
{
	const float FontSize = 10.0f;
	ServerFriends.Margin(5.0f, &ServerFriends);

	ServerFriends.HSplitTop(22.0f, &Button, &ServerFriends);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	Ui()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
	Button.VSplitLeft(58.0f, nullptr, &Button);
	Ui()->DoEditBox(&NameInput, &Button, FontSize + 2.0f);

	ServerFriends.HSplitTop(3.0f, nullptr, &ServerFriends);
	ServerFriends.HSplitTop(22.0f, &Button, &ServerFriends);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	Ui()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
	Button.VSplitLeft(58.0f, nullptr, &Button);
	Ui()->DoEditBox(&ClanInput, &Button, FontSize + 2.0f);

	ServerFriends.HSplitTop(3.0f, nullptr, &ServerFriends);
	ServerFriends.HSplitTop(22.0f, &Button, &ServerFriends);
}
