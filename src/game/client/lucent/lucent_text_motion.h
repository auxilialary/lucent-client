#ifndef GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H
#define GAME_CLIENT_LUCENT_LUCENT_TEXT_MOTION_H

#include "lucent_style.h"

#include <base/vmath.h>

#include <array>

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
						       (LucentStyle::Motion::TextSpeedHigh - LucentStyle::Motion::TextSpeedLow),
			0.f, 1.f);
		return m_Target + vec2(m_Direction * m_GlyphWidth * LucentStyle::Motion::TextLeadGlyphs * Speed * Decay(Now), 0);
	}
	void Update(vec2 Target, float Height, double Now, bool Animate, bool Reposition, int Glyphs = 1)
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
		if(std::abs(Delta.y) > Height * .5f || std::abs(Delta.x) / std::max(1, Glyphs) > Height * LucentStyle::Motion::TextJumpLimit)
		{
			Update(Target, Height, Now, Animate, true);
			return;
		}
		const float Direction = Delta.x < 0 ? -1.f : 1.f;
		const float Speed = std::abs(Delta.x) / std::max(float(Interval), LucentStyle::Motion::TextMinEventInterval);
		const float Blend = 1 - std::exp(-float(Interval) / LucentStyle::Motion::TextSpeedResponse);
		// Seed from the new direction immediately. Never carry rightward velocity
		// into Backspace, or accumulate a delayed caret position.
		if(Interval > LucentStyle::Motion::TextIdleInterval)
			m_Speed = 0;
		else if(Direction != m_Direction)
			m_Speed = Speed;
		else
			m_Speed += (Speed - m_Speed) * Blend;
		m_Direction = Direction;
		m_GlyphWidth = std::min(Height * LucentStyle::Motion::TextJumpLimit, std::abs(Delta.x) / std::max(1, Glyphs));
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
	float TrailAlpha(double Now) const
	{
		const float Speed = std::clamp(m_Speed / LucentStyle::Motion::TextSpeedHigh, 0.f, 1.f);
		return LucentStyle::Motion::TextTrailIntensity * LucentStyle::Motion::Lerp(LucentStyle::Motion::TextTrailIdleIntensity, 1.f, Speed) * Decay(Now);
	}
	bool Moving(double Now) const { return Decay(Now) > 0; }
};
// Short-lived edit ranges in decoded Unicode positions, never byte offsets or a
// per-frame text copy. A paste/IME commit uses one range regardless of its length.
class CLucentTextAppear
{
	struct CRange
	{
		int m_Begin = 0, m_End = 0;
		double m_Start = 0;
	};
	std::array<CRange, 32> m_aRanges{};
	size_t m_Count = 0;

public:
	void Reset() { m_Count = 0; }
	void Replace(int Begin, int Removed, int Added, double Now)
	{
		std::array<CRange, 32> aNext{};
		size_t Count = 0;
		const auto Add = [&](int Start, int End, double Time) {
			// Reserve one slot for the newest edit; overflowed old fades finish.
			if(Start < End && Count < aNext.size() - 1)
				aNext[Count++] = {Start, End, Time};
		};
		for(size_t i = 0; i < m_Count; ++i)
		{
			const auto &Range = m_aRanges[i];
			if(Now - Range.m_Start >= LucentStyle::Motion::TextAppear)
				continue;
			Add(Range.m_Begin, std::min(Range.m_End, Begin), Range.m_Start);
			Add(std::max(Range.m_Begin, Begin + Removed) + Added - Removed, Range.m_End + Added - Removed, Range.m_Start);
		}
		if(Added > 0)
			aNext[Count++] = {Begin, Begin + Added, Now};
		m_aRanges = aNext;
		m_Count = Count;
	}
	float Alpha(int Character, double Now, int CompositionBegin = 0, int CompositionLength = 0) const
	{
		if(Character >= CompositionBegin)
		{
			if(Character < CompositionBegin + CompositionLength)
				return 1.f; // The IME owns its preedit text.
			Character -= CompositionLength;
		}
		for(size_t i = 0; i < m_Count; ++i)
			if(Character >= m_aRanges[i].m_Begin && Character < m_aRanges[i].m_End)
				return LucentStyle::Motion::Smooth(float((Now - m_aRanges[i].m_Start) / LucentStyle::Motion::TextAppear));
		return 1.f;
	}
	bool Active(double Now) const
	{
		for(size_t i = 0; i < m_Count; ++i)
			if(Now - m_aRanges[i].m_Start < LucentStyle::Motion::TextAppear)
				return true;
		return false;
	}
};
#endif
