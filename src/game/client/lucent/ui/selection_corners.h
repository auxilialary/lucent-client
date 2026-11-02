#ifndef GAME_CLIENT_LUCENT_UI_SELECTION_CORNERS_H
#define GAME_CLIENT_LUCENT_UI_SELECTION_CORNERS_H
#include <game/client/lucent/motion/transition.h>
#include <game/client/ui_rect.h>

#include <array>
class CUi;
class IGraphics;
namespace LucentUi
{
	struct CSelectionCornersGeometry
	{
		struct CPath
		{
			std::array<vec2, 35> m_Points{}, m_Normals{};
			int m_Count = 0;
		};
		std::array<CPath, 4> m_Corners{};
		float m_Thickness = 0, m_Feather = 0;
		static CSelectionCornersGeometry Calculate(CUIRect Rect, float Pixel, float Progress)
		{
			CSelectionCornersGeometry G;
			const float Side = std::min(Rect.w, Rect.h);
			if(Side <= 0)
				return G;
			Pixel = std::max(.001f, Pixel);
			G.m_Thickness = std::min(std::max(Pixel, LucentStyle::Style::Hairline), Side * .08f);
			G.m_Feather = std::min(Pixel, Side * .1f);
			const float Inset = (G.m_Thickness + G.m_Feather) / 2 + std::min(LucentStyle::Style::Hairline, Side * .05f);
			const float Reveal = LucentStyle::Motion::Lerp(.75f, 1.f, std::clamp(Progress, 0.f, 1.f));
			const float X = std::min(Rect.w * .2f, Side * .35f) * Reveal;
			const float Y = std::min(Rect.h * .25f, Side * .35f) * Reveal;
			const float Radius = std::min(LucentStyle::Style::Radius, std::min(X, Y) * .4f);
			// Keep both AA edges outside the bend's center, even on tiny controls.
			G.m_Thickness = std::min(G.m_Thickness, Radius * .8f);
			G.m_Feather = std::min(G.m_Feather, Radius * .8f);
			const int Steps = std::clamp(int(std::ceil(pi * Radius / Pixel)), 4, 32);
			for(int Corner = 0; Corner < 4; ++Corner)
			{
				const vec2 Sign(Corner == 1 || Corner == 2 ? -1.f : 1.f, Corner >= 2 ? -1.f : 1.f);
				const vec2 Origin(Sign.x > 0 ? Rect.x + Inset : Rect.x + Rect.w - Inset, Sign.y > 0 ? Rect.y + Inset : Rect.y + Rect.h - Inset);
				auto &Path = G.m_Corners[Corner];
				const auto Add = [&](vec2 Point, vec2 Normal) {
					Path.m_Points[Path.m_Count] = Origin + vec2(Point.x * Sign.x, Point.y * Sign.y);
					Path.m_Normals[Path.m_Count++] = vec2(Normal.x * Sign.x, Normal.y * Sign.y);
				};
				Add({X, 0}, {0, -1});
				for(int I = 0; I <= Steps; ++I)
				{
					const float Angle = -pi / 2 - pi / 2 * I / Steps;
					const vec2 Normal(std::cos(Angle), std::sin(Angle));
					Add(vec2(Radius, Radius) + Normal * Radius, Normal);
				}
				Add({0, Y}, {-1, 0});
			}
			return G;
		}
	};
	// Background paint has its own inset/coverage edge, never a border along the corners.
	struct CSelectionBackgroundGeometry
	{
		std::array<vec2, 36> m_Inner{}, m_Outer{};
		static CSelectionBackgroundGeometry Calculate(CUIRect Rect, float Pixel)
		{
			CSelectionBackgroundGeometry G;
			const float Side = std::max(0.f, std::min(Rect.w, Rect.h));
			const float Inset = std::min(std::max(LucentStyle::Style::Radius, 2 * std::max(.001f, Pixel) + LucentStyle::Style::Hairline), Side * .2f);
			const float Radius = std::min(LucentStyle::Style::Radius, std::max(0.f, Side / 2 - Inset));
			const float Feather = std::min(std::max(.001f, Pixel), Radius / 2);
			const float InnerRadius = Radius - Feather;
			for(int Corner = 0; Corner < 4; ++Corner)
			{
				const bool Right = Corner == 1 || Corner == 2, Bottom = Corner >= 2;
				const vec2 Center(Right ? Rect.x + Rect.w - Inset - Radius : Rect.x + Inset + Radius, Bottom ? Rect.y + Rect.h - Inset - Radius : Rect.y + Inset + Radius);
				for(int I = 0; I <= 8; ++I)
				{
					const float Angle = pi + Corner * pi / 2 + I * pi / 16;
					const vec2 Normal(std::cos(Angle), std::sin(Angle));
					G.m_Outer[Corner * 9 + I] = Center + Normal * Radius;
					G.m_Inner[Corner * 9 + I] = Center + Normal * InnerRadius;
				}
			}
			return G;
		}
	};
	void DrawSelectionBackground(IGraphics *pGraphics, CUIRect Rect, float Pixel, ColorRGBA Color);
	// Paint only: inherit the caller's clipping and opacity, never alter input/layout.
	void DrawSelectionCorners(IGraphics *pGraphics, CUIRect Rect, float Pixel, float Progress);
	// Each control owns an ordinary Lucent transition; config remains the source of selection.
	void SelectionCorners(CUi *pUi, CUIRect Rect, LucentStyle::Motion::CTransition &Motion, bool Selected, bool Animate = true);
}
#endif
