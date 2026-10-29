#ifndef GAME_CLIENT_LUCENT_UI_SCROLLBAR_H
#define GAME_CLIENT_LUCENT_UI_SCROLLBAR_H

#include "slider_geometry.h"

class CUIRect;
class IGraphics;

namespace LucentUi
{
	void DrawSlider(IGraphics *pGraphics, const CSliderGeometry &Geometry, float Hover, bool Active, bool Hot, bool Enabled);

	// Paint geometry only; the host retains the full-sized rail and drag hitbox.
	void DrawScrollbar(CUIRect Slider, bool Horizontal, bool Active, bool Hot);
}
#endif
