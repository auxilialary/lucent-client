#ifndef GAME_CLIENT_LUCENT_UI_SLIDER_GEOMETRY_H
#define GAME_CLIENT_LUCENT_UI_SLIDER_GEOMETRY_H

#include <game/client/lucent/style/tokens.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>

namespace LucentUi
{
	// Paint only: the host's value mapping, drag rail and hitboxes stay unchanged.
	struct CSliderGeometry
	{
		CUIRect m_Track, m_Fill, m_Thumb;
		static float Radius(const CUIRect &Rect)
		{
			return std::max(0.f, std::min(Rect.w, Rect.h) / 2);
		}
		// Coverage is centered on the nominal circle, not an enlarged knob.
		// Only this one-framebuffer-pixel band is translucent.
		static vec2 ThumbCoverageRadii(const CUIRect &Thumb, float Pixel)
		{
			const float R = Radius(Thumb);
			const float HalfPixel = std::min(std::max(.001f, Pixel) / 2, R * .9f);
			return vec2(R - HalfPixel, R + HalfPixel);
		}
		static CSliderGeometry Calculate(const CUIRect &Rail, const CUIRect &Handle, float Value, float Pixel, bool ContentAnchored = false)
		{
			Pixel = std::max(.001f, Pixel);
			const float TrackPixels = std::max(1.f, std::round(LucentStyle::Style::Radius / Pixel));
			const float Thickness = TrackPixels * Pixel;
			// Match the host's existing handle-center travel, without changing its
			// wide drag handle or hitboxes. The visible thumb is a small round knob.
			const float Inset = std::clamp(Handle.w, 0.f, std::max(0.f, Rail.w)) / 2;
			const float X = ContentAnchored ? Rail.x : std::round((Rail.x + Inset) / Pixel) * Pixel;
			const float End = ContentAnchored ? Rail.x + std::max(0.f, Rail.w) : std::round((Rail.x + std::max(0.f, Rail.w) - Inset) / Pixel) * Pixel;
			const float Y = std::round((Rail.y + Rail.h / 2 - Thickness / 2) / Pixel) * Pixel;
			CSliderGeometry Result;
			Result.m_Track = {X, Y, std::max(0.f, End - X), Thickness};
			Result.m_Fill = Result.m_Track;
			Result.m_Fill.w *= std::clamp(Value, 0.f, 1.f);
			const float ThumbPixels = std::max(TrackPixels + 2,
				std::round((LucentStyle::Style::ControlSize - 2 * LucentStyle::Style::Radius) / Pixel));
			// Keep the circular knob on the track's exact axis, including odd
			// pixel track heights; do not enlarge it just to match edge parity.
			const float Diameter = ThumbPixels * Pixel;
			const float KnobRadius = std::min(Diameter / 2, Result.m_Track.w / 2);
			const float CenterX = ContentAnchored ?
						      std::clamp(X + Result.m_Fill.w, X + KnobRadius, End - KnobRadius) :
						      std::round((X + Result.m_Fill.w) / Pixel) * Pixel;
			const float VisibleDiameter = ContentAnchored ? std::min(Diameter, Result.m_Track.w) : Diameter;
			Result.m_Thumb = {CenterX - VisibleDiameter / 2, Y + (Thickness - VisibleDiameter) / 2, VisibleDiameter, VisibleDiameter};
			return Result;
		}
		static CUIRect ValueLabel(const CUIRect &Group, const CUIRect &Slider, float Pixel)
		{
			const CUIRect Rail = {Slider.x, Slider.y + 5.f, Slider.w, Slider.h - 10.f};
			// Same handle width as DoScrollbarH; no changes to its interaction rail.
			const float HandleWidth = std::min(std::max(33.f, Rail.h), std::max(0.f, Rail.w / 3));
			const CUIRect Handle = {Rail.x, Rail.y, HandleWidth, Rail.h};
			const auto Geometry = Calculate(Rail, Handle, 0, Pixel);
			const float X = std::clamp(Geometry.m_Track.x + Geometry.m_Track.w + 10.f, Group.x, Group.x + Group.w);
			return {X, Group.y, std::max(0.f, Group.x + Group.w - X), Group.h};
		}
	};
}
#endif
