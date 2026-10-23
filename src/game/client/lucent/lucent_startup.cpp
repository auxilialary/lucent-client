#include "lucent_startup.h"

#include <base/time.h>
#include <engine/graphics.h>

#include <algorithm>
#include <cmath>

namespace
{
	double Now() { return static_cast<double>(time_get()) / time_freq(); }
	float Smooth(double Value)
	{
		const float T = static_cast<float>(std::clamp(Value, 0.0, 1.0));
		return T * T * (3 - 2 * T);
	}

	void Capsule(IGraphics *pGraphics, float X, float Y, float Width, float Height, float Alpha, double Time, bool Fill)
	{
		if(Width <= 0 || Alpha <= 0)
			return;
		const float Radius = std::min(Height / 2, Width / 2);
		const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		const auto Extent = [&](float At) {
			const float D = std::max(0.0f, Radius - std::min(At, Width - At));
			return Height / 2 - Radius + std::sqrt(std::max(0.0f, Radius * Radius - D * D));
		};
		const auto Color = [&](float At) {
			const float T = At / Width;
			const float Shimmer = Fill ? std::pow(std::max(0.0f, std::cos((T - static_cast<float>(std::fmod(Time * 0.65, 2.0))) * 6.2831853f)), 12.0f) * 0.2f : 0;
			return Fill ? ColorRGBA(1, std::min(1.0f, 0.94f - T * 0.22f + Shimmer), std::min(1.0f, 0.98f - T * 0.10f + Shimmer), Alpha) : ColorRGBA(0.22f, 0.17f, 0.25f, Alpha * 0.45f);
		};
		constexpr int Steps = 160;
		for(int i = 0; i < Steps; ++i)
		{
			const float A = Width * i / Steps, B = Width * (i + 1) / Steps;
			const float EA = Extent(A), EB = Extent(B);
			const ColorRGBA CA = Color(A), CB = Color(B);
			pGraphics->SetColor4(CA, CB, CA, CB);
			IGraphics::CFreeformItem Body({X + A, Y - EA}, {X + B, Y - EB}, {X + A, Y + EA}, {X + B, Y + EB});
			pGraphics->QuadsDrawFreeform(&Body, 1);
			for(int Side : {-1, 1})
			{
				pGraphics->SetColor4(CA, CB, CA.WithAlpha(0), CB.WithAlpha(0));
				IGraphics::CFreeformItem Fringe({X + A, Y + Side * EA}, {X + B, Y + Side * EB}, {X + A, Y + Side * (EA + Pixel)}, {X + B, Y + Side * (EB + Pixel)});
				pGraphics->QuadsDrawFreeform(&Fringe, 1);
			}
		}
		pGraphics->QuadsEnd();
	}
}

void CLucentStartup::Clear(IGraphics *pGraphics)
{
	pGraphics->Clear(0.018f, 0.015f, 0.027f);
}

void CLucentStartup::Finish()
{
	if(m_Ready < 0)
		m_Ready = Now();
}

void CLucentStartup::Render(IGraphics *pGraphics, ITextRender *pTextRender, const CUIRect &Screen, std::optional<float> Progress)
{
	const double Time = Now();
	if(m_FirstFrame < 0)
		m_FirstFrame = Time;
	const double Age = Time - m_FirstFrame;
	const float Alpha = m_Ready < 0 ? 1 : 1 - Smooth((Time - m_Ready) / 0.3);
	if(Alpha <= 0)
		return;
	if(m_Ready < 0)
		m_Progress = Progress;
	Screen.Draw(ColorRGBA(0.018f, 0.015f, 0.027f, Alpha), 0, 0);
	const float CenterX = Screen.x + Screen.w / 2;
	const float CenterY = Screen.y + Screen.h * 0.43f;
	const float LogoWidth = std::min(400.0f, Screen.w * 0.38f);
	const float Scale = LogoWidth / 400;
	m_Logo.Render(pGraphics, pTextRender, CenterX, CenterY - 29 * Scale, LogoWidth, Age, 0, vec2(0, 0), nullptr, Alpha * Smooth(Age / 0.4), Smooth((Age - 0.10) / 0.4));
	// UI units follow the viewport mapping, just like the existing menu wordmark.
	const float Width = std::min(240.0f, Screen.w * 0.32f);
	const float Left = CenterX - Width / 2, Y = CenterY + 76 * Scale;
	const float BarAlpha = Alpha * Smooth((Age - 0.15) / 0.25);
	Capsule(pGraphics, Left, Y, Width, 3.5f, BarAlpha, Age, false);
	float FillLeft = Left;
	float FillWidth;
	if(m_Progress.has_value())
		FillWidth = Width * std::clamp(*m_Progress, 0.0f, 1.0f);
	else
	{
		FillWidth = Width * 0.24f;
		FillLeft += (Width - FillWidth) * (0.5f - 0.5f * std::cos(static_cast<float>(std::fmod(Age, 1.6) / 1.6) * 6.2831853f));
	}
	if(FillWidth > 0)
	{
		for(int i = 3; i > 0; --i)
		{
			CUIRect Glow = {FillLeft + FillWidth - 3 - i * 2.0f, Y - 1.75f - i, 6 + i * 4.0f, 3.5f + i * 2};
			Glow.Draw(ColorRGBA(1, 0.72f, 0.86f, BarAlpha * 0.025f), IGraphics::CORNER_ALL, Glow.h / 2);
		}
		Capsule(pGraphics, FillLeft, Y, FillWidth, 3.5f, BarAlpha, Age, true);
	}
	pGraphics->SetColor(1, 1, 1, 1);
}

void CLucentStartup::RenderTransition(IGraphics *pGraphics, ITextRender *pTextRender, const CUIRect &Screen)
{
	if(m_Ready >= 0 && Now() - m_Ready < 0.3)
		Render(pGraphics, pTextRender, Screen, m_Progress);
}

