#ifndef GAME_CLIENT_LUCENT_LUCENT_LOGO_H
#define GAME_CLIENT_LUCENT_LUCENT_LOGO_H
#include <engine/textrender.h>

class CLucentLogo
{
	STextContainerIndex m_aText[2];
	float m_Width = 0.0f;
	float m_Scale = 0.0f;

public:
	void Reset(ITextRender *pTextRender);
	void Render(IGraphics *pGraphics, ITextRender *pTextRender, float CenterX, float Top, float AvailableWidth, double Time, float Energy, vec2 OrbitOffset, const float *pHighlight = nullptr, float Alpha = 1, float OrbitAlpha = 1);
};
#endif
