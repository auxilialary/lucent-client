#ifndef GAME_CLIENT_LUCENT_RADIAL_TRANSITION_H
#define GAME_CLIENT_LUCENT_RADIAL_TRANSITION_H

#include <base/vmath.h>

#include <algorithm>
#include <cmath>

// Cursor-Origin Radial Transition. Coordinates use the same space as CUi.
// Absolute time prevents focus loss or a slow frame from stranding navigation.
class CRadialTransition
{
public:
	enum class EState
	{
		IDLE,
		ACTIVE,
		COMPLETED
	};
	static constexpr double DURATION = 0.88;
	struct CMask
	{
		vec2 m_Origin;
		float m_Radius;
		float m_Feather;
		float Coverage(vec2 Point) const
		{
			const float T = std::clamp((m_Radius - distance(Point, m_Origin)) / m_Feather, 0.0f, 1.0f);
			return T * T * (3 - 2 * T);
		}
	};

private:
	EState m_State = EState::IDLE;
	int m_FrameScreen = -3;
	vec2 m_Origin = vec2(0, 0);
	double m_Start = 0;
	float m_Progress = 0;

public:
	void Reset() { *this = CRadialTransition(); }
	void BeginFrame(int Screen) { m_FrameScreen = Screen; }
	bool CaptureNeeded(int Destination, bool Animate) const
	{
		return Animate && m_State == EState::IDLE && m_FrameScreen != -3 && Destination != -3 && Destination != m_FrameScreen;
	}
	bool Start(double Now, vec2 Origin, bool Animate)
	{
		if(m_State != EState::IDLE)
			return false;
		m_Origin = Origin;
		m_Start = Now;
		m_Progress = Animate ? 0 : 1;
		m_State = Animate ? EState::ACTIVE : EState::COMPLETED;
		return true;
	}
	void Update(double Now, bool Animate)
	{
		if(!Active())
			return;
		m_Progress = Animate ? std::max(m_Progress, static_cast<float>(std::clamp((Now - m_Start) / DURATION, 0.0, 1.0))) : 1;
		if(m_Progress >= 1)
			m_State = EState::COMPLETED;
	}
	bool Active() const { return m_State == EState::ACTIVE; }
	bool Completed() const { return m_State == EState::COMPLETED; }
	vec2 Origin() const { return m_Origin; }
	float Progress() const { return m_Progress; }
	float MaxRadius(vec2 TopLeft, vec2 Size) const
	{
		const vec2 Delta = m_Origin - TopLeft;
		return length(vec2(std::max(std::abs(Delta.x), std::abs(Size.x - Delta.x)), std::max(std::abs(Delta.y), std::abs(Size.y - Delta.y))));
	}
	float Radius(vec2 TopLeft, vec2 Size, float Feather) const
	{
		// Quadratic ease-out keeps the early expansion brisk without a cubic tail.
		const float Ease = 1 - (1 - m_Progress) * (1 - m_Progress);
		return Ease * (MaxRadius(TopLeft, Size) + Feather);
	}
	float Coverage(vec2 Point, vec2 TopLeft, vec2 Size, float Feather) const
	{
		if(Completed())
			return 1;
		if(!Active() || m_Progress <= 0)
			return 0;
		return Mask(TopLeft, Size, Feather).Coverage(Point);
	}
	CMask Mask(vec2 TopLeft, vec2 Size, float Feather) const { return {m_Origin, Radius(TopLeft, Size, Feather), Feather}; }
};

#endif
