#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_EFFECTS_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_EFFECTS_H

#include <base/vmath.h>

#include <game/client/ui_rect.h>

class IGraphics;

// Fixed geometry caches, bounded tessellation and no per-frame resource creation.
namespace CLucentMenuEffects
{
	void FocusStar(IGraphics *pGraphics, vec2 Center, float Scale, float Alpha);
	void Orbit(IGraphics *pGraphics, vec2 Center, float Scale, double Time, const float *pHighlight, bool Foreground, float Energy = 0, float Alpha = 1);
	void Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, float Highlight, double Time, int Glyph = -1, float ContentAlpha = 1, float ContentOffset = 0, bool ContentOnly = false);
	void Connection(IGraphics *pGraphics, vec2 Center, float Scale, const CUIRect &Target, int Index, float Progress, float Highlight, float PulseAge);
}

#endif
