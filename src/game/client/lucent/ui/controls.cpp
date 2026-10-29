#include "controls.h"

#include "scopes.h"

namespace LucentUi
{
	float Highlight(const void *pId, bool Target, EChannel Channel, float Duration)
	{
		struct CState
		{
			const void *m_pId = nullptr;
			EChannel m_Channel = EChannel::HOVER;
			LucentStyle::Motion::CTransition m_Transition;
			double m_Last = -1;
		};
		static std::array<CState, 768> s_aStates;
		const auto &Clock = LucentStyle::Motion::Clock();
		auto *pState = &s_aStates[0];
		for(auto &State : s_aStates)
		{
			if(State.m_pId == pId && State.m_Channel == Channel)
			{
				pState = &State;
				break;
			}
			if(State.m_Last < pState->m_Last)
				pState = &State;
		}
		if(pState->m_pId != pId || pState->m_Channel != Channel)
		{
			*pState = {};
			pState->m_pId = pId;
			pState->m_Channel = Channel;
		}
		pState->m_Last = Clock.m_Now;
		return pState->m_Transition.Update(Target, Clock.m_Now, Clock.m_Animate,
			Duration > 0 ? Duration : Channel == EChannel::FOCUS ? LucentStyle::Motion::Focus :
									       LucentStyle::Motion::Hover);
	}

	bool FilterTab(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, const CUIRect &Rect, bool Selected)
	{
		const float Hot = Highlight(pId, pUi->Enabled() && pUi->MouseHovered(&Rect));
		const float Active = Highlight(pId, Selected, EChannel::ACTIVE, LucentStyle::Motion::Selection);
		CUIRect Line = {Rect.x, Rect.y + Rect.h - 1, Rect.w, LucentStyle::Style::Hairline};
		Divider(Line, Active * LucentStyle::Style::ButtonUnderline, false, true);
		pTextRender->TextColor((Selected ? LucentStyle::Gradient::White : LucentStyle::Style::Text).WithAlpha(Selected ? 1.f : LucentStyle::Motion::Lerp(.58f, .85f, Hot)));
		const float Size = std::min(11.f, 11.f * std::max(1.f, Rect.w - 6) / std::max(1.f, pTextRender->TextWidth(11, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}

	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags)
	{
		return CheckBox(pUi, pId, pText, pMark, Rect, Flags, false);
	}

	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags, bool ControlRight)
	{
		return CheckBox(pUi, pId, pText, pMark, Rect, Flags, ControlRight, pUi->Enabled());
	}

	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags, bool ControlRight, bool VisualEnabled)
	{
		CUIRect Box, Label;
		if(ControlRight)
			Rect.VSplitRight(LucentStyle::Style::ControlSize, &Label, &Box);
		else
			Rect.VSplitLeft(LucentStyle::Style::ControlSize, &Box, &Label);
		Box.HMargin((Box.h - LucentStyle::Style::ControlSize) / 2, &Box);
		if(ControlRight)
			Label.VSplitRight(LucentStyle::Style::LabelGap, &Label, nullptr);
		else
			Label.VSplitLeft(LucentStyle::Style::LabelGap, nullptr, &Label);
		const float Light = VisualEnabled ? Highlight(pId, *pMark || (pUi->Enabled() && pUi->MouseHovered(&Rect))) : 0.f;
		Surface(Box, false, false, IGraphics::CORNER_ALL, Light);
		const float Active = VisualEnabled ? Highlight(pId, *pMark != 0, EChannel::ACTIVE) : float(*pMark != 0);
		if(Active > .001f)
		{
			Box.Margin(LucentStyle::Style::CheckInset, &Box);
			// The mixed-state bar retains its shape while sharing the active gradient.
			if(*pMark == 'O')
				Box.HMargin((Box.h - LucentStyle::Style::MixedHeight) / 2, &Box);
			GradientFill(Box, Active * (VisualEnabled ? LucentStyle::Style::CheckAlpha : LucentStyle::Style::Disabled), IGraphics::CORNER_ALL, LucentStyle::Style::CheckRadius, VisualEnabled);
		}
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = true;
		Props.m_EnableWidthCheck = false;
		CTextScope Text(pUi->TextRender(), std::min(Label.w, pUi->TextRender()->TextWidth(11, pText)), Active, Light, VisualEnabled);
		pUi->DoLabel(&Label, pText, 11, TEXTALIGN_ML, Props);
		return pUi->DoButtonLogic(pId, 0, &Rect, Flags);
	}

