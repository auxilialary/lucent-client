#ifndef GAME_CLIENT_LUCENT_SERVER_CONTROLS_H
#define GAME_CLIENT_LUCENT_SERVER_CONTROLS_H

#include <game/client/ui_rect.h>

#include <algorithm>

// A small reversible timeline using the same cubic reveal as Lucent's chips.
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
		if(!Focused || !Running)
			m_Time = 0;
		else
			m_Time = Animate ? std::clamp(m_Time + (IsOpen() ? Dt : -Dt), 0.0f, 0.20f) : (IsOpen() ? 0.20f : 0.0f);
	}
	float Progress() const
	{
		const float T = m_Time / 0.20f;
		return 1 - (1 - T) * (1 - T) * (1 - T);
	}
	bool Interactive() const { return IsOpen() && Progress() > 0.05f; }
	static bool CanStart(bool Running, bool Starting) { return !Running && !Starting; }
	static CUIRect Bounds(const CUIRect &Chip, const CUIRect &Group)
	{
		// Below the right group so the popover never covers Settings or Quit.
		return {Chip.x - 6, Group.y + Group.h + 10, Chip.w + 12, 76};
	}
	static CUIRect HitArea(CUIRect Panel)
	{
		Panel.y -= 8;
		Panel.h += 8;
		return Panel;
	}
	static CUIRect Row(const CUIRect &Panel, int Index, float Progress)
	{
		return {Panel.x + 6, Panel.y + 6 + Index * 34 - 8 * (1 - Progress), Panel.w - 12, 30};
	}
};

#endif
