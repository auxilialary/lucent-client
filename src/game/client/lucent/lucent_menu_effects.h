#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_EFFECTS_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_EFFECTS_H

#include <base/vmath.h>
#include <game/client/ui_rect.h>

class IGraphics;

// Stateless geometry, fixed segment counts and no per-frame resource creation.
namespace CLucentMenuEffects
{
void Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, bool Accent);
void Connection(IGraphics *pGraphics, vec2 Center, float Scale, const CUIRect &Target, int Index, float Progress, float Highlight, double Time, float PulseAge);
}

#endif
