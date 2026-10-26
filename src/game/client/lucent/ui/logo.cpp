#include <game/client/lucent/ui/logo.h>

#include <game/client/lucent/ui/scene_effects.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/style/gradient.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>

void CLucentLogo::Reset(ITextRender *pTextRender)
{
	for(auto &Text : m_aText)
		if(Text.Valid())
			pTextRender->DeleteTextContainer(Text);
	m_Scale = 0.0f;
}

void CLucentLogo::Render(IGraphics *pGraphics, ITextRender *pTextRender, float CenterX, float Top, float AvailableWidth, double Time, float Energy, vec2 OrbitOffset, const float *pHighlight, float Alpha, float OrbitAlpha)
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
	// Subtle diffuse wordmark light, rendered before the rear orbital streams.
	for(int i = 5; i > 0; --i)
	{
		const float Spread = i * 4.0f * Scale;
		CUIRect Glow = {CenterX - m_Width / 2 - Spread, Top + 8 * Scale - Spread, m_Width * 0.7f + 2 * Spread, 32 * Scale + 2 * Spread};
		Glow.Draw(ColorRGBA(1, 0.70f, 0.85f, (0.003f + Energy * 0.001f) * Alpha), IGraphics::CORNER_ALL, 24 * Scale);
	}
	const vec2 OrbitCenter = vec2(CenterX, Top + 29.0f * Scale) + OrbitOffset;
	CLucentMenuEffects::Orbit(pGraphics, OrbitCenter, Scale, Time, pHighlight, false, Energy, Alpha * OrbitAlpha);

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
		const ColorRGBA Color = LucentStyle::Gradient::Sample((i + 0.5f) / Bands, LucentStyle::Motion::Clock().GradientTime()).WithAlpha(Alpha);
		for(const auto &Text : m_aText)
			if(Text.Valid())
				pTextRender->RenderTextContainer(Text, Color, ColorRGBA(0, 0, 0, 0), Left, Top);
	}
	pGraphics->ClipDisable();
	CLucentMenuEffects::Orbit(pGraphics, OrbitCenter, Scale, Time, pHighlight, true, Energy, Alpha * OrbitAlpha);
	pGraphics->SetColor(1, 1, 1, 1);
}
