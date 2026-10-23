#ifndef GAME_CLIENT_LUCENT_LUCENT_STARTUP_H
#define GAME_CLIENT_LUCENT_LUCENT_STARTUP_H

#include "lucent_logo.h"
#include <game/client/ui_rect.h>
#include <optional>

// Main-thread presentation only: never waits or gates initialization/input.
class CLucentStartup
{
	CLucentLogo m_Logo;
	double m_FirstFrame = -1;
	double m_Ready = -1;
	std::optional<float> m_Progress;

public:
	static void Clear(IGraphics *pGraphics);
	bool Loading() const { return m_Ready < 0; }
	void Finish();
	void ResetLogo(ITextRender *pTextRender) { m_Logo.Reset(pTextRender); }
	void Render(IGraphics *pGraphics, ITextRender *pTextRender, const CUIRect &Screen, std::optional<float> Progress);
	void RenderTransition(IGraphics *pGraphics, ITextRender *pTextRender, const CUIRect &Screen);
};
#endif
