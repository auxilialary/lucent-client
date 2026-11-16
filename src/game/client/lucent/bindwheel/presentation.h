#ifndef GAME_CLIENT_LUCENT_BINDWHEEL_PRESENTATION_H
#define GAME_CLIENT_LUCENT_BINDWHEEL_PRESENTATION_H

#include "model.h"

#include <base/vmath.h>

#include <game/client/lucent/motion/transition.h>

namespace LucentBindWheel
{
	inline int PreviewHit(vec2 Cursor, float Radius, float Width, int Count)
	{
		if(Radius <= 0 || Width <= .98f)
			return -1;
		Cursor.x /= Width;
		if(length(Cursor) > Radius * (183.f / 190.f))
			return -1;
		return Select(Cursor, Count, Radius * (80.f / 190.f));
	}
	struct CFlipFrame
	{
		int m_Page;
		float m_Width;
	};
	struct CFlipPresentation
	{
		LucentStyle::Motion::CTransition m_Transition;
		CFlipFrame Update(int Page, double Now, bool Animate)
		{
			const float Phase = m_Transition.Update(Page == 1, Now, Animate, .18f);
			return {Animate ? int(Phase >= .5f) : Page, std::max(.025f, std::abs(std::cos(Phase * pi)))};
		}
	};
}
#endif
