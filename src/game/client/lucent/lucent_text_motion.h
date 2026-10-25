#ifndef GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H
#define GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H

#include "lucent_style.h"

#include <base/vmath.h>

// Geometry only: never retain input text, glyphs or password contents.
class CLucentTextMotion
{
	vec2 m_From = vec2(0, 0), m_Target = vec2(0, 0);
	double m_Start = 0;
	bool m_Valid = false;
	bool m_Jump = false;

public:
	void Reset() { m_Valid = false; }
	vec2 Position(double Now) const
	{
		const float T = LucentStyle::Motion::CubicOut(float((Now - m_Start) / LucentStyle::Motion::TextCaret));
		return m_From + (m_Target - m_From) * T;
	}
	void Update(vec2 Target, float Height, double Now, bool Animate, bool Reposition)
	{
		if(!m_Valid || !Animate || Reposition)
		{
			m_From = m_Target = Target;
			m_Start = Now - LucentStyle::Motion::TextTrail;
			m_Valid = true;
			m_Jump = false;
			return;
		}
		if(Target == m_Target)
			return;
		const vec2 Visible = Position(Now);
		// Line changes and long jumps reposition without a stripe across the text.
		const bool Jump = std::abs(Target.y - m_Target.y) > Height * .5f || distance(Target, Visible) > Height * LucentStyle::Motion::TextJumpLimit;
		m_Jump = Jump;
		m_From = Jump ? Target : Visible;
		m_Target = Target;
		m_Start = Now;
	}
	vec2 Tail(double Now, float Height) const
	{
		const vec2 Visible = Position(Now);
		const vec2 Delta = m_From - Visible;
		const float Length = length(Delta);
		return Visible + Delta * (Length > 0 ? std::min(1.f, Height * LucentStyle::Motion::TextTrailLength / Length) : 0.f);
	}
	float TrailAlpha(double Now) const
	{
		return LucentStyle::Motion::TextTrailIntensity * (1 - std::clamp(float((Now - m_Start) / LucentStyle::Motion::TextTrail), 0.f, 1.f));
	}
	float CaretAlpha(double Now) const
	{
		return m_Jump ? LucentStyle::Motion::CubicOut(float((Now - m_Start) / LucentStyle::Motion::TextCaret)) : 1.f;
	}
	bool Moving(double Now) const { return Now - m_Start < LucentStyle::Motion::TextTrail; }
};
#endif
