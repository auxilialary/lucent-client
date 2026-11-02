#ifndef GAME_CLIENT_LUCENT_SETTINGS_NAVIGATION_H
#define GAME_CLIENT_LUCENT_SETTINGS_NAVIGATION_H

#include "layout.h"

#include <game/client/lucent/motion/transition.h>

namespace LucentSettings
{
	// Transient presentation state only. Selected pages remain owned by saved configuration.
	struct CNavigation
	{
		LucentStyle::Motion::CTransition m_Expansion;
		float m_Scroll = 0;
		int m_Focus = -1;

		float UpdateExpansion(bool Target, double Now, bool Animate)
		{
			return m_Expansion.Update(Target, Now, Animate, LucentStyle::Motion::Hover);
		}
		void Reset() { *this = CNavigation(); }
		void Focus(int Category, int Count = CategoryCount) { m_Focus = std::clamp(Category, 0, Count - 1); }
		void Move(int Direction, int Count = CategoryCount) { Focus((m_Focus + Direction + Count) % Count, Count); }
		void Scroll(float Distance, const CLayout &Layout) { m_Scroll = std::clamp(m_Scroll + Distance, 0.f, Layout.MaxScroll()); }
		void RevealFocus(const CLayout &Layout)
		{
			if(m_Focus < 0)
				return;
			const float Top = m_Focus * RowHeight;
			if(Top < m_Scroll)
				m_Scroll = Top;
			else if(Top + RowHeight > m_Scroll + Layout.m_Viewport.h)
				m_Scroll = Top + RowHeight - Layout.m_Viewport.h;
			Scroll(0, Layout);
		}
	};
}
#endif