	int FilledCheckBox(CUi *pUi, const void *pId, const char *pText, bool Checked, const CUIRect &Rect)
	{
		return FilledCheckBox(pUi, pId, pText, Checked, Rect, pUi->Enabled(), false);
	}

	int FilledCheckBox(CUi *pUi, const void *pId, const char *pText, bool Checked, const CUIRect &Rect, bool VisualEnabled, bool WrapLabel, float IdleLabelBrightness)
	{
		struct CState
		{
			const void *m_pId = nullptr;
			double m_Last = -1;
			LucentStyle::Motion::CTransition m_Fill, m_Hover;
		};
		static std::array<CState, 128> s_aStates;
		const auto &Clock = LucentStyle::Motion::Clock();
		auto *pState = &s_aStates[0];
		for(auto &State : s_aStates)
		{
			if(State.m_pId == pId)
			{
				pState = &State;
				break;
			}
			if(State.m_Last < pState->m_Last)
				pState = &State;
		}
		if(pState->m_pId != pId)
		{
			*pState = {};
			pState->m_pId = pId;
			pState->m_Fill.Update(Checked, Clock.m_Now, false);
		}
		pState->m_Last = Clock.m_Now;
		const bool Enabled = VisualEnabled;
		const float Fill = pState->m_Fill.Update(Checked, Clock.m_Now, Clock.m_Animate, LucentStyle::Checkbox::FillDuration);
		const float Hot = pState->m_Hover.Update(pUi->Enabled() && pUi->MouseHovered(&Rect), Clock.m_Now, Clock.m_Animate);
		const float Alpha = Enabled ? 1.f : LucentStyle::Style::Disabled;
		CUIRect Label = Rect;
		const float LeftInset = WrapLabel ? 0.f : LucentStyle::Checkbox::LeftInset;
		Label.VSplitLeft(LeftInset + LucentStyle::Checkbox::Size + LucentStyle::Style::LabelGap, nullptr, &Label);
		if(WrapLabel)
			Label.y += 4;
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = !WrapLabel;
		Props.m_EnableWidthCheck = false;
		// Match DoLabel's single-line cursor placement and TextEx's pixel-aligned
		// baseline. Cap height supplies a stable optical center across translations.
		float TextHeight = 0, MaxHeight = 0, CapHeight = 0, AlignedSize = 0;
		STextSizeProperties Metrics{};
		Metrics.m_pHeight = &TextHeight;
		Metrics.m_pMaxCharacterHeightInLine = &MaxHeight;
		Metrics.m_pAlignedFontSize = &AlignedSize;
		const float TextWidth = pUi->TextRender()->TextWidth(11, pText, -1, Label.w, TEXTFLAG_STOP_AT_END, Metrics);
		STextSizeProperties CapMetrics{};
		CapMetrics.m_pMaxCharacterHeightInLine = &CapHeight;
		pUi->TextRender()->TextWidth(11, "H", -1, -1, 0, CapMetrics);
		auto *pGraphics = pUi->Graphics();
		const float Pixel = pGraphics->GetScreen().Height() / pGraphics->ScreenHeight();
		const vec2 Cursor = CUi::CalcAlignedCursorPos(&Label, vec2(TextWidth, TextHeight), WrapLabel ? TEXTALIGN_TL : TEXTALIGN_ML, &MaxHeight);
		const float Baseline = round_to_int(Cursor.y / Pixel) * Pixel + AlignedSize;
		const vec2 Center = CheckboxCenter(vec2(Rect.x + LeftInset - LucentStyle::Checkbox::LeftInset, Rect.y), Baseline - Rect.y, CapHeight);
		const auto Coverage = CheckboxRingCoverage(Pixel);
		const float Opacity = Alpha * CUIRect::DrawOpacity();
		const float Radius = LucentStyle::Checkbox::Size / 2;
		// Both shapes sample one fixed spatial field and the shared clock. Neither
		// hover nor the dot's changing radius can reset or shift the gradient phase.
		const auto Gradient = [&](vec2 Point) {
			return LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin + LucentStyle::Gradient::ControlSpan *
													    ((Point.x - Center.x) / (2 * Radius) + .5f + LucentStyle::Checkbox::GradientTilt * (Point.y - Center.y) / (2 * Radius)),
				Clock.GradientTime(), {LucentStyle::Checkbox::Period});
		};
		const auto Idle = LucentStyle::Gradient::Blend(LucentStyle::Checkbox::BorderIdle, LucentStyle::Checkbox::BorderHover, Hot);
		const auto RingColor = [&](vec2 Point) {
			return LucentStyle::Gradient::Blend(Idle, Gradient(Point), Fill)
				.WithAlpha(Opacity * LucentStyle::Motion::Lerp(1, LucentStyle::Checkbox::RingIntensity, Fill));
		};
		// A true annulus keeps the hole transparent; coverage fades on both edges.
		const std::array Contours = {
			CheckboxBoundary(Center, Coverage.m_Radii[0]), CheckboxBoundary(Center, Coverage.m_Radii[1]),
			CheckboxBoundary(Center, Coverage.m_Radii[2]), CheckboxBoundary(Center, Coverage.m_Radii[3])};
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(size_t Band = 0; Band < 3; ++Band)
			for(size_t I = 0; I < Contours[0].size(); ++I)
			{
				const size_t J = (I + 1) % Contours[0].size();
				const auto A = Contours[Band][I], B = Contours[Band + 1][I];
				const auto C = Contours[Band][J], D = Contours[Band + 1][J];
				const auto InnerColor = [&](vec2 P) { return RingColor(P).WithAlpha(Band == 0 ? 0.f : RingColor(P).a * Coverage.m_Peak); };
				const auto OuterColor = [&](vec2 P) { return RingColor(P).WithAlpha(Band == 2 ? 0.f : RingColor(P).a * Coverage.m_Peak); };
				pGraphics->SetColor4(InnerColor(A), OuterColor(B), OuterColor(D), InnerColor(C));
				const IGraphics::CFreeformItem Quad(A, B, C, D);
				pGraphics->QuadsDrawFreeform(&Quad, 1);
			}
		pGraphics->QuadsEnd();
		if(Fill > .001f)
		{
			const float DotRadius = LucentStyle::Checkbox::DotSize / 2 * Fill;
			const float Edge = std::min(Pixel / 2, DotRadius * .9f);
			CheckboxMesh(pGraphics, Center, CheckboxBoundary(Center, DotRadius - Edge),
				CheckboxBoundary(Center, DotRadius + Edge),
				[&](vec2 Point) { return Gradient(Point).WithAlpha(Fill * Opacity); });
		}
		const float TextAlpha = LucentStyle::Motion::Lerp(
			LucentStyle::Motion::Lerp(LucentStyle::Checkbox::TextIdle, LucentStyle::Checkbox::TextHover, Hot),
			LucentStyle::Checkbox::TextChecked, Fill);
		CTextScope Text(pUi->TextRender(), std::min(Label.w, pUi->TextRender()->TextWidth(11, pText)), Fill, Hot, Enabled, TextAlpha, {LucentStyle::Checkbox::Period}, IdleLabelBrightness);
		pUi->DoLabel(&Label, pText, 11, WrapLabel ? TEXTALIGN_TL : TEXTALIGN_ML, Props);
		return pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);
	}

	bool Button(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected, bool Enabled)
	{
		const bool Interactive = Enabled && pUi->Enabled();
		const bool Hover = Interactive && pUi->MouseHovered(&Rect);
		const float Light = Highlight(pId, Enabled && (Selected || Hover || (Interactive && pUi->CheckActiveItem(pId))));
		if(Light > .001f)
			Rect.Draw(Accent(Light * (Selected ? LucentStyle::Style::Selected : LucentStyle::Style::Hover) + (Interactive && pUi->CheckActiveItem(pId) ? LucentStyle::Style::Pressed : 0.f)), 0, 0);
		if(Selected)
		{
			CUIRect Underline = {Rect.x + 5, Rect.y + Rect.h - 1, std::max(0.f, Rect.w - 10), .6f};
			Underline.Draw(Accent(LucentStyle::Style::ButtonUnderline), 0, 0);
		}
		pTextRender->TextColor(Accent(Enabled ? 1.f : LucentStyle::Style::Disabled));
		const float Size = std::min(11.f, 11.f * std::max(1.f, Rect.w - 6) / std::max(1.f, pTextRender->TextWidth(11, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return Interactive && pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}

	bool SceneButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Enabled)
	{
		const bool Interactive = Enabled && pUi->Enabled();
		const bool Hover = Interactive && pUi->MouseHovered(&Rect);
		const bool Pressed = Interactive && pUi->CheckActiveItem(pId);
		const float Light = Highlight(pId, Hover || Pressed);
		Surface(Rect, false, false, IGraphics::CORNER_ALL, LucentStyle::Style::SceneBase + LucentStyle::Style::SceneHover * Light);
		CUIRect Inner;
		Rect.Margin(1, &Inner);
		Inner.Draw(Accent((Pressed ? LucentStyle::Style::ScenePressed : LucentStyle::Style::SceneIdle) + LucentStyle::Style::SceneIdle * Light), IGraphics::CORNER_ALL, RADIUS);
		pTextRender->TextColor(Accent(Enabled ? 1.f : LucentStyle::Style::Disabled));
		const float Size = std::min(12.f, 12.f * std::max(1.f, Rect.w - 12) / std::max(1.f, pTextRender->TextWidth(12, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return Interactive && pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);
	}

	bool TextButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected, float Size, int Align, bool Underline, const char *pIcon)
	{
		const float Light = Highlight(pId, pUi->Enabled() && pUi->MouseHovered(&Rect));
		const bool Pressed = pUi->CheckActiveItem(pId);
		if(Selected && Underline)
		{
			CUIRect Line = {Rect.x + 4, Rect.y + Rect.h - 2, std::max(0.f, Rect.w - 8), 1};
			Line.Draw(Accent(Pressed ? LucentStyle::Style::PressedUnderline : LucentStyle::Style::ButtonUnderline), 0, 0);
		}
		pTextRender->TextColor(Selected ? Accent(Pressed ? LucentStyle::Style::PressedText : 1.f) : LucentStyle::Gradient::Blend(LucentStyle::Style::NavigationText, LucentStyle::Style::NavigationHover, Light));
		CUIRect Label = Rect;
		if(pIcon)
		{
			CUIRect Icon;
			Label.VSplitLeft(18, &Icon, &Label);
			pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
			pUi->DoLabel(&Icon, pIcon, Size, TEXTALIGN_MC);
			pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
		SLabelProperties Props;
		Props.m_EnableWidthCheck = false;
		Props.m_MaxWidth = std::max(0.f, Label.w - 6);
		Props.m_StopAtEnd = true;
		pUi->DoLabel(&Label, pText, Size, Align, Props);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}

	bool IconButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pIcon, const char *pHint, CUIRect Rect, bool Selected, bool Tooltip)
	{
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		const bool Clicked = Button(pUi, pTextRender, pId, pIcon, Rect, Selected);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		if(Tooltip && !Clicked && !pUi->IsPopupOpen())
			pUi->DoToolTip(pId, &Rect, pHint);
		return Clicked;
	}
}
