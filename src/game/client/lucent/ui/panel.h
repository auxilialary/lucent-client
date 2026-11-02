#ifndef GAME_CLIENT_LUCENT_UI_PANEL_H
#define GAME_CLIENT_LUCENT_UI_PANEL_H
#include <game/client/ui_scrollregion.h>
namespace LucentUi
{
	// Shared spacing and registration for future Lucent panels.
	inline CUIRect PanelRow(CUIRect &View, CScrollRegion &Scroll, float Height = 32.f)
	{
		CUIRect Row;
		View.HSplitTop(Height, &Row, &View);
		Scroll.AddRect(Row);
		Row.HMargin(4.f, &Row);
		return Row;
	}
	inline void IngamePanel(CUIRect &View)
	{
		View.Draw(ColorRGBA(.06f, .07f, .10f, .80f), IGraphics::CORNER_ALL, 8.f);
		View.Margin(12.f, &View);
	}
}
#endif
