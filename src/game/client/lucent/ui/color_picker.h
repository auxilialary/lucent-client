#ifndef GAME_CLIENT_LUCENT_UI_COLOR_PICKER_H
#define GAME_CLIENT_LUCENT_UI_COLOR_PICKER_H
#include <base/color.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
class CUi;
class ITextRender;
namespace LucentUi
{
	struct CHueRingGeometry
	{
		vec2 m_Center;
		float m_Inner, m_Outer;
		static CHueRingGeometry Calculate(CUIRect Rect, float Pixel)
		{
			const float Radius = std::max(0.f, std::min(Rect.w, Rect.h) / 2 - std::max(2.f, 2 * Pixel));
			return {Rect.Center(), Radius * .76f, Radius};
		}
		vec2 Point(float Hue, float Radius) const
		{
			const float Angle = Hue * 2 * pi - pi / 2;
			return m_Center + vec2(std::cos(Angle), std::sin(Angle)) * Radius;
		}
		vec2 Indicator(float Hue) const { return Point(Hue, (m_Inner + m_Outer) / 2); }
		bool Contains(vec2 Position) const
		{
			const float Radius = distance(Position, m_Center);
			return m_Outer > 0 && Radius >= m_Inner && Radius <= m_Outer;
		}
		float HueAt(vec2 Position, float Previous) const
		{
			const vec2 Delta = Position - m_Center;
			if(length(Delta) < .001f)
				return Previous;
			float Hue = (std::atan2(Delta.y, Delta.x) + pi / 2) / (2 * pi);
			return Hue < 0 ? Hue + 1 : Hue;
		}
	};
	struct CColorPickerState
	{
		std::array<char, 4> m_Ids{};
		int m_Focus = 0;
		void Cancel(CUi *pUi);
		void MoveFocus(bool Alpha, bool Reverse)
		{
			const int Count = Alpha ? 4 : 3;
			m_Focus = (std::clamp(m_Focus, 0, Count - 1) + (Reverse ? Count - 1 : 1)) % Count;
		}
		static float Step(float Value, int Direction)
		{
			return std::clamp(Value + Direction / 255.f, 0.f, 1.f);
		}
	};
	bool ColorPicker(CUi *pUi, ITextRender *pTextRender, CUIRect View, unsigned *pColor, bool Alpha, float Darkest, CColorPickerState &State);
}
#endif
