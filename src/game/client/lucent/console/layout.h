#ifndef GAME_CLIENT_LUCENT_CONSOLE_LAYOUT_H
#define GAME_CLIENT_LUCENT_CONSOLE_LAYOUT_H

#include <game/client/lucent/lucent_style.h>

#include <array>
#include <span>
#include <utility>

namespace LucentConsole
{
	enum class EEmptyHistory
	{
		NONE,
		BACKLOG,
		FILTERED
	};
	inline EEmptyHistory EmptyHistory(bool HasBacklog, bool HasVisibleEntries)
	{
		return HasVisibleEntries ? EEmptyHistory::NONE : HasBacklog ? EEmptyHistory::FILTERED :
									      EEmptyHistory::BACKLOG;
	}
	enum class EPopover
	{
		NONE,
		MORE,
		LEVELS
	};
	struct CPopoverState
	{
		EPopover m_Open = EPopover::NONE, m_Display = EPopover::NONE;
		bool IsOpen() const { return m_Open != EPopover::NONE; }
		bool Close()
		{
			const bool WasOpen = IsOpen();
			m_Open = EPopover::NONE;
			return WasOpen;
		}
		void Toggle(EPopover Which)
		{
			m_Open = m_Open == Which ? EPopover::NONE : Which;
			if(IsOpen())
				m_Display = m_Open;
		}
		bool DismissOutside(bool InsidePopup, bool InsideAnchor)
		{
			return IsOpen() && !InsidePopup && !InsideAnchor && Close();
		}
	};
	inline bool CompactPanel(float Width) { return Width < LucentStyle::Console::FilterFullWidth; }
	inline int PanelColumns(float Width) { return std::max(1, int(Width / LucentStyle::Console::FilterMinWidth)); }
	inline float PanelHeight(float Width, bool More, bool Levels)
	{
		return LucentStyle::Console::FilterHeight;
	}
	inline float PopoverContentHeight(bool Levels, bool Remote, float HintHeight)
	{
		using namespace LucentStyle::Console;
		return 2 * PopoverPadding + (Levels ? 2 * PopoverHeader + 3 * PopoverRow + FilterGap + (Remote ? 0 : PopoverRow) + HintHeight : 5 * PopoverRow);
	}
	struct COverlayPosition
	{ float m_X, m_Y, m_W, m_H; };
	inline bool Overlaps(const COverlayPosition &A, const COverlayPosition &B)
	{
		return A.m_X < B.m_X + B.m_W && A.m_X + A.m_W > B.m_X && A.m_Y < B.m_Y + B.m_H && A.m_Y + A.m_H > B.m_Y;
	}
	inline COverlayPosition CopyBeside(const COverlayPosition &Fragment, float Width, float Height, const COverlayPosition &Bounds, std::span<const COverlayPosition> Occupied)
	{
		const float Gap = LucentStyle::Console::CopyGap;
		const float Y = std::clamp(Fragment.m_Y, Bounds.m_Y, std::max(Bounds.m_Y, Bounds.m_Y + Bounds.m_H - Height));
		const float X = std::clamp(Fragment.m_X, Bounds.m_X, std::max(Bounds.m_X, Bounds.m_X + Bounds.m_W - Width));
		const std::array<COverlayPosition, 3> Candidates = {{{Fragment.m_X + Fragment.m_W + Gap, Y, Width, Height},
			{Fragment.m_X - Gap - Width, Y, Width, Height}, {X, Fragment.m_Y + Fragment.m_H + Gap, Width, Height}}};
		const auto Fits = [&](const COverlayPosition &Rect) {
			return Rect.m_X >= Bounds.m_X && Rect.m_Y >= Bounds.m_Y && Rect.m_X + Rect.m_W <= Bounds.m_X + Bounds.m_W && Rect.m_Y + Rect.m_H <= Bounds.m_Y + Bounds.m_H &&
			       std::none_of(Occupied.begin(), Occupied.end(), [&](const auto &Other) { return Overlaps(Rect, Other); });
		};
		for(const auto &Rect : Candidates)
			if(Fits(Rect))
				return Rect;
		// A fully selected viewport may have no legal floating-control location.
		return {};
	}
	inline COverlayPosition AboveAnchor(float AnchorX, float AnchorY, float Width, float Height, float BoundsX, float BoundsY, float BoundsW, float BoundsH, float Gap)
	{
		Width = std::min(Width, std::max(0.f, BoundsW));
		Height = std::min(Height, std::max(0.f, BoundsH));
		return {std::clamp(AnchorX, BoundsX, BoundsX + BoundsW - Width),
			std::clamp(AnchorY - Gap - Height, BoundsY, BoundsY + BoundsH - Height), Width, Height};
	}
	inline COverlayPosition CopyOverlay(float AnchorX, float AnchorY, float LineHeight, float Width, float Height, float BoundsX, float BoundsY, float BoundsW, float BoundsH)
	{
		const float Gap = LucentStyle::Console::Gap;
		auto Result = AboveAnchor(AnchorX, AnchorY, Width, Height, BoundsX, BoundsY, BoundsW, BoundsH, Gap);
		if(AnchorY - Height - Gap < BoundsY && AnchorY + LineHeight + Gap + Height <= BoundsY + BoundsH)
			Result.m_Y = AnchorY + LineHeight + Gap;
		return Result;
	}
	inline std::pair<int, int> PillSelection(int Start, int End, int CharacterOffset, int VisibleCharacters)
	{
		return {std::clamp(Start - CharacterOffset, 0, VisibleCharacters), std::clamp(End - CharacterOffset, 0, VisibleCharacters)};
	}
	inline float CopyControlX(float EndX, float Width, float BoundsWidth)
	{
		return std::clamp(EndX, 0.f, std::max(0.f, BoundsWidth - Width));
	}
}
#endif
