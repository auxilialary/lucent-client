#include "scrollbar.h"
#include "surfaces.h"

namespace LucentUi
{
	void DrawScrollbar(CUIRect Slider, bool Horizontal, bool Active, bool Hot)
	{
		if(Horizontal)
			Slider.HMargin(std::max(0.f, (Slider.h - 2) * .5f), &Slider);
		else
			Slider.VMargin(std::max(0.f, (Slider.w - 2) * .5f), &Slider);
		const float Rounding = Horizontal ? Slider.h / 2.0f : Slider.w / 2.0f;
		Slider.Draw(Accent(Active ? .65f : Hot ? .48f : .24f), IGraphics::CORNER_ALL, Rounding);
	}
}
