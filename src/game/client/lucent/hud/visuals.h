#ifndef GAME_CLIENT_LUCENT_HUD_VISUALS_H
#define GAME_CLIENT_LUCENT_HUD_VISUALS_H
#include "panel_geometry.h"

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
	inline CPixelGrid PixelGrid(IGraphics *pGraphics)
	{
		const auto Screen = pGraphics->GetScreen();
		const auto Pixels = pGraphics->ScreenSize();
		return {Screen.m_TopLeft, {Screen.Width() / std::max(1.f, Pixels.x), Screen.Height() / std::max(1.f, Pixels.y)}};
	}
	inline void DrawSoftFill(IGraphics *pGraphics, const CPanelGeometry &G, ColorRGBA Top, ColorRGBA Bottom)
	{
		if(G.m_Count == 0)
			return;
		const auto Inner = G.Boundary(-G.m_Feather), Outer = G.Boundary(G.m_Feather);
		const vec2 Center(G.m_Bounds.x + G.m_Bounds.w / 2, G.m_Bounds.y + G.m_Bounds.h / 2);
		const auto ColorAt = [&](vec2 P) {
			return LucentStyle::Gradient::Blend(Top, Bottom, std::clamp((P.y - G.m_Bounds.y) / G.m_Bounds.h, 0.f, 1.f));
		};
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(int I = 0; I < G.m_Count; ++I)
		{
			const int J = (I + 1) % G.m_Count;
			// Same fan + transparent coverage fringe as LucentUi::CheckboxMesh.
			pGraphics->SetColor4(ColorAt(Center), ColorAt(Inner[I]), ColorAt(Inner[J]), ColorAt(Inner[J]));
			const IGraphics::CFreeformItem Core(Center, Inner[I], Inner[J], Inner[J]);
			pGraphics->QuadsDrawFreeform(&Core, 1);
			// SetColor4 uses TL/TR/BL/BR; the freeform API orders its vertices differently.
			pGraphics->SetColor4(ColorAt(Inner[I]), ColorAt(Outer[I]).WithAlpha(0), ColorAt(Outer[J]).WithAlpha(0), ColorAt(Inner[J]));
			const IGraphics::CFreeformItem Fringe(Inner[I], Outer[I], Inner[J], Outer[J]);
			pGraphics->QuadsDrawFreeform(&Fringe, 1);
		}
		pGraphics->QuadsEnd();
	}
	inline void DrawSoftBorder(IGraphics *pGraphics, const CPanelGeometry &G, float Alpha, bool Selected)
	{
		if(G.m_Count == 0 || Alpha <= 0)
			return;
		// Keep the rose identity, but avoid the old high-contrast 0.40 selected stroke.
		const auto Color = LucentStyle::Gradient::Blend(LucentStyle::Style::DetailText, LucentStyle::Style::Accent, LucentStyle::Style::Selected)
					   .WithAlpha((LucentStyle::Style::SurfaceGlow + (Selected ? LucentStyle::Style::BorderIdle : 0.f)) * Alpha * G.m_BorderPeak);
		const auto Offsets = G.BorderOffsets();
		const std::array Contours = {G.Boundary(Offsets[0]), G.Boundary(Offsets[1]), G.Boundary(Offsets[2]), G.Boundary(Offsets[3])};
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(int Band = 0; Band < 3; ++Band)
		{
			if(Offsets[Band] == Offsets[Band + 1])
				continue;
			const auto InnerColor = Color.WithAlpha(Band == 0 ? 0.f : Color.a);
			const auto OuterColor = Color.WithAlpha(Band == 2 ? 0.f : Color.a);
			for(int I = 0; I < G.m_Count; ++I)
			{
				const int J = (I + 1) % G.m_Count;
				pGraphics->SetColor4(InnerColor, OuterColor, OuterColor, InnerColor);
				const IGraphics::CFreeformItem Segment(Contours[Band][I], Contours[Band + 1][I], Contours[Band][J], Contours[Band + 1][J]);
				pGraphics->QuadsDrawFreeform(&Segment, 1);
			}
		}
		pGraphics->QuadsEnd();
	}
	inline void SoftFill(IGraphics *pGraphics, const CRect &Rect, ColorRGBA Color, float Scale)
	{
		const auto G = CPanelGeometry::Calculate(Rect, Scale, PixelGrid(pGraphics));
		DrawSoftFill(pGraphics, G, Color, Color);
	}
	inline void Panel(IGraphics *pGraphics, const CRect &Rect, float Alpha, float Scale)
	{
		if(Alpha <= 0)
			return;
		const auto G = CPanelGeometry::Calculate(Rect, Scale, PixelGrid(pGraphics));
		DrawSoftFill(pGraphics, G, PanelSurface(Alpha, true), PanelSurface(Alpha, false));
		DrawSoftBorder(pGraphics, G, Alpha, false);
	}
	inline void SelectedSlot(IGraphics *pGraphics, const CRect &Rect, float Alpha, float Scale, float Selection = 1.f)
	{
		Alpha *= Selection;
		if(Alpha <= 0)
			return;
		const auto G = CPanelGeometry::Calculate(Rect, Scale, PixelGrid(pGraphics));
		const auto Fill = Accent(LucentStyle::Style::Selected * Alpha);
		DrawSoftFill(pGraphics, G, Fill, Fill);
		DrawSoftBorder(pGraphics, G, Alpha, true);
	}
}
#endif
