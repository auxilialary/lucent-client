#ifndef GAME_CLIENT_LUCENT_SETTINGS_MODE_RENDER_H
#define GAME_CLIENT_LUCENT_SETTINGS_MODE_RENDER_H
#include "mode.h"

#include <engine/keys.h>

#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui.h>
#include <game/localization.h>
namespace LucentSettings
{
	template<typename TPresentation, typename TButtons>
	CModeHeaderLayout RenderModeHeader(CUi *pUi, ITextRender *pTextRender, CUIRect View, TPresentation &Presentation, TButtons &Buttons, bool Keyboard, const char *pFirstLabel = nullptr, const char *pSecondLabel = nullptr)
	{
		const auto &Clock = LucentStyle::Motion::Clock();
		const char *apModeLabels[] = {pFirstLabel ? pFirstLabel : Localize("Basic", "General settings"), pSecondLabel ? pSecondLabel : Localize("Advanced settings", "General settings")};
		const auto MeasureHeading = [&](float Size) {
			CModeHeaderLayout::CMetrics Metrics{};
			STextSizeProperties Props{};
			Props.m_pAlignedFontSize = &Metrics.m_AlignedSize;
			for(int i = 0; i < 2; ++i)
				Metrics.m_Widths[i] = pTextRender->TextWidth(Size, apModeLabels[i], -1, -1, 0, Props);
			return Metrics;
		};
		const auto Header = CModeHeaderLayout::Calculate(View, pUi->PixelSize(), MeasureHeading);
		const auto &Modes = Header.m_Modes;
		const bool ActivateMode = Keyboard && Presentation.m_ModeFocused &&
					  (pUi->ConsumeHotkey(CUi::HOTKEY_ENTER) || pUi->Input()->KeyPress(KEY_SPACE));
		for(int i = 0; i < 2; ++i)
		{
			if(pUi->DoButtonLogic(&Buttons[i], 0, &Modes[i], BUTTONFLAG_LEFT) ||
				(ActivateMode && Presentation.m_FocusedAdvanced == bool(i)))
			{
				Presentation.SelectMode(bool(i));
			}
		}
		for(int i = 0; i < 2; ++i)
		{
			const CUIRect &Mode = Modes[i];
			const bool Selected = Presentation.m_Advanced == bool(i);
			const float Hot = Presentation.m_HeaderHover[i].Update(pUi->Enabled() && pUi->MouseHovered(&Mode), Clock.m_Now, Clock.m_Animate);
			const float ModeFocus = Presentation.m_HeaderFocus[i].Update(pUi->Enabled() && Presentation.m_ModeFocused && Presentation.m_FocusedAdvanced == bool(i), Clock.m_Now, Clock.m_Animate, LucentStyle::Motion::Focus);
			const float Underline = Presentation.m_HeaderSelection[i].Update(Selected, Clock.m_Now, Clock.m_Animate, LucentStyle::Motion::Selection);
			if(ModeFocus > 0)
				Mode.DrawOutline(LucentUi::Accent(LucentStyle::Style::InputFocus * ModeFocus));
			const auto Color = Selected ? LucentUi::Accent(1.f) : LucentStyle::Gradient::Blend(LucentStyle::Style::NavigationText, LucentStyle::Style::NavigationHover, Hot);
			if(Header.m_FontSize > 0)
			{
				const auto Previous = pTextRender->GetTextColor();
				pTextRender->TextColor(Color);
				CTextCursor Cursor;
				Cursor.SetPosition(vec2(Mode.x, Header.m_TextY));
				Cursor.m_FontSize = Header.m_FontSize;
				Cursor.m_LineWidth = -1; // fitted and measured as a single line
				pTextRender->TextEx(&Cursor, apModeLabels[i], -1);
				pTextRender->TextColor(Previous);
			}
			const CUIRect &Line = Header.m_Underlines[i];
			if(Underline > 0)
				LucentUi::GradientFill(Line, Underline * LucentStyle::Style::ButtonUnderline, 0, 0);
		}
		return Header;
	}
}
#endif
