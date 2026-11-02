#ifndef GAME_CLIENT_LUCENT_HUD_PANEL_GEOMETRY_H
#define GAME_CLIENT_LUCENT_HUD_PANEL_GEOMETRY_H

#include "layout.h"

#include <base/vmath.h>

namespace LucentHud
{
	// Paint coordinates only. Logical layout/collision rectangles stay unchanged.
	struct CPixelGrid
	{
		vec2 m_Origin, m_Step;
		CPixelGrid(vec2 Origin, vec2 Step) : m_Origin(Origin), m_Step(Step)
		{
			for(int Axis = 0; Axis < 2; ++Axis)
			{
				if(!std::isfinite(m_Origin[Axis]))
					m_Origin[Axis] = 0;
				if(!std::isfinite(m_Step[Axis]) || m_Step[Axis] <= 0)
					m_Step[Axis] = 1;
			}
		}
		vec2 Snap(vec2 Point) const
		{
			for(int Axis = 0; Axis < 2; ++Axis)
				Point[Axis] = m_Origin[Axis] + float(std::round((double(Point[Axis]) - m_Origin[Axis]) / m_Step[Axis])) * m_Step[Axis];
			return Point;
		}
		CRect SnapBounds(const CRect &Rect) const
		{
			const vec2 Start = Snap({Rect.x, Rect.y}), End = Snap({Rect.x + Rect.w, Rect.y + Rect.h});
			return {Start.x, Start.y, std::max(0.f, End.x - Start.x), std::max(0.f, End.y - Start.y)};
		}
		float Pixel() const { return std::max(m_Step.x, m_Step.y); }
	};

	struct CPanelGeometry
	{
		static constexpr int MaxSteps = 32, MaxPoints = 4 * (MaxSteps + 1);
		using CBoundary = std::array<vec2, MaxPoints>;
		CRect m_Bounds{};
		CBoundary m_Points{}, m_Normals{};
		int m_Count = 0, m_Steps = 0;
		float m_Radius = 0, m_Feather = 0, m_BorderWidth = 0, m_BorderPeak = 0;

		static CPanelGeometry Calculate(const CRect &Rect, float Scale, const CPixelGrid &Grid)
		{
			CPanelGeometry G;
			if(!std::isfinite(Rect.x) || !std::isfinite(Rect.y) || !std::isfinite(Rect.w) || !std::isfinite(Rect.h) ||
				!std::isfinite(Scale) || Scale <= 0 || Rect.w <= 0 || Rect.h <= 0)
				return G;
			G.m_Bounds = Grid.SnapBounds(Rect);
			if(!std::isfinite(G.m_Bounds.w) || !std::isfinite(G.m_Bounds.h) || G.m_Bounds.w <= 0 || G.m_Bounds.h <= 0)
				return G;
			const float Pixel = Grid.Pixel(), Side = std::min(G.m_Bounds.w, G.m_Bounds.h);
			G.m_Radius = std::min(Side / 2, std::max(Pixel / 2, std::round(PanelRadius(G.m_Bounds, Scale) / Pixel) * Pixel));
			G.m_Feather = std::min(Pixel / 2, G.m_Radius / 4);
			const float NominalWidth = LucentStyle::Style::Hairline * MenuUnit * Scale;
			G.m_BorderWidth = std::min(G.m_Radius / 2, std::max(Pixel / 2, std::round(2 * NominalWidth / Pixel) * Pixel / 2));
			G.m_BorderPeak = std::min(1.f, G.m_BorderWidth / (2 * G.m_Feather));
			// <= 1/8 physical pixel chord error over supported HUD resolutions.
			G.m_Steps = std::clamp(int(std::ceil(pi / 2 * std::sqrt((G.m_Radius + G.m_Feather) / Pixel))), 4, MaxSteps);
			G.m_Count = 4 * (G.m_Steps + 1);
			for(int Corner = 0; Corner < 4; ++Corner)
			{
				const bool Right = Corner == 1 || Corner == 2, Bottom = Corner >= 2;
				const vec2 Center(G.m_Bounds.x + (Right ? G.m_Bounds.w - G.m_Radius : G.m_Radius),
					G.m_Bounds.y + (Bottom ? G.m_Bounds.h - G.m_Radius : G.m_Radius));
				for(int Step = 0; Step <= G.m_Steps; ++Step)
				{
					const float Angle = pi + Corner * pi / 2 + Step * pi / (2 * G.m_Steps);
					const int Index = Corner * (G.m_Steps + 1) + Step;
					G.m_Normals[Index] = {std::cos(Angle), std::sin(Angle)};
					G.m_Points[Index] = Center + G.m_Normals[Index] * G.m_Radius;
				}
			}
			return G;
		}

		CBoundary Boundary(float Offset) const
		{
			CBoundary Result{};
			for(int I = 0; I < m_Count; ++I)
				Result[I] = m_Points[I] + m_Normals[I] * Offset;
			return Result;
		}
		std::array<float, 4> BorderOffsets() const
		{
			// Same inner/outer coverage policy as Lucent checkbox rings, applied
			// to a rounded rectangle instead of an unfeathered hard annulus.
			const float Plateau = std::max(0.f, m_BorderWidth - 2 * m_Feather) / 2;
			return {-m_BorderWidth - m_Feather, -m_BorderWidth / 2 - Plateau, -m_BorderWidth / 2 + Plateau, m_Feather};
		}
	};
}
#endif
