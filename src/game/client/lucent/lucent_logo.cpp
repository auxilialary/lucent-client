#include "lucent_logo.h"

#include <base/time.h>

#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TAU = 6.28318530718f;

	ColorRGBA Gradient(float Position, double Time)
	{
		const float Wave = 0.5f + 0.5f * std::sin(TAU * (Position - static_cast<float>(Time / 6.0)));
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		const ColorRGBA Rose(243.0f / 255.0f, 180.0f / 255.0f, 210.0f / 255.0f, 1.0f);
		const ColorRGBA Pink(239.0f / 255.0f, 161.0f / 255.0f, 199.0f / 255.0f, 1.0f);
		const ColorRGBA A = Wave < 0.5f ? White : Rose;
		const ColorRGBA B = Wave < 0.5f ? Rose : Pink;
		const float Blend = Wave < 0.5f ? Wave * 2.0f : Wave * 2.0f - 1.0f;
		return ColorRGBA(mix(A.r, B.r, Blend), mix(A.g, B.g, Blend), mix(A.b, B.b, Blend), 1.0f);
	}

	void Orbit(IGraphics *pGraphics, vec2 Center, float Scale, double Time)
	{
		constexpr int Segments = 192;
		const auto Point = [=](float Angle) {
			const float X = 174.0f * Scale * std::cos(Angle);
			const float Y = 37.0f * Scale * std::sin(Angle);
			return Center + vec2(X * 0.989f + Y * 0.148f, Y * 0.989f - X * 0.148f);
		};
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(int i = 0; i < Segments; ++i)
		{
			const float A = TAU * i / Segments;
			const float B = TAU * (i + 1) / Segments;
			const vec2 P = Point(A);
			const vec2 Q = Point(B);
			const vec2 Normal = normalize(vec2(-(Q - P).y, (Q - P).x));
			const float Accent = std::pow(0.5f + 0.5f * std::cos(A - TAU * static_cast<float>(Time / 10.0)), 20.0f);
			// Faint broad halo and a feathered core, always behind the wordmark.
			for(int Layer = 0; Layer < 2; ++Layer)
			{
				const float Radius = (Layer == 0 ? 2.5f : 0.65f) * Scale;
				const float Alpha = (Layer == 0 ? 0.045f : 0.22f) + Accent * (Layer == 0 ? 0.08f : 0.38f);
				const ColorRGBA Color(1.0f, 0.79f, 0.89f, Alpha);
				const ColorRGBA Clear(1.0f, 0.79f, 0.89f, 0.0f);
				for(int Side : {-1, 1})
				{
					pGraphics->SetColor4(Color, Color, Clear, Clear);
					IGraphics::CFreeformItem Quad(P, Q, P + Normal * (Radius * Side), Q + Normal * (Radius * Side));
					pGraphics->QuadsDrawFreeform(&Quad, 1);
				}
			}
		}
		pGraphics->QuadsEnd();
	}
}

void CLucentLogo::Reset(ITextRender *pTextRender)
{
	for(auto &Text : m_aText)
		if(Text.Valid())
			pTextRender->DeleteTextContainer(Text);
	m_Scale = 0.0f;
}

void CLucentLogo::Render(IGraphics *pGraphics, ITextRender *pTextRender, float CenterX, float Top, float AvailableWidth)
{
	const float Scale = std::min(1.0f, AvailableWidth / 400.0f);
	if(m_Scale != Scale)
	{
		Reset(pTextRender);
		m_Scale = Scale;
		const unsigned Flags = pTextRender->GetRenderFlags();
		const ColorRGBA Color = pTextRender->GetTextColor();
		pTextRender->SetRenderFlags(TEXT_RENDER_FLAG_KERNING | TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);
		pTextRender->TextColor(1, 1, 1, 1);
		const float MainWidth = pTextRender->TextWidth(48.0f * Scale, "Lucent");
		const float SubWidth = pTextRender->TextWidth(22.0f * Scale, "Client");
		m_Width = MainWidth + 12.0f * Scale + SubWidth;
		for(int i = 0; i < 2; ++i)
		{
			CTextCursor Cursor;
			Cursor.m_FontSize = (i == 0 ? 48.0f : 22.0f) * Scale;
			Cursor.SetPosition(vec2(i == 0 ? 0.0f : MainWidth + 12.0f * Scale, i == 0 ? 0.0f : 22.0f * Scale));
			pTextRender->CreateTextContainer(m_aText[i], &Cursor, i == 0 ? "Lucent" : "Client");
		}
		pTextRender->TextColor(Color);
		pTextRender->SetRenderFlags(Flags);
	}
	// time_get is monotonic. No accumulated frame deltas or work outside this view.
	const bool Animate = g_Config.m_ClLucentLogoAnimation && (g_Config.m_GfxRefreshRate == 0 || g_Config.m_GfxRefreshRate >= 30);
	const double Time = Animate ? static_cast<double>(time_get()) / time_freq() : 0.0;
	Orbit(pGraphics, vec2(CenterX, Top + 29.0f * Scale), Scale, Time);

	const CScreenRect Screen = pGraphics->GetScreen();
	const vec2 Pixels = pGraphics->ScreenSize() / Screen.Size();
	const float Left = CenterX - m_Width / 2.0f;
	const int First = static_cast<int>(std::floor((Left - Screen.m_TopLeft.x) * Pixels.x));
	const int Last = static_cast<int>(std::ceil((Left + m_Width - Screen.m_TopLeft.x) * Pixels.x));
	// Adjacent integer scissor bands cover each physical pixel exactly once.
	// Cached text buffers are reused; only uniform colors and scissors change.
	constexpr int Bands = 96;
	for(int i = 0; i < Bands; ++i)
	{
		const int X = First + (Last - First) * i / Bands;
		const int End = First + (Last - First) * (i + 1) / Bands;
		if(End <= X)
			continue;
		pGraphics->ClipEnable(X, 0, End - X, pGraphics->ScreenHeight());
		const ColorRGBA Color = Gradient((i + 0.5f) / Bands, Time);
		for(const auto &Text : m_aText)
			if(Text.Valid())
				pTextRender->RenderTextContainer(Text, Color, ColorRGBA(0, 0, 0, 0), Left, Top);
	}
	pGraphics->ClipDisable();
	pGraphics->SetColor(1, 1, 1, 1);
}
