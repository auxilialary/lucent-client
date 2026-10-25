#ifndef GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#define GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#include "lucent_style.h"

#include <base/vmath.h>

#include <array>

namespace LucentUi
{
	inline vec2 CheckboxCenter(vec2 RowOrigin, float RowHeight)
	{
		return RowOrigin + vec2(LucentStyle::Checkbox::LeftInset + LucentStyle::Checkbox::Size / 2, RowHeight / 2);
	}

	// Uniform angular spacing gives a smooth, consistently wound circle at all scales.
	inline std::array<vec2, 96> CheckboxBoundary(vec2 Center, float Radius)
	{
		std::array<vec2, 96> aBoundary;
		for(size_t I = 0; I < aBoundary.size(); ++I)
		{
			const float Angle = 2 * pi * I / aBoundary.size();
			aBoundary[I] = Center + vec2(std::cos(Angle), std::sin(Angle)) * Radius;
		}
		return aBoundary;
	}
}
#endif
