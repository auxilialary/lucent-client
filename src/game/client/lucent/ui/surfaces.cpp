#include "surfaces.h"
#include "controls.h"

#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/style/gradient.h>

namespace LucentUi
{
	void GradientFill(const CUIRect &Rect, float Alpha, int Corners, float Radius, bool Animate)
	{
		const double Time = Animate ? LucentStyle::Motion::Clock().GradientTime() : 0;
		const auto Left = LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin, Time).WithAlpha(Alpha);
		const auto Right = LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin + LucentStyle::Gradient::ControlSpan, Time).WithAlpha(Alpha);
		Rect.Draw4(Left, Right, Left, Right, Corners, Radius);
	}

	void Divider(CUIRect Rect, float Alpha, bool Glow, bool Gradient)
	{
		CUIRect Left, Right;
		Rect.VSplitMid(&Left, &Right);
		const double Time = LucentStyle::Motion::Clock().GradientTime();
		const auto Color = [&](float Position, float Opacity) {
			return Gradient ? LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin + LucentStyle::Gradient::ControlSpan * Position, Time).WithAlpha(Opacity) : Accent(Opacity);
		};
		Left.Draw4(Color(0, 0), Color(.5f, Alpha), Color(0, 0), Color(.5f, Alpha), 0, 0);
		Right.Draw4(Color(.5f, Alpha), Color(1, 0), Color(.5f, Alpha), Color(1, 0), 0, 0);
		if(Glow)
		{
			Rect.y += Rect.h;
			Rect.h = 4;
			Rect.VSplitMid(&Left, &Right);
			Left.Draw4(Accent(0), Accent(.025f), Accent(0), Accent(0), 0, 0);
			Right.Draw4(Accent(.025f), Accent(0), Accent(0), Accent(0), 0, 0);
		}
	}

	void InputFrame(const CUIRect &Rect, const void *pId, bool Filled, bool Focus, bool Hover, bool Enabled, int Corners)
	{
		const float Active = Enabled ? Highlight(pId, Filled, EChannel::ACTIVE) : 0.f;
		const float Focused = Enabled ? Highlight(pId, Focus, EChannel::FOCUS) : 0.f;
		const float Hot = Enabled ? Highlight(pId, Hover) : 0.f;
		const float Alpha = Enabled ? 1.f : LucentStyle::Style::Disabled;
		Rect.Draw(Accent((LucentStyle::Style::BorderIdle + LucentStyle::Style::InputHover * Hot + LucentStyle::Style::InputFocus * Focused) * Alpha), Corners, RADIUS);
		if(Active > .001f)
			GradientFill(Rect, Active * (LucentStyle::Style::FilledBorder + (LucentStyle::Style::FocusBorder - LucentStyle::Style::FilledBorder) * Focused) * Alpha, Corners);
		CUIRect Inner;
		Rect.Margin(LucentStyle::Style::Hairline, &Inner);
		Inner.Draw(LucentStyle::Style::Surface, Corners, RADIUS);
	}

	void TooltipSurface(CUIRect &Rect, const CUIRect &Anchor, const CUIRect &Screen, float Margin, float Radius, float Alpha)
	{
		// Header hints always sit below the complete header, away from search.
		Rect.x = std::clamp(Anchor.x + Anchor.w - Rect.w, Margin, Screen.w - Rect.w - Margin);
		Rect.y = Anchor.y + Anchor.h + Margin;
		if(Rect.y + Rect.h > Screen.h - Margin)
			Rect.y = Anchor.y - Rect.h - Margin;
		Rect.y = std::clamp(Rect.y, Margin, Screen.h - Rect.h - Margin);
		Rect.Draw(ColorRGBA(.078f, .067f, .098f, .98f * Alpha), IGraphics::CORNER_ALL, Radius);
	}

	void Surface(const CUIRect &Rect, bool Focus, bool Hover, int Corners, float Highlight)
	{
		const float GlowAlpha = Highlight >= 0 ? Highlight : float(Focus || Hover);
		if(GlowAlpha > .001f)
		{
			CUIRect Glow = Rect;
			Glow.Margin(-1, &Glow);
			Glow.Draw(Accent(GlowAlpha * LucentStyle::Style::SurfaceGlow), Corners, RADIUS + 1);
		}
		Rect.Draw(Accent(LucentStyle::Style::BorderIdle + (LucentStyle::Style::BorderActive - LucentStyle::Style::BorderIdle) * GlowAlpha),
			Corners, RADIUS);
		CUIRect Inner;
		Rect.Margin(LucentStyle::Style::Hairline, &Inner);
		Inner.Draw(LucentStyle::Style::Surface, Corners, RADIUS);
	}
}
