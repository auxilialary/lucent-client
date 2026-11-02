#ifndef GAME_CLIENT_LUCENT_HUD_MODIFIER_TOOLTIP_H
#define GAME_CLIENT_LUCENT_HUD_MODIFIER_TOOLTIP_H
#include "layout.h"

#include <game/client/ui_rect.h>
namespace LucentHud
{
	// Convert the rendered icon, never the padded group or the hotbar, to UI space.
	inline CUIRect ModifierHitRect(CRect Icon, vec2 HudSize, CUIRect UiScreen)
	{
		if(HudSize.x <= 0 || HudSize.y <= 0)
			return {};
		const vec2 Scale(UiScreen.w / HudSize.x, UiScreen.h / HudSize.y);
		return {UiScreen.x + Icon.x * Scale.x, UiScreen.y + Icon.y * Scale.y, Icon.w * Scale.x, Icon.h * Scale.y};
	}
}
#endif
