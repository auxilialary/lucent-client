#ifndef GAME_CLIENT_LUCENT_SERVER_CONTROLS_H
#define GAME_CLIENT_LUCENT_SERVER_CONTROLS_H

#include <game/client/ui_rect.h>

#include <algorithm>

// Reusable inline insertion: content dimensions determine every following offset.
class CLucentAnimatedInsertion
{
public:
	static float Height(float RowHeight, float Gap, int Count, float Progress)
	{
		return (RowHeight + Gap) * Count * Progress;
	}
	static void Offset(CUIRect &Rect, float Height) { Rect.y += Height; }
};

// A reversible 360 ms timeline shared by layout and staggered child reveals.
// Dismissal clears intent: parent re-entry must never reopen a stale panel.
class CLucentServerControls
{
	bool m_Requested = false;
	bool m_Available = false;
	float m_Time = 0;

public:
	void Reset() { *this = CLucentServerControls(); }
	void Open() { m_Requested = true; }
	void Toggle() { m_Requested = !m_Requested; }
	void Close() { m_Requested = false; }
	bool IsOpen() const { return m_Requested && m_Available; }
	void Update(float Dt, bool Animate, bool Running, bool ParentOpen, bool Focused)
	{
		m_Available = Running && ParentOpen && Focused;
		if(!m_Available)
			Close();
		if(!Focused)
			m_Time = 0;
		else
			m_Time = Animate ? std::clamp(m_Time + (IsOpen() ? Dt : -Dt), 0.0f, 0.36f) : (IsOpen() ? 0.36f : 0.0f);
	}
	float Progress() const
	{
		const float T = m_Time / 0.36f;
		return T * T * (3 - 2 * T);
	}
	float Reveal(int Index) const
	{
		const float T = std::clamp((m_Time - Index * 0.055f) / 0.305f, 0.0f, 1.0f);
		return T * T * (3 - 2 * T);
	}
	bool Interactive() const { return IsOpen() && Progress() > 0.05f; }
	static bool CanStart(bool Running, bool Starting) { return !Running && !Starting; }
	static float Gap(const CUIRect &Chip) { return Chip.h * 0.24f; }
	static float Height(const CUIRect &Chip, float Progress)
	{
		return CLucentAnimatedInsertion::Height(Chip.h, Gap(Chip), 2, Progress);
	}
	static CUIRect Bounds(const CUIRect &Chip, float Progress)
	{
		return {Chip.x, Chip.y + Chip.h, Chip.w, Height(Chip, Progress)};
	}
	static CUIRect HitArea(CUIRect Panel) { return Panel; }
	static CUIRect Row(const CUIRect &Chip, int Index, float Reveal)
	{
		// Keep the chip and typography at their normal size. The expanding
		// group's clip exposes them while each chip settles through one gap.
		return {Chip.x, Chip.y + Chip.h + Index * (Chip.h + Gap(Chip)) + Gap(Chip) * Reveal,
			Chip.w, Chip.h};
	}
	static CUIRect VisibleRow(const CUIRect &Chip, int Index, float Progress, float Reveal)
	{
		CUIRect Result = Row(Chip, Index, Reveal);
		const CUIRect Group = Bounds(Chip, Progress);
		Result.h = std::max(0.0f, std::min(Result.h, Group.y + Group.h - Result.y));
		return Result;
	}
};

#endif
