#include "lucent_menu_interaction.h"

#include <algorithm>
#include <cmath>

bool CLucentMenuInteraction::Contains(const CUIRect &Rect, vec2 Point)
{
	return Point.x >= Rect.x && Point.x <= Rect.x + Rect.w && Point.y >= Rect.y && Point.y <= Rect.y + Rect.h;
}

bool CLucentMenuInteraction::InTriangle(vec2 Point, vec2 A, vec2 B, vec2 C)
{
	const auto Cross = [](vec2 U, vec2 V) { return U.x * V.y - U.y * V.x; };
	if(std::abs(Cross(B - A, C - A)) < 0.001f)
		return false;
	const float X = Cross(B - A, Point - A);
	const float Y = Cross(C - B, Point - B);
	const float Z = Cross(A - C, Point - C);
	return (X >= 0 && Y >= 0 && Z >= 0) || (X <= 0 && Y <= 0 && Z <= 0);
}

void CLucentMenuInteraction::Reset()
{
	*this = CLucentMenuInteraction();
}

void CLucentMenuInteraction::Close()
{
	m_Open = false;
	m_Suppressed = true;
	m_Side = 0;
	m_TransitStart = -1;
	m_OutsideSince = -1;
}

bool CLucentMenuInteraction::Update(double Now, vec2 Mouse, const CUIRect &Logo, const CUIRect &Left, const CUIRect &Right, bool OverControl, bool Keyboard, bool CanResume)
{
	const bool OverLogo = Contains(Logo, Mouse);
	const int Region = OverLogo ? 1 : OverControl ? (Contains(Left, Mouse) ? 2 : Contains(Right, Mouse) ? 3 : 4) : 0;
	// A resize/UI-scale change invalidates cursor history in the old coordinate
	// system. Keep the open state and give the new layout its normal grace.
	const CUIRect aLayout[] = {Logo, Left, Right};
	bool LayoutChanged = false;
	for(int i = 0; i < 3; ++i)
	{
		const CUIRect &Old = m_aLayout[i], &New = aLayout[i];
		LayoutChanged |= m_HasLayout && (Old.x != New.x || Old.y != New.y || Old.w != New.w || Old.h != New.h);
		m_aLayout[i] = New;
	}
	m_HasLayout = true;
	if(LayoutChanged)
	{
		m_PreviousRegion = 0;
		m_Side = 0;
		m_OutsideSince = Now;
	}
	const bool Initialized = m_LastTime >= 0 && !LayoutChanged;
	const double Dt = Initialized ? Now - m_LastTime : 0;
	const vec2 Delta = Initialized ? Mouse - m_Previous : vec2(0, 0);
	if(!OverLogo && !OverControl)
		m_Suppressed = false;
	if(Keyboard)
		m_Suppressed = false;
	if((OverLogo && !m_Suppressed) || Keyboard || ((m_Open || (CanResume && !m_Suppressed)) && OverControl))
	{
		m_Open = true;
		m_OutsideSince = -1;
		m_Side = 0;
		m_TransitStart = -1;
	}
	else if(m_Open)
	{
		if(m_PreviousRegion != 0 && Initialized)
		{
			m_Origin = m_Previous;
			m_Returning = m_PreviousRegion != 1;
			m_Side = m_PreviousRegion == 2 ? -1 : m_PreviousRegion == 3 ? 1 : Mouse.x < Logo.x + Logo.w / 2 ? -1 : 1;
			m_TransitStart = Now;
			m_LastProgress = Now;
		}
		const float Speed = Dt > 0 ? length(Delta) / static_cast<float>(Dt) : 0;
		const float ReverseTolerance = std::clamp(Speed * static_cast<float>(std::min(Dt, 0.05)) * 0.04f, 0.05f, 0.5f);
		float Forward = Delta.x * m_Side * (m_Returning ? -1 : 1);
		// Reverse from the last sample without restarting the transit deadline.
		if(m_Side != 0 && Forward < -ReverseTolerance)
		{
			m_Returning = !m_Returning;
			m_Origin = m_Previous;
			Forward = -Forward;
		}
		const CUIRect &Target = m_Returning ? Logo : m_Side < 0 ? Left : Right;
		const float Edge = (m_Side < 0) != m_Returning ? Target.x + Target.w : Target.x;
		if(Forward > 0.001f)
			m_LastProgress = Now;
		const bool Corridor = m_Side != 0 &&
			Now - m_TransitStart < 1.8 && Now - m_LastProgress < 0.3 &&
			InTriangle(Mouse, m_Origin, vec2(Edge, Target.y), vec2(Edge, Target.y + Target.h));
		if(Corridor)
			m_OutsideSince = -1;
		else
		{
			m_Side = 0;
			if(m_OutsideSince < 0)
				m_OutsideSince = Now;
			if(Now - m_OutsideSince >= 0.25)
			{
				Close();
				// Natural hover closure may reverse while its visuals remain.
				// Explicit Escape/focus closure keeps suppression instead.
				m_Suppressed = false;
			}
		}
	}
	m_Previous = Mouse;
	m_LastTime = Now;
	m_PreviousRegion = Region;
	return m_Open;
}
