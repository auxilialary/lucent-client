#ifndef GAME_CLIENT_LUCENT_UI_SCROLLBAR_H
#define GAME_CLIENT_LUCENT_UI_SCROLLBAR_H

class CUIRect;

namespace LucentUi
{
	// Paint geometry only; the host retains the full-sized rail and drag hitbox.
	void DrawScrollbar(CUIRect Slider, bool Horizontal, bool Active, bool Hot);
}
#endif
