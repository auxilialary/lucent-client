#ifndef GAME_CLIENT_LUCENT_UI_SURFACES_H
#define GAME_CLIENT_LUCENT_UI_SURFACES_H

#include <game/client/lucent/style/tokens.h>
#include <game/client/ui.h>

namespace LucentUi
{
	constexpr float RADIUS = LucentStyle::Style::Radius;
	inline ColorRGBA Accent(float Alpha) { return LucentStyle::Style::Accent.WithAlpha(Alpha); }

	void GradientFill(const CUIRect &Rect, float Alpha, int Corners = IGraphics::CORNER_ALL, float Radius = RADIUS, bool Animate = true);
	void Divider(CUIRect Rect, float Alpha = .25f, bool Glow = false, bool Gradient = false);
	void InputFrame(const CUIRect &Rect, const void *pId, bool Filled, bool Focus, bool Hover, bool Enabled, int Corners);
	void TooltipSurface(CUIRect &Rect, const CUIRect &Anchor, const CUIRect &Screen, float Margin, float Radius, float Alpha);
	void Surface(const CUIRect &Rect, bool Focus = false, bool Hover = false, int Corners = IGraphics::CORNER_ALL, float Highlight = -1);
}
#endif
