#ifndef GAME_CLIENT_LUCENT_STYLE_GRADIENT_H
#define GAME_CLIENT_LUCENT_STYLE_GRADIENT_H

#include <game/client/lucent/motion/transition.h>

namespace LucentStyle
{
	namespace Gradient
	{
		constexpr float ControlOrigin = -.25f, ControlSpan = .5f;
		inline const ColorRGBA White(1, 1, 1, 1);
		inline const ColorRGBA Rose(243.f / 255, 180.f / 255, 210.f / 255, 1);
		inline const ColorRGBA Pink(239.f / 255, 161.f / 255, 199.f / 255, 1);
		struct CParameters
		{
			double m_Period = 6;
			float m_Direction = 1, m_Intensity = 1;
		};
		inline ColorRGBA Blend(ColorRGBA A, ColorRGBA B, float T)
		{
			return ColorRGBA(Motion::Lerp(A.r, B.r, T), Motion::Lerp(A.g, B.g, T), Motion::Lerp(A.b, B.b, T), Motion::Lerp(A.a, B.a, T));
		}
		// Identical stops and traveling wave to the original Lucent wordmark.
		inline ColorRGBA Sample(float Position, double Time, CParameters Parameters = {})
		{
			const double Phase = std::fmod(Time, std::max(.001, Parameters.m_Period)) / std::max(.001, Parameters.m_Period);
			const float Wave = .5f + .5f * std::sin(6.28318530718f * (Position - float(Phase) * Parameters.m_Direction));
			const auto Color = Wave < .5f ? Blend(White, Rose, Wave * 2) : Blend(Rose, Pink, Wave * 2 - 1);
			return Blend(White, Color, std::clamp(Parameters.m_Intensity, 0.f, 1.f));
		}
		// One half-wave across each label: spatial color without brightness pulsing.
		inline ColorRGBA TextSample(float X, float Width, double Time, float Active, ColorRGBA Idle, CParameters Parameters = {})
		{
			const auto Color = Blend(Idle, Sample(ControlOrigin + ControlSpan * X / std::max(1.f, Width), Time, Parameters), Active);
			return Color.WithAlpha(Idle.a);
		}
	}
}
#endif
