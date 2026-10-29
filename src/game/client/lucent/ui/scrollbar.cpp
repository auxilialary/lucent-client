#include "scrollbar.h"
#include "surfaces.h"
#include "controls.h"

namespace LucentUi
{
	void DrawSlider(IGraphics *pGraphics, const CSliderGeometry &Geometry, float Hover, bool Active, bool Hot, bool Enabled)
	{
		if(Geometry.m_Track.w <= 0)
			return;
		const float Alpha = Enabled ? 1.f : LucentStyle::Style::Disabled;
		Geometry.m_Track.Draw(Accent((LucentStyle::Style::BorderIdle + LucentStyle::Style::InputHover * Hover * .5f) * Alpha), IGraphics::CORNER_ALL, CSliderGeometry::Radius(Geometry.m_Track));
		if(Geometry.m_Fill.w > 0)
			GradientFill(Geometry.m_Fill, (LucentStyle::Style::FilledBorder + LucentStyle::Style::InputFocus + LucentStyle::Style::InputHover * Hover + LucentStyle::Style::SurfaceGlow * float(Active)) * Alpha,
				IGraphics::CORNER_ALL, CSliderGeometry::Radius(Geometry.m_Fill), Enabled);
		if(Geometry.m_Thumb.w > 0)
		{
			const float Emphasis = LucentStyle::Style::FilledBorder + LucentStyle::Style::InputFocus + LucentStyle::Style::InputHover * Hover +
				LucentStyle::Style::InputFocus * float(Active) + LucentStyle::Style::SurfaceGlow * float(Hot);
			// Opaque local paint still inherits CUIRect page fade. Emphasis changes brightness.
			const float Brightness = std::clamp(Emphasis * Alpha, 0.f, 1.f);
			const auto Color = Accent(1.f);
			const ColorRGBA Paint(Color.r * Brightness, Color.g * Brightness, Color.b * Brightness, CUIRect::DrawOpacity());
			const float Pixel = pGraphics->GetScreen().Height() / pGraphics->ScreenHeight();
			const auto Radii = CSliderGeometry::ThumbCoverageRadii(Geometry.m_Thumb, Pixel);
			const vec2 Center(Geometry.m_Thumb.x + Geometry.m_Thumb.w / 2, Geometry.m_Thumb.y + Geometry.m_Thumb.h / 2);
			// Reuse Lucent's opaque disk + edge coverage mesh. Clipping stays on
			// the graphics context; page opacity is applied once to every vertex.
			CheckboxMesh(pGraphics, Center, CheckboxBoundary(Center, Radii.x), CheckboxBoundary(Center, Radii.y),
				[&](vec2) { return Paint; });
		}
	}

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
