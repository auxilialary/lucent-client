#ifndef GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H
#define GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H

#include "lucent_style.h"
#include <base/vmath.h>

// Bounded geometry only. The logical caret never follows this decoration.
class CLucentTextMotion
{
	vec2 m_Target = vec2(0, 0);
	double m_LastMove = 0;
	float m_Speed = 0, m_GlyphWidth = 0, m_Direction = 0;
	bool m_Valid = false;

public:
	void Reset() { m_Valid = false; }
	float Decay(double Now) const
	{
		return 1 - LucentStyle::Motion::CubicOut(float((Now - m_LastMove) / LucentStyle::Motion::TextTrail));
	}
	vec2 Position(double Now) const
	{
		const float Speed = std::clamp((m_Speed - LucentStyle::Motion::TextSpeedLow) /
			(LucentStyle::Motion::TextSpeedHigh - LucentStyle::Motion::TextSpeedLow), 0.f, 1.f);
		return m_Target + vec2(m_Direction * m_GlyphWidth * LucentStyle::Motion::TextLeadGlyphs * Speed * Decay(Now), 0);
	}
	void Update(vec2 Target, float Height, double Now, bool Animate, bool Reposition)
	{
		if(!m_Valid || !Animate || Reposition)
		{
			m_Target = Target;
			m_LastMove = Now - LucentStyle::Motion::TextTrail;
			m_Speed = m_Direction = m_GlyphWidth = 0;
			m_Valid = true;
			return;
		}
		if(Target == m_Target)
			return;
		const vec2 Delta = Target - m_Target;
		const double Interval = Now - m_LastMove;
		if(std::abs(Delta.y) > Height * .5f || std::abs(Delta.x) > Height * LucentStyle::Motion::TextJumpLimit)
		{
			Update(Target, Height, Now, Animate, true);
			return;
		}
		const float Direction = Delta.x < 0 ? -1.f : 1.f;
		const float Speed = std::abs(Delta.x) / std::max(float(Interval), LucentStyle::Motion::TextSpeedResponse);
		const float Blend = 1 - std::exp(-float(Interval) / LucentStyle::Motion::TextSpeedResponse);
		m_Speed = Interval > LucentStyle::Motion::TextIdleInterval || Direction != m_Direction ? 0 : m_Speed + (Speed - m_Speed) * Blend;
		m_Direction = Direction;
		m_GlyphWidth = std::abs(Delta.x);
		m_Target = Target;
		m_LastMove = Now;
	}
	// Translate when the editor scrolls, without interpreting scroll as typing.
	void Translate(vec2 Offset) { m_Target += Offset; }
	vec2 Tail(double Now, float Height) const
	{
		const float Speed = std::clamp(m_Speed / LucentStyle::Motion::TextSpeedHigh, 0.f, 1.f);
		return Position(Now) - vec2(m_Direction * Height * LucentStyle::Motion::TextTrailLength * Speed * Decay(Now), 0);
	}
	float TrailAlpha(double Now) const { return LucentStyle::Motion::TextTrailIntensity * Decay(Now); }
	bool Moving(double Now) const { return Decay(Now) > 0; }
};
#endif
