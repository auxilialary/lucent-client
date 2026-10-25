#ifndef GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#define GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#include "lucent_style.h"

#include <base/vmath.h>

#include <array>

namespace LucentUi
{
	inline std::array<vec2, 36> CheckboxBoundary(vec2 Center, float Half, float Radius)
	{
		std::array<vec2, 36> aBoundary;
		for(int Corner = 0; Corner < 4; ++Corner)
		{
			const vec2 Origin((Corner == 0 || Corner == 3) ? Half - Radius : -Half + Radius, Corner < 2 ? Half - Radius : -Half + Radius);
			for(int Step = 0; Step <= 8; ++Step)
			{
				const float Angle = (Corner + Step / 8.f) * pi / 2;
				aBoundary[Corner * 9 + Step] = Center + Origin + vec2(std::cos(Angle), std::sin(Angle)) * Radius;
			}
		}
		return aBoundary;
	}
	inline std::array<vec2, 3> CheckboxMark(vec2 Center, float Size)
	{
		return {Center + vec2(-.25f, -.01f) * Size, Center + vec2(-.07f, .17f) * Size, Center + vec2(.27f, -.20f) * Size};
	}
}
#endif
