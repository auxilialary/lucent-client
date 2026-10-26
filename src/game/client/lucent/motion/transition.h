#ifndef GAME_CLIENT_LUCENT_MOTION_TRANSITION_H
#define GAME_CLIENT_LUCENT_MOTION_TRANSITION_H

#include <game/client/lucent/style/tokens.h>

#include <algorithm>
#include <cmath>

namespace LucentStyle
{
	namespace Motion
	{
		inline float Lerp(float A, float B, float T) { return A + (B - A) * T; }
		inline float CubicOut(float T)
		{
			T = std::clamp(T, 0.f, 1.f);
			return 1 - (1 - T) * (1 - T) * (1 - T);
		}
		inline float QuadraticOut(float T)
		{
			T = std::clamp(T, 0.f, 1.f);
			return 1 - (1 - T) * (1 - T);
		}
		inline float Smooth(float T)
		{
			T = std::clamp(T, 0.f, 1.f);
			return T * T * (3 - 2 * T);
		}
		inline float Response(float Dt, float Rate) { return 1 - std::exp(-std::max(0.f, Dt) * Rate); }
		inline float Stagger(float T, float Delay, float Duration) { return CubicOut((T - Delay) / Duration); }
		inline float Slide(float Distance, float Progress) { return Distance * (1 - Progress); }
		// Absolute timestamps prevent accumulated FPS error. Retargeting starts at the visible value.
		struct CTransition
		{
			float m_Value = 0, m_From = 0;
			bool m_Target = false;
			double m_Start = 0;
			float Update(bool Target, double Now, bool Animate, float Duration = Hover)
			{
				if(Target != m_Target)
				{
					m_From = m_Value;
					m_Target = Target;
					m_Start = Now;
				}
				if(!Animate)
				{
					m_Value = m_From = float(Target);
					m_Start = Now;
				}
				else
					m_Value = Lerp(m_From, float(Target), CubicOut(float(std::max(0.0, Now - m_Start) / Duration)));
				return m_Value;
			}
		};
	}
}
#endif
