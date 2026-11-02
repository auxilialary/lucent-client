#ifndef GAME_CLIENT_LUCENT_SETTINGS_PRESENTATION_H
#define GAME_CLIENT_LUCENT_SETTINGS_PRESENTATION_H

#include <base/vmath.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>

namespace LucentSettings
{
	// One geometry path for the actual preview and a clearly labelled small-size loupe.
	// Inputs are read-only artwork measurements; there is no runtime config access.
	struct CPreviewLayout
	{
		static constexpr float Height = 112;
		static int WheelValue(int Current, int Direction)
		{
			return std::clamp(std::clamp(Current, 5, 500) + std::clamp(Direction, -1, 1) * 5, 5, 500);
		}
		static float PreviewScale(vec2 Size, vec2 Offset, CUIRect Area)
		{
			const float Width = Size.x + 2 * std::abs(Offset.x);
			const float Height = Size.y + 2 * std::abs(Offset.y);
			return std::clamp(std::min(std::max(0.f, Area.w - 24) / std::max(1.f, Width), std::max(0.f, Area.h - 24) / std::max(1.f, Height)), 0.f, 1.f);
		}
		CUIRect m_Actual, m_Hint, m_Sprite, m_Loupe, m_LoupeSprite, m_LoupeLabel;
		float m_Fit = 1, m_Magnification = 1;
		static CPreviewLayout Calculate(CUIRect Area, vec2 Size, vec2 Offset, bool Cursor)
		{
			CPreviewLayout L;
			Area.HSplitBottom(std::min(18.f, Area.h), &L.m_Actual, &L.m_Hint);
			if(Cursor && std::max(Size.x, Size.y) < 16 && Area.w >= 220)
			{
				L.m_Actual.VSplitRight(100, &L.m_Actual, &L.m_Loupe);
				L.m_Loupe.HSplitBottom(24, &L.m_Loupe, &L.m_LoupeLabel);
				L.m_Magnification = std::min(8.f, 24.f / std::max(1.f, std::max(Size.x, Size.y)));
				const vec2 Enlarged = Size * L.m_Magnification;
				L.m_LoupeSprite = {L.m_Loupe.x + (L.m_Loupe.w - Enlarged.x) / 2, L.m_Loupe.y + (L.m_Loupe.h - Enlarged.y) / 2, Enlarged.x, Enlarged.y};
			}
			L.m_Fit = PreviewScale(Size, Offset, L.m_Actual);
			const vec2 Scaled = Size * L.m_Fit;
			L.m_Sprite = {L.m_Actual.x + (L.m_Actual.w - Scaled.x) / 2 + Offset.x * L.m_Fit, L.m_Actual.y + (L.m_Actual.h - Scaled.y) / 2 + Offset.y * L.m_Fit, Scaled.x, Scaled.y};
			return L;
		}
	};

}
#endif
