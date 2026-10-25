#ifndef GAME_CLIENT_LUCENT_BROWSER_AMBIENT_H
#define GAME_CLIENT_LUCENT_BROWSER_AMBIENT_H

#include <base/vmath.h>

#include <game/client/ui_rect.h>

class IGraphics;

// No textures, shaders, heap allocations or backend-specific facilities.
class CLucentBrowserAmbient
{
	vec2 m_Cursor = vec2(0, 0);
	double m_Time = 0;

public:
	void Update(float Dt, bool Animate, vec2 Cursor);
	void Render(IGraphics *pGraphics, const CUIRect &Screen, float Pulse, float Opacity) const;
};
#endif
