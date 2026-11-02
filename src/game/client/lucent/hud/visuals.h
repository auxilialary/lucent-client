#ifndef GAME_CLIENT_LUCENT_HUD_VISUALS_H
#define GAME_CLIENT_LUCENT_HUD_VISUALS_H
#include "layout.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/graphics.h>

#include <game/client/lucent/style/gradient.h>

#include <cmath>

namespace LucentHud
{
	// Adapt the shared menu palette, never maintain a second HUD palette.
	inline ColorRGBA Text(float Alpha) { return LucentStyle::Style::Text.WithAlpha(Alpha); }
	inline ColorRGBA Secondary(float Alpha)
	{
		// Small secondary labels need more contrast than menu metadata over a map.
		return LucentStyle::Gradient::Blend(LucentStyle::Style::DetailText, LucentStyle::Style::Text, LucentStyle::Style::BorderActive).WithAlpha(Alpha);
	}
	inline ColorRGBA Accent(float Alpha) { return LucentStyle::Style::Accent.WithAlpha(Alpha); }
	inline ColorRGBA Negative(float Alpha) { return LucentStyle::Style::ErrorText.WithAlpha(Alpha); }
	inline ColorRGBA Positive(float Alpha) { return LucentStyle::Console::SeverityColors[2].WithAlpha(Alpha); }
	inline ColorRGBA FrozenAccent(float Alpha) { return LucentStyle::Console::SeverityColors[3].WithAlpha(Alpha); }
	inline ColorRGBA PanelSurface(float Alpha, bool Top)
	{
		// Static plum depth from existing surface/selection tokens. The brighter
		// upper surface is restrained; no moving sheen or extra shadow pass.
		return LucentStyle::Gradient::Blend(LucentStyle::Style::Surface, LucentStyle::Style::ServerSelection,
			Top ? LucentStyle::Style::SceneBase : LucentStyle::Style::Selected)
			.WithAlpha(LucentStyle::Console::PopoverSurface.a * Alpha);
	}
	inline void Contour(IGraphics *pGraphics, const CRect &Rect, ColorRGBA Color, float Scale)
	{
		const float CornerRadius = PanelRadius(Rect, Scale);
		const float Edge = std::min(CornerRadius, LucentStyle::Style::Hairline * MenuUnit * Scale);
		if(Edge <= 0 || Color.a <= 0)
			return;
		// Finer arcs keep larger HUD corners smooth. Fixed stack storage and one
		// perimeter pass, shared by idle panels and the existing active selection.
		constexpr int Steps = 8, Points = 4 * (Steps + 1);
		vec2 aOuter[Points], aInner[Points];
		for(int Corner = 0; Corner < 4; ++Corner)
		{
			const vec2 Center(Rect.x + ((Corner == 1 || Corner == 2) ? Rect.w - CornerRadius : CornerRadius),
				Rect.y + (Corner >= 2 ? Rect.h - CornerRadius : CornerRadius));
			for(int Step = 0; Step <= Steps; ++Step)
			{
				const float Angle = pi + Corner * pi / 2 + Step * pi / (2 * Steps);
				const vec2 Direction(std::cos(Angle), std::sin(Angle));
				const int Index = Corner * (Steps + 1) + Step;
				aOuter[Index] = Center + Direction * CornerRadius;
				aInner[Index] = Center + Direction * (CornerRadius - Edge);
			}
		}
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(Color);
		for(int i = 0; i < Points; ++i)
		{
			const int Next = (i + 1) % Points;
			const IGraphics::CFreeformItem Segment(aOuter[i], aInner[i], aOuter[Next], aInner[Next]);
			pGraphics->QuadsDrawFreeform(&Segment, 1);
		}
		pGraphics->QuadsEnd();
	}
	inline void Panel(IGraphics *pGraphics, const CRect &Rect, float Alpha, float Scale)
	{
		const auto Top = PanelSurface(Alpha, true), Bottom = PanelSurface(Alpha, false);
		pGraphics->DrawRect4(Rect.x, Rect.y, Rect.w, Rect.h, Top, Top, Bottom, Bottom, IGraphics::CORNER_ALL, PanelRadius(Rect, Scale));
		// A true perimeter avoids putting a rose fill underneath all panel text.
		Contour(pGraphics, Rect, Accent(LucentStyle::Style::BorderIdle * Alpha), Scale);
	}
	inline void SelectedSlot(IGraphics *pGraphics, const CRect &Rect, float Alpha, float Scale, float Selection = 1.f)
	{
		Alpha *= Selection;
		pGraphics->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, Accent(LucentStyle::Style::Selected * Alpha), IGraphics::CORNER_ALL, PanelRadius(Rect, Scale));
		Contour(pGraphics, Rect, Accent(LucentStyle::Style::BorderActive * Alpha), Scale);
	}
}
#endif
