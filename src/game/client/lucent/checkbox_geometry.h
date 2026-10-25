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

	inline std::array<vec2, 68> CheckboxBoundary(vec2 Center, float Half, float Radius)
	{
		std::array<vec2, 68> aBoundary;
		for(int Corner = 0; Corner < 4; ++Corner)
		{
			const vec2 Origin((Corner == 0 || Corner == 3) ? Half - Radius : -Half + Radius, Corner < 2 ? Half - Radius : -Half + Radius);
			for(int Step = 0; Step <= 16; ++Step)
			{
				const float Angle = (Corner + Step / 16.f) * pi / 2;
				aBoundary[Corner * 17 + Step] = Center + Origin + vec2(std::cos(Angle), std::sin(Angle)) * Radius;
			}
		}
		return aBoundary;
	}
	inline std::array<vec2, 3> CheckboxMark(vec2 Center, float Size)
	{
		return {Center + vec2(-.25f, -.01f) * Size, Center + vec2(-.07f, .17f) * Size, Center + vec2(.27f, -.20f) * Size};
	}
	// A single rounded stroke outline avoids dark overlaps during mark fades.
	inline std::array<vec2, 52> CheckboxMarkBoundary(vec2 Center, float Size, float Radius)
	{
		const auto Points = CheckboxMark(Center, Size);
		const vec2 D0 = normalize(Points[1] - Points[0]), D1 = normalize(Points[2] - Points[1]);
		const vec2 N0(-D0.y, D0.x), N1(-D1.y, D1.x);
		const float A0 = std::atan2(N0.y, N0.x), A1 = std::atan2(N1.y, N1.x);
		std::array<vec2, 52> Boundary;
		for(int Step = 0; Step <= 16; ++Step)
		{
			const float T = Step / 16.f;
			const float Start = A0 + pi - pi * T;
			const float Join = A0 + (A1 - A0) * T;
			const float End = A1 - pi * T;
			Boundary[Step] = Points[0] + vec2(std::cos(Start), std::sin(Start)) * Radius;
			Boundary[17 + Step] = Points[1] + vec2(std::cos(Join), std::sin(Join)) * Radius;
			Boundary[34 + Step] = Points[2] + vec2(std::cos(End), std::sin(End)) * Radius;
		}
		Boundary[51] = Points[1] - (N0 + N1) * (Radius / (1 + dot(N0, N1)));
		return Boundary;
	}
}
#endif
