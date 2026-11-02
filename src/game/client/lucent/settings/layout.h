#ifndef GAME_CLIENT_LUCENT_SETTINGS_LAYOUT_H
#define GAME_CLIENT_LUCENT_SETTINGS_LAYOUT_H

#include <game/client/lucent/motion/transition.h>
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
		CUIRect m_Content, m_Sidebar, m_Viewport, m_Back, m_WorkspaceRow, m_aWorkspaces[2], m_WorkspaceDivider;
		bool m_WorkspacesExpanded = false;
		float m_WorkspaceReveal = 0;
		int m_ItemCount = CategoryCount;

		static CLayout Calculate(CUIRect View, float Reveal, int ItemCount = CategoryCount)
		{
			CLayout Result;
			Result.m_ItemCount = std::clamp(ItemCount, 1, CategoryCount);
			const float Width = std::max(0.f, View.w);
			const float Height = std::max(0.f, View.h);
			const float InsetX = std::min(EdgeInset, Width / 4);
			const float InsetY = std::min(EdgeInset, Height / 4);
			const float Rail = std::min(CollapsedWidth, Width - 2 * InsetX);
			const float Reserved = std::min(Width, InsetX + Rail);
			Result.m_Content = {View.x + Reserved, View.y, Width - Reserved, Height};
			const float Expanded = std::min(ExpandedWidth, Width - 2 * InsetX);
			// Include the pinned workspace row/divider in the required height. Only
			// screens shorter than this complete stack need category scrolling.
			const float PanelHeight = std::min(CategoryCount * RowHeight + 2 * RowHeight + FooterGap + 3 * Padding, Height - 2 * InsetY);
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
			// Fixed horizontal workspace row; rail width and footer stay unchanged.
			CUIRect WorkspaceRow;
			const float HeaderHeight = std::min(RowHeight, Result.m_Viewport.h / 3);
			Result.m_Viewport.HSplitTop(HeaderHeight, &WorkspaceRow, &Result.m_Viewport);
			WorkspaceRow.VMargin(SidePad, &WorkspaceRow);
			Result.m_WorkspaceRow = WorkspaceRow;
			Result.m_WorkspacesExpanded = Reveal >= .99f;
			Result.m_WorkspaceReveal = LucentStyle::Motion::Smooth(Reveal);
			if(Result.m_WorkspacesExpanded)
				WorkspaceRow.VSplitMid(&Result.m_aWorkspaces[0], &Result.m_aWorkspaces[1], std::min(2.f, WorkspaceRow.w / 4));
			else
				for(auto &Button : Result.m_aWorkspaces)
					Button = {WorkspaceRow.x, WorkspaceRow.y, 0, 0};
			const float DividerGap = std::min(Padding, Result.m_Viewport.h / 4);
			Result.m_WorkspaceDivider = {WorkspaceRow.x, Result.m_Viewport.y + DividerGap / 2, WorkspaceRow.w, std::min(1.f, DividerGap)};
			Result.m_Viewport.HSplitTop(DividerGap, nullptr, &Result.m_Viewport);
			return Result;
		}
		CUIRect WorkspaceVisual(int Index) const
		{
			CUIRect Target[2];
			m_WorkspaceRow.VSplitMid(&Target[0], &Target[1], std::min(2.f, m_WorkspaceRow.w / 4));
			const auto Mix = [&](float A, float B) { return LucentStyle::Motion::Lerp(A, B, m_WorkspaceReveal); };
			return {Mix(m_WorkspaceRow.x, Target[Index].x), m_WorkspaceRow.y, Mix(m_WorkspaceRow.w, Target[Index].w), m_WorkspaceRow.h};
		}
		CUIRect WorkspaceIcon(int Index) const
		{
			CUIRect Target[2];
			m_WorkspaceRow.VSplitMid(&Target[0], &Target[1], std::min(2.f, m_WorkspaceRow.w / 4));
			const float Size = std::min(14.f, std::max(0.f, Target[Index].w - 4));
			const float Centered = m_Sidebar.x + std::min(CollapsedWidth, m_Sidebar.w) / 2 - Size / 2;
			const float Expanded = Target[Index].x + std::min(3.f, Target[Index].w / 8);
			return {LucentStyle::Motion::Lerp(Centered, Expanded, m_WorkspaceReveal), m_WorkspaceRow.y + (m_WorkspaceRow.h - Size) / 2, Size, Size};
		}
		float MaxScroll() const { return std::max(0.f, m_ItemCount * RowHeight - m_Viewport.h); }
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
