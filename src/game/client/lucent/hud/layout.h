#ifndef GAME_CLIENT_LUCENT_HUD_LAYOUT_H
#define GAME_CLIENT_LUCENT_HUD_LAYOUT_H
#include <game/client/lucent/style/tokens.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace LucentHud
{
	// Virtual HUD coordinates: 300 units high at every physical resolution.
	inline constexpr float Margin = 6.f, Gap = 4.f, Padding = 4.f;
	inline constexpr float BodySize = 6.f, CaptionSize = 5.f, TimerSize = 9.f;
	// Menu coordinates are twice as dense as the 300-unit HUD.
	inline constexpr float MenuUnit = .5f;
	// HUD panels carry a different shape role from small menu inputs: round
	// compact panels to half their short side, cap larger panels at a control's
	// half-height. Keep the coordinate conversion, not the menu input radius.
	inline constexpr float TextPaddingY = 3.f, Radius = LucentStyle::Style::ControlSize * MenuUnit;
	inline constexpr float TimerInset = 3.f, TimerPaddingX = 6.f, TimerLineGap = 3.f, ClockLineGap = 2.f;
	inline constexpr float CoordinatesSize = 5.f, CoordinatesPadding = 2.f;
	inline constexpr float StatusGroupGap = 3.f;
	inline constexpr int MaxStatusItems = 32;
	// Match the existing renderer's integer FreeType glyph sizes (6..128 px).
	inline float TargetFontSize(float Size, int PixelHeight, float VirtualHeight)
	{
		const float Ratio = std::max(1, PixelHeight) / std::max(1.f, VirtualHeight);
		const float Pixels = std::clamp(std::round(Size * Ratio), 6.f, 128.f);
		// Text renderer truncates; epsilon avoids losing a pixel through float conversion.
		return (Pixels + .001f) / Ratio;
	}
	// Measure format cells, never changing the displayed time string.
	template<size_t N>
	inline void TimerFormatTemplate(char (&Out)[N], const char *pText)
	{
		static_assert(N > 0);
		size_t i = 0;
		for(; i + 1 < N && pText[i]; ++i)
			Out[i] = pText[i] >= '0' && pText[i] <= '9' ? '0' : pText[i];
		Out[i] = 0;
	}
	struct CJumpState
	{
		int m_Total, m_Available, m_Used;
		bool m_Known;
	};
	inline CJumpState Jumps(int Jumps, int JumpedTotal, int Jumped, bool Grounded, bool Endless, bool Known)
	{
		const int Total = (int)std::min<int64_t>(Jumps < 0 ? -(int64_t)Jumps : Jumps, INT32_MAX);
		if(!Known)
			return {Total, 0, 0, false};
		int64_t Used = Jumps > 1 ? (int64_t)JumpedTotal + !Grounded : Jumps == 1 ? (Jumped & 2) :
								      Jumps == -1        ? !Grounded :
											   0;
		if(Endless && Total > 0 && Used >= Total)
			Used = Total - 1;
		int64_t Available = Total - Used;
		if(!(Jumped & 2) && Available <= 0)
			Available = 1;
		const int Remaining = (int)std::clamp<int64_t>(Available, 0, Total);
		return {Total, Remaining, Total - Remaining, true};
	}
	inline constexpr float WeaponIconScale = .92f; // +17.95% relative to v1.1's .78.
	// HUD uses half the menu UI coordinate density: 22 HUD units = 44 UI units.
	inline constexpr float SlotWidth = 22.f, SlotHeight = 20.f, SlotGap = 2.5f;
	inline constexpr float BarPadding = 2.f, BarHeight = SlotHeight + 2 * BarPadding;
	inline constexpr float HotbarFactor = .85f;
	struct CRect
	{ float x, y, w, h; };
	inline float PanelRadius(const CRect &Rect, float Scale)
	{
		return std::max(0.f, std::min({Radius * Scale, Rect.h / 2, Rect.w / 2}));
	}
	struct CStatusLayout
	{
		std::array<CRect, MaxStatusItems> m_aItems{};
		std::array<CRect, MaxStatusItems> m_aRows{};
		std::array<CRect, 2> m_aGroups{};
		CRect m_Bounds = {};
		int m_RowCount = 0;
	};

	struct CWeapons
	{
		std::array<int, 6> m_aIds{};
		int m_Count = 0;
	};
	inline CWeapons Weapons(unsigned Owned, int Active, bool InventoryKnown)
	{
		CWeapons Result;
		// Vanilla snapshots reveal the current weapon, not a complete inventory.
		const unsigned ActiveMask = Active >= 0 && Active < 6 ? 1u << Active : 0;
		const unsigned Mask = InventoryKnown ? Owned | ActiveMask : ActiveMask;
		for(int Weapon = 0; Weapon < 6; ++Weapon)
			if(Mask & (1u << Weapon))
				Result.m_aIds[Result.m_Count++] = Weapon;
		return Result;
	}
	struct CLayout
	{
		float m_Width, m_Height, m_Scale;
		CLayout(float Width, float Height, int Scale) : m_Width(Width), m_Height(Height), m_Scale(std::clamp(Scale, 50, 150) / 100.f) {}
		float Unit(float Value) const { return Value * m_Scale; }
		float BarUnit(float Value) const { return Unit(Value * HotbarFactor); }
		// Keep the v1.3 status/information anchor independent of hotbar size.
		float InformationAnchorY() const { return m_Height - Unit(Margin + BarHeight); }
		CRect TopCenter(float Width, float Height) const { return {(m_Width - Unit(Width)) / 2, Unit(Margin), Unit(Width), Unit(Height)}; }
		CRect TopRight(float Width, float Height, float OffsetY = 0) const { return {m_Width - Unit(Margin + Width), Unit(Margin) + OffsetY, Unit(Width), Unit(Height)}; }
		CRect Timer(float ContentWidth, bool Main, bool Secondary, float ClockWidth = 0) const
		{
			if(!Main && !Secondary && ClockWidth <= 0)
				return {};
			const float Height = 2 * TimerInset + (Main ? TimerSize : 0) + (Secondary ? BodySize : 0) + (Main && Secondary ? TimerLineGap : 0) +
					     (ClockWidth > 0 ? CaptionSize + (Main || Secondary ? ClockLineGap : 0) : 0);
			return TopCenter(std::max(ContentWidth, ClockWidth) + 2 * TimerPaddingX, Height);
		}
		float TimerLineY(const CRect &Bounds, bool Secondary, bool Main) const
		{
			return Bounds.y + Unit(TimerInset + (Secondary && Main ? TimerSize + TimerLineGap : 0));
		}
		float TimerClockY(const CRect &Bounds, bool Main, bool Secondary) const
		{
			return Bounds.y + Unit(TimerInset + (Main ? TimerSize : 0) + (Secondary ? BodySize : 0) + (Main && Secondary ? TimerLineGap : 0) + (Main || Secondary ? ClockLineGap : 0));
		}
		CRect Hotbar(int Count) const
		{
			const float Width = Count > 0 ? Count * SlotWidth + (Count - 1) * SlotGap + 2 * BarPadding : 0;
			return {(m_Width - BarUnit(Width)) / 2, m_Height - Unit(Margin) - BarUnit(BarHeight), BarUnit(Width), BarUnit(BarHeight)};
		}
		CRect Slot(int Index, int Count) const
		{
			auto Bar = Hotbar(Count);
			return {Bar.x + BarUnit(BarPadding + Index * (SlotWidth + SlotGap)), Bar.y + BarUnit(BarPadding), BarUnit(SlotWidth), BarUnit(SlotHeight)};
		}
		CStatusLayout Status(const std::array<float, MaxStatusItems> &Widths, int Count, float SideClearance = 0, int JumpCount = 0) const
		{
			CStatusLayout Result;
			Count = std::clamp(Count, 0, MaxStatusItems);
			JumpCount = std::clamp(JumpCount, 0, Count);
			if(Count == 0)
				return Result;
			const float Padding = Unit(2), Space = Unit(2), RowHeight = Unit(12), GroupGap = Unit(StatusGroupGap);
			float MaxWidth = std::min({m_Width - 2 * Unit(Margin), m_Width * .44f, Unit(180)});
			const float CenteredSpace = m_Width - 2 * SideClearance;
			if(CenteredSpace >= Unit(48))
				MaxWidth = std::min(MaxWidth, CenteredSpace);
			auto NaturalWidth = [&](int Begin, int End) {
				float Width = 0;
				for(int i = Begin; i < End; ++i)
					Width += Unit(Widths[i]) + (i > Begin ? Space : 0);
				return End > Begin ? Width + 2 * Padding : 0.f;
			};
			const bool Both = JumpCount > 0 && Count > JumpCount;
			const float JumpBudget = std::min(MaxWidth, NaturalWidth(0, JumpCount));
			// Avoid a very tall one-icon modifier column beside a wide jump group.
			const float ModifierMinimum = std::min(NaturalWidth(JumpCount, Count), Unit(32));
			const bool Horizontal = Both && JumpBudget + GroupGap + ModifierMinimum <= MaxWidth;
			std::array<int, MaxStatusItems> RowGroups{};
			auto Pack = [&](int Begin, int End, float Budget, int Group) {
				if(Begin == End)
					return;
				const float Content = std::max(Unit(8), Budget - 2 * Padding);
				const int FirstRow = Result.m_RowCount;
				int Row = FirstRow;
				std::array<float, MaxStatusItems> RowWidths{};
				for(int i = Begin; i < End; ++i)
				{
					const float Width = std::min(Unit(Widths[i]), Content);
					if(RowWidths[Row] > 0 && RowWidths[Row] + Space + Width > Content)
						++Row;
					Result.m_aItems[i] = {RowWidths[Row] + (RowWidths[Row] > 0 ? Space : 0), (float)Row, Width, Unit(8)};
					RowWidths[Row] = Result.m_aItems[i].x + Width;
				}
				Result.m_RowCount = Row + 1;
				float GroupWidth = 0;
				for(int r = FirstRow; r <= Row; ++r)
					GroupWidth = std::max(GroupWidth, RowWidths[r] + 2 * Padding);
				Result.m_aGroups[Group] = {0, 0, GroupWidth, (Row - FirstRow + 1) * RowHeight + (Row - FirstRow) * Space};
				for(int r = FirstRow; r <= Row; ++r)
				{
					const float Width = RowWidths[r] + 2 * Padding;
					Result.m_aRows[r] = {(GroupWidth - Width) / 2, (r - FirstRow) * (RowHeight + Space), Width, RowHeight};
					RowGroups[r] = Group;
				}
				for(int i = Begin; i < End; ++i)
				{
					const int r = (int)Result.m_aItems[i].y;
					Result.m_aItems[i].x += Result.m_aRows[r].x + Padding;
					Result.m_aItems[i].y = Result.m_aRows[r].y + Padding;
				}
			};
			Pack(0, JumpCount, JumpBudget, 0);
			Pack(JumpCount, Count, Horizontal ? MaxWidth - Result.m_aGroups[0].w - GroupGap : MaxWidth, 1);
			auto &Jumps = Result.m_aGroups[0];
			auto &Modifiers = Result.m_aGroups[1];
			const float Width = Horizontal ? Jumps.w + GroupGap + Modifiers.w : std::max(Jumps.w, Modifiers.w);
			const float Height = Horizontal ? std::max(Jumps.h, Modifiers.h) : Jumps.h + Modifiers.h + (Both ? GroupGap : 0);
			float Bottom = InformationAnchorY() - Unit(Gap);
			if(SideClearance > 0 && CenteredSpace < Unit(48))
				Bottom = std::min(Bottom, 200.f - Unit(Gap));
			Result.m_Bounds = {(m_Width - Width) / 2, Bottom - Height, Width, Height};
			Jumps.x = Result.m_Bounds.x;
			Jumps.y = Horizontal ? Bottom - Jumps.h : Result.m_Bounds.y;
			Modifiers.x = Horizontal ? Jumps.x + Jumps.w + GroupGap : Result.m_Bounds.x;
			Modifiers.y = Horizontal ? Bottom - Modifiers.h : Result.m_Bounds.y + Jumps.h + (Both ? GroupGap : 0);
			for(int r = 0; r < Result.m_RowCount; ++r)
			{
				const auto &Group = Result.m_aGroups[RowGroups[r]];
				Result.m_aRows[r].x += Group.x;
				Result.m_aRows[r].y += Group.y;
			}
			for(int i = 0; i < Count; ++i)
			{
				const auto &Group = Result.m_aGroups[i < JumpCount ? 0 : 1];
				Result.m_aItems[i].x += Group.x;
				Result.m_aItems[i].y += Group.y;
			}
			return Result;
		}
		CRect Coordinates(float Width, bool ScoreVisible, bool SpeedVisible, const CRect &StatusBounds = {}) const
		{
			// Reserve the existing score HUD and the centered hotbar without moving either.
			const float Bottom = std::min(m_Height - 15.f - (ScoreVisible ? 56.f : 0.f) - Unit(Gap), InformationAnchorY() - Unit(Gap));
			const float Height = Unit(SpeedVisible ? 15.f : 8.f);
			CRect Rect = {m_Width - Unit(Margin) - Width, Bottom - Height, Width, Height};
			if(StatusBounds.w > 0 && Rect.x < StatusBounds.x + StatusBounds.w && Rect.x + Rect.w > StatusBounds.x && Rect.y + Rect.h + Unit(Gap) > StatusBounds.y)
				Rect.y = std::min(Rect.y, StatusBounds.y - Unit(Gap) - Rect.h);
			return Rect;
		}
	};
	// Keep optional readouts clear of the timer even at narrow aspect ratios.
	inline CRect BelowIfOverlapping(CRect Moving, const CRect &Fixed, float Distance)
	{
		if(Fixed.w > 0 && Moving.x < Fixed.x + Fixed.w + Distance && Moving.x + Moving.w + Distance > Fixed.x && Moving.y < Fixed.y + Fixed.h + Distance && Moving.y + Moving.h + Distance > Fixed.y)
			Moving.y = Fixed.y + Fixed.h + Distance;
		return Moving;
	}
	inline float Opacity(int Percent) { return std::clamp(Percent, 0, 100) / 100.f; }
}
#endif
