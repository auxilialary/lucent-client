#include <game/client/components/menus.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/ui_listbox.h>

#include <algorithm>

void CMenus::ConfigureLucentBrowserPlayers(CListBox &ListBox, float &FontSize, float &RowHeight)
{
	FontSize = 11.f;
	RowHeight = 28.f;
	ListBox.SetScrollbarWidth(10.0f);
	ListBox.SetScrollbarMargin(1.0f);
	ListBox.SetColors(ColorRGBA(1, .82f, .91f, .08f), ColorRGBA(1, .82f, .91f, .045f));
}

void CMenus::LayoutLucentBrowserPlayer(const void *pItemId, const CUIRect &Row, float Width, CUIRect &Skin, CUIRect &Name, CUIRect &Clan, CUIRect &Score, CUIRect &Flag)
{
	Name = Row;
	Name.Draw(ColorRGBA(1, .82f, .91f, 0), 0, 0);
	Name.VSplitLeft(1.0f, nullptr, &Name);
	Name.VSplitLeft(29.0f, &Skin, &Name);
	Name.VSplitRight(std::min(58.f, Width * .22f), &Name, &Score);
	Name.VSplitRight(26.0f, &Name, &Flag);
	Flag.HMargin((Flag.h - 12.f) / 2, &Flag);
	Flag.VMargin(1.f, &Flag);
	Name.HSplitTop(15.f, &Name, &Clan);
	const float Hover = LucentUi::Highlight(pItemId, Ui()->Enabled() && Ui()->MouseHovered(&Row));
	Row.Draw(LucentUi::Accent(.045f * Hover), 0, 0);
}
