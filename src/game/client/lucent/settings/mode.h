#ifndef GAME_CLIENT_LUCENT_SETTINGS_MODE_H
#define GAME_CLIENT_LUCENT_SETTINGS_MODE_H
#include <game/client/lucent/motion/transition.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
namespace LucentSettings
{
	inline CUIRect SettingsContentRegion(CUIRect Column)
	{
		Column.VMargin(std::min(LucentStyle::Style::LabelGap, std::max(0.f, Column.w) / 2), &Column);
		return Column;
	}

	// Shared two-mode text navigation only. Measure must return unwrapped, pixel-aligned
	// text widths at the requested size; remeasure after fitting, never scale widths.
	struct CModeHeaderLayout
	{
		struct CMetrics
		{
			std::array<float, 2> m_Widths;
			float m_AlignedSize;
		};
		CUIRect m_Header, m_Content;
		std::array<CUIRect, 2> m_Modes, m_Underlines;
		float m_FontSize = 0, m_TextY = 0, m_Baseline = 0;
		template<typename FMeasure>
		static CModeHeaderLayout Calculate(CUIRect View, float Pixel, FMeasure Measure)
		{
			CModeHeaderLayout Result{};
			Pixel = std::max(.001f, Pixel);
			const float Width = std::max(0.f, View.w);
			float Gap = std::min(18.f, Width * .08f);
			float Size = 15;
			CMetrics Metrics{};
			while(Size >= Pixel)
			{
				Metrics = Measure(Size);
				const float Total = Metrics.m_Widths[0] + Metrics.m_Widths[1];
				if(Total + Gap <= Width)
					break;
				// Shrink spacing together with type on narrow or heavily scaled UIs.
				const float Ratio = Width / std::max(1.f, Total + Gap);
				Gap *= Ratio;
				Size = std::min(Size - Pixel, Size * Ratio);
			}
			if(Size < Pixel)
			{
				Size = 0;
				Metrics = {};
				Gap = 0;
			}
			Result.m_FontSize = Size;
			const float HeaderHeight = std::max(22.f, Metrics.m_AlignedSize + 7);
			View.HSplitTop(HeaderHeight, &Result.m_Header, &Result.m_Content);
			Result.m_Content.HSplitTop(std::min(8.f, std::max(0.f, Result.m_Content.h)), nullptr, &Result.m_Content);
			// One snapped origin + one aligned font size gives both texts exactly
			// the same baseline, independent of each translation's cap/descender mix.
			Result.m_TextY = std::round((Result.m_Header.y + 2) / Pixel) * Pixel;
			Result.m_Baseline = Result.m_TextY + Metrics.m_AlignedSize;
			CUIRect Remaining = Result.m_Header;
			Remaining.VSplitLeft(Metrics.m_Widths[0], &Result.m_Modes[0], &Remaining);
			Remaining.VSplitLeft(Gap, nullptr, &Remaining);
			Remaining.VSplitLeft(Metrics.m_Widths[1], &Result.m_Modes[1], nullptr);
			for(int i = 0; i < 2; ++i)
				Result.m_Underlines[i] = {Result.m_Modes[i].x, Result.m_Baseline + 2, Metrics.m_Widths[i], 1};
			return Result;
		}
	};

	struct CModePresentation
	{
		LucentStyle::Motion::CTransition m_ModeReveal;
		std::array<LucentStyle::Motion::CTransition, 2> m_HeaderHover, m_HeaderFocus, m_HeaderSelection;
		bool m_FocusedAdvanced = false;
		bool m_Advanced = false, m_ShownAdvanced = false, m_ModeFocused = false;
		double m_LastFrame = -1;
		void FocusMode()
		{
			m_ModeFocused = true;
			m_FocusedAdvanced = m_Advanced;
		}
		void ClearKeyboardFocus()
		{
			m_ModeFocused = false;
		}
		bool ContentInteractive() const { return m_ShownAdvanced == m_Advanced && m_ModeReveal.m_Value == 1.f; }
		bool SelectMode(bool Advanced)
		{
			if(m_Advanced == Advanced)
				return false;
			m_Advanced = Advanced;
			return true;
		}
		void ToggleMode() { SelectMode(!m_Advanced); }
		void Leave()
		{
			ClearKeyboardFocus();
			m_HeaderHover = {};
			m_HeaderFocus = {};
			m_HeaderSelection = {};
			m_LastFrame = -1;
		}
		bool UpdateMode(double Now, bool Animate)
		{
			const bool Fresh = m_LastFrame < 0 || Now - m_LastFrame > .5;
			if(Fresh)
			{
				m_ModeFocused = false;
				m_ShownAdvanced = m_Advanced;
				m_ModeReveal.Update(true, Now, false);
				m_HeaderHover = {};
				m_HeaderFocus = {};
				for(int i = 0; i < 2; ++i)
					m_HeaderSelection[i].Update(bool(i) == m_Advanced, Now, false);
			}
			m_ModeReveal.Update(m_ShownAdvanced == m_Advanced, Now, Animate, LucentStyle::Motion::Hover / 2);
			if(m_ModeReveal.m_Value == 0 && m_ShownAdvanced != m_Advanced)
			{
				m_ShownAdvanced = m_Advanced;
				m_ModeReveal.Update(true, Now, Animate, LucentStyle::Motion::Hover / 2);
			}
			m_LastFrame = Now;
			return Fresh;
		}
	};
}
#endif
