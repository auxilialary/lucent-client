#ifndef GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#define GAME_CLIENT_LUCENT_CHECKBOX_GEOMETRY_H
#include "lucent_style.h"

#include <base/vmath.h>

#include <array>

namespace LucentUi
{
	inline vec2 CheckboxCenter(vec2 RowOrigin, float Baseline, float CapHeight)
	{
		return RowOrigin + vec2(LucentStyle::Checkbox::LeftInset + LucentStyle::Checkbox::Size / 2, Baseline - CapHeight / 2);
	}

	struct CCheckboxRingCoverage
	{
		std::array<float, 4> m_Radii;
		float m_Peak;
	};
	inline CCheckboxRingCoverage CheckboxRingCoverage(float Pixel)
	{
		const float Outer = LucentStyle::Checkbox::Size / 2;
		const float Border = LucentStyle::Checkbox::Border;
		const float Feather = Pixel / 2;
		const float Middle = Outer - Border / 2;
		const float Plateau = std::max(0.f, Border - Pixel) / 2;
		return {{Outer - Border - Feather, Middle - Plateau, Middle + Plateau, Outer + Feather}, std::min(1.f, Border / Pixel)};
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
