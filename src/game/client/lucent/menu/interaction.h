#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_INTERACTION_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_INTERACTION_H
#include <game/client/lucent/motion/transition.h>

#include <base/vmath.h>

#include <game/client/ui_rect.h>

#include <algorithm>

// All positions are in CUi screen coordinates, never physical pixels.
class CLucentMenuInteraction
{
	vec2 m_Previous = vec2(0, 0);
	vec2 m_Origin = vec2(0, 0);
	double m_LastTime = -1;
	double m_TransitStart = -1;
	double m_LastProgress = -1;
	double m_OutsideSince = -1;
	int m_Side = 0;
	CUIRect m_aLayout[3] = {};
	bool m_HasLayout = false;
	int m_PreviousRegion = 0;
	bool m_Returning = false;
	bool m_Open = false;
	bool m_Suppressed = false;

public:
	static bool Contains(const CUIRect &Rect, vec2 Point);
	static bool InTriangle(vec2 Point, vec2 A, vec2 B, vec2 C);
	void Reset();
	void Close();
	bool Update(double Now, vec2 Mouse, const CUIRect &Logo, const CUIRect &Left, const CUIRect &Right, bool OverControl, bool Keyboard, bool CanResume = false);
	bool IsOpen() const { return m_Open; }
};

// Reversible timeline: five 160 ms reveals, staggered by 40 ms (320 ms total).
// Intent survives a parent's temporary closure, but never a completed closure.
class CLucentSecondaryNavigation
{
	bool m_Requested = false;
	bool m_ParentOpen = false;
	float m_Time = 0;

public:
	void Reset() { *this = CLucentSecondaryNavigation(); }
	void Toggle() { m_Requested = !m_Requested; }
	void Close() { m_Requested = false; }
	bool IsOpen() const { return m_Requested && m_ParentOpen; }
	void Update(float Dt, bool Animate, bool ParentOpen, bool ParentVisible, bool Focused)
	{
		if(!Focused || !ParentVisible)
		{
			Reset();
			return;
		}
		m_ParentOpen = ParentOpen;
		const bool Open = IsOpen();
		m_Time = Animate ? std::clamp(m_Time + (Open ? Dt : -Dt), 0.0f, LucentStyle::Motion::SecondaryReveal) : (Open ? LucentStyle::Motion::SecondaryReveal : 0.0f);
	}
	float Progress(int Row) const
	{
		return LucentStyle::Motion::Stagger(m_Time, Row * LucentStyle::Motion::SecondaryStagger, LucentStyle::Motion::SecondaryItem);
	}
	bool Interactive(int Row) const { return IsOpen() && Progress(Row) > 0.05f; }
};

#endif
