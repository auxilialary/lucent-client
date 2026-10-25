#ifndef GAME_CLIENT_LUCENT_LUCENT_STYLE_H
#define GAME_CLIENT_LUCENT_LUCENT_STYLE_H

#include <base/color.h>

#include <algorithm>
#include <cmath>

// The visual vocabulary for Lucent scenes and controls. No renderer or config dependency.
namespace LucentStyle
{
	namespace Style
	{
		inline const ColorRGBA Accent(1, .82f, .91f, 1);
		inline const ColorRGBA Surface(.025f, .021f, .035f, .98f);
		inline const ColorRGBA MenuBackground(.018f, .015f, .027f, 1);
		inline const ColorRGBA BrowserBackground(.010f, .009f, .014f, 1);
		inline const ColorRGBA Text(.88f, .87f, .92f, 1);
		inline const ColorRGBA PrimaryText(.96f, .85f, .92f, 1);
		inline const ColorRGBA MutedText(.64f, .60f, .67f, 1);
		inline const ColorRGBA NavigationText(.66f, .63f, .69f, 1);
		inline const ColorRGBA NavigationHover(.91f, .83f, .88f, 1);
		inline const ColorRGBA MenuHover(1, .82f, .9f, 1);
		constexpr float Radius = 3, Hairline = .5f, ControlSize = 14, LabelGap = 7;
		constexpr float BorderIdle = .12f, BorderActive = .40f, FilledBorder = .42f, FocusBorder = .62f;
		inline const ColorRGBA BrowserHint(.77f, .71f, .76f, 1);
		inline const ColorRGBA SecondaryText(.63f, .62f, .69f, 1);
		inline const ColorRGBA DetailText(.72f, .68f, .74f, 1);
		inline const ColorRGBA UnavailableText(.64f, .58f, .64f, 1);
		inline const ColorRGBA ServerText(.86f, .84f, .88f, 1);
		inline const ColorRGBA ServerHover(.98f, .96f, .98f, 1);
		inline const ColorRGBA MetadataText(.58f, .57f, .64f, 1);
		inline const ColorRGBA MetadataHover(.72f, .69f, .76f, 1);
		inline const ColorRGBA UtilitySurface(0, 0, 0, 1);
		inline const ColorRGBA ErrorText(1, .4f, .4f, 1);
		inline const ColorRGBA FavoriteText(.95f, .79f, .87f, 1);
		constexpr float InputHover = .08f, InputFocus = .12f, SurfaceGlow = .07f, TextHover = .25f;
		constexpr float CheckAlpha = .85f, CheckInset = 4, CheckRadius = 1, MixedHeight = 1.5f;
		constexpr float ButtonUnderline = .7f, PressedUnderline = .95f, PressedText = .8f;
		constexpr float SceneBase = .25f, SceneHover = .75f, ScenePressed = .10f, SceneIdle = .025f;
		constexpr float Disabled = .35f, Selected = .13f, Hover = .045f, Pressed = .07f;
	}
	namespace Checkbox
	{
		constexpr float Size = 14, Border = 1.1f, DotSize = 7, LeftInset = 5;
		constexpr float LabelOffset = LeftInset + Size + Style::LabelGap;
		constexpr float TextIdle = .80f, TextHover = .95f, TextChecked = 1.f;
		constexpr float FillDuration = .20f;
		constexpr float RingIntensity = .85f, GradientTilt = .2f;
		constexpr double Period = 5;
		inline const ColorRGBA BorderIdle(.22f, .20f, .25f, 1);
		inline const ColorRGBA BorderHover(.48f, .35f, .43f, 1);
	}

	namespace Motion
	{
		constexpr float Hover = .18f, Focus = .18f, Selection = .22f, Branch = .28f;
		constexpr double Radial = .88;
		constexpr float MenuResponse = 12;
		constexpr float SecondaryReveal = .32f, SecondaryStagger = .04f, SecondaryItem = .16f;
		constexpr float MenuReveal = .35f, MenuDelay = .065f, MenuStagger = .02f, MenuItem = .245f;
		constexpr float MapTitle = .42f;
		constexpr float TextAppear = .12f, TextTrail = .22f, TextTrailIntensity = .28f;
		constexpr float TextTrailGlyphs = 1.5f, TextSpeedLow = 25.f, TextSpeedHigh = 240.f;
		constexpr float TextSpeedResponse = .055f, TextIdleInterval = .3f, TextMinEventInterval = .001f;
		constexpr float TextTrailIdleIntensity = .4f;
		constexpr float TextHeadWidth = .065f, TextHeadMinWidth = .7f;
		constexpr float TextJumpLimit = 3.f;
		constexpr float FilterInput = .27f, Filters = .48f, FilterRise = 12.f;
		constexpr float FilterNavTravel = 60, FilterFooterTravel = 50;
		constexpr float FilterQuickDelay = .08f, FilterBodyDelay = .16f;
		constexpr float Primary = .30f, Scene = .36f, Overlay = .32f, Expansion = .38f, Players = .40f;
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
