#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_INTERACTION_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_INTERACTION_H
#include <base/vmath.h>

#include <game/client/ui_rect.h>

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
	bool Update(double Now, vec2 Mouse, const CUIRect &Logo, const CUIRect &Left, const CUIRect &Right, bool OverControl, bool Keyboard);
	bool IsOpen() const { return m_Open; }
};
#endif
