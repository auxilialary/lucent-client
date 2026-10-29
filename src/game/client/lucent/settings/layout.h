#ifndef GAME_CLIENT_LUCENT_SETTINGS_LAYOUT_H
#define GAME_CLIENT_LUCENT_SETTINGS_LAYOUT_H

#include <game/client/ui_rect.h>

#include <algorithm>

namespace LucentSettings
{
	constexpr int CategoryCount = 11;
	constexpr float CollapsedWidth = 52, ExpandedWidth = 200, RowHeight = 32, Padding = 6;
	constexpr float EdgeInset = 10, FooterGap = 8;

	// UI coordinates already account for UI scale. Content never follows the reveal.
	struct CLayout
	{
		CUIRect m_Content, m_Sidebar, m_Viewport, m_Back;

		static CLayout Calculate(CUIRect View, float Reveal)
		{
			CLayout Result;
			const float Width = std::max(0.f, View.w);
			const float Height = std::max(0.f, View.h);
			const float InsetX = std::min(EdgeInset, Width / 4);
			const float InsetY = std::min(EdgeInset, Height / 4);
			const float Rail = std::min(CollapsedWidth, Width - 2 * InsetX);
			const float Reserved = std::min(Width, InsetX + Rail);
			Result.m_Content = {View.x + Reserved, View.y, Width - Reserved, Height};
			const float Expanded = std::min(ExpandedWidth, Width - 2 * InsetX);
			const float PanelHeight = std::min(CategoryCount * RowHeight + RowHeight + FooterGap + 2 * Padding, Height - 2 * InsetY);
			Result.m_Sidebar = {View.x + InsetX, View.y + (Height - PanelHeight) / 2,
				Rail + (Expanded - Rail) * std::clamp(Reveal, 0.f, 1.f), PanelHeight};
			const float Pad = std::min(Padding, PanelHeight / 8);
			const float SidePad = std::min(Padding, Result.m_Sidebar.w / 2);
			const float FooterHeight = std::min(RowHeight, PanelHeight / 3);
			const float Gap = std::min(FooterGap, PanelHeight / 8);
			Result.m_Back = {Result.m_Sidebar.x + SidePad, Result.m_Sidebar.y + PanelHeight - Pad - FooterHeight,
				std::max(0.f, Result.m_Sidebar.w - 2 * SidePad), FooterHeight};
			Result.m_Viewport = Result.m_Sidebar;
			Result.m_Viewport.y += Pad;
			Result.m_Viewport.h = std::max(0.f, PanelHeight - FooterHeight - Gap - 2 * Pad);
			return Result;
		}
		float MaxScroll() const { return std::max(0.f, CategoryCount * RowHeight - m_Viewport.h); }
		CUIRect Row(int Index, float Scroll) const
		{
			const float SidePad = std::min(Padding, m_Viewport.w / 2);
			return {m_Viewport.x + SidePad, m_Viewport.y + Index * RowHeight - Scroll,
				std::max(0.f, m_Viewport.w - 2 * SidePad), RowHeight};
		}
		CUIRect VisualRow(int Index, float Scroll, float Hover) const
		{
			auto Rect = Row(Index, Scroll);
			const float Inset = 3.f - 2.f * std::clamp(Hover, 0.f, 1.f);
			Rect.y += Inset;
			Rect.h -= 2 * Inset;
			return Rect;
		}
		CUIRect Hitbox(int Index, float Scroll) const
		{
			auto Rect = Row(Index, Scroll);
			const float Bottom = std::min(Rect.y + Rect.h, m_Viewport.y + m_Viewport.h);
			Rect.y = std::max(Rect.y, m_Viewport.y);
			Rect.h = std::max(0.f, Bottom - Rect.y);
			return Rect;
		}
	};
}
#endif
