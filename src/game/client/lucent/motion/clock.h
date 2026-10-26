#ifndef GAME_CLIENT_LUCENT_MOTION_CLOCK_H
#define GAME_CLIENT_LUCENT_MOTION_CLOCK_H

#include <algorithm>

namespace LucentStyle
{
	namespace Motion
	{
		struct CClock
		{
			double m_Now = 0, m_Time = 0, m_Last = -1;
			bool m_Animate = true;
			void BeginFrame(double Now, bool Animate)
			{
				if(m_Last >= 0 && Animate && m_Animate)
					m_Time += std::max(0.0, Now - m_Last);
				m_Last = m_Now = Now;
				m_Animate = Animate;
			}
			double GradientTime() const { return m_Animate ? m_Time : 0; }
		};
		CClock &Clock();
	}
}
#endif
