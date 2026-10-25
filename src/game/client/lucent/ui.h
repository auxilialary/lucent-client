#ifndef GAME_CLIENT_LUCENT_UI_H
#define GAME_CLIENT_LUCENT_UI_H
#include "checkbox_geometry.h"
#include "lucent_style.h"

#include <game/client/ui.h>

#include <array>

// Lucent surfaces share UI coordinates, so radii and hairlines follow UI scale.
namespace LucentUi
{
	// Separate channels keep hover, activation and focus independently reversible.
	enum class EChannel
	{
		HOVER,
		ACTIVE,
		FOCUS
	};
	inline float Highlight(const void *pId, bool Target, EChannel Channel = EChannel::HOVER, float Duration = 0)
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
	constexpr float RADIUS = LucentStyle::Style::Radius;
	inline ColorRGBA Accent(float Alpha) { return LucentStyle::Style::Accent.WithAlpha(Alpha); }
	// Four vertex colors, one draw: no scissor bands or temporary textures for controls.
	inline void GradientFill(const CUIRect &Rect, float Alpha, int Corners = IGraphics::CORNER_ALL, float Radius = RADIUS, bool Animate = true)
	{
		const double Time = Animate ? LucentStyle::Motion::Clock().GradientTime() : 0;
		const auto Left = LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin, Time).WithAlpha(Alpha);
		const auto Right = LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin + LucentStyle::Gradient::ControlSpan, Time).WithAlpha(Alpha);
		Rect.Draw4(Left, Right, Left, Right, Corners, Radius);
	}
	inline void Divider(CUIRect Rect, float Alpha = .25f, bool Glow = false, bool Gradient = false)
	{
		CUIRect Left, Right;
		Rect.VSplitMid(&Left, &Right);
		const double Time = LucentStyle::Motion::Clock().GradientTime();
		const auto Color = [&](float Position, float Opacity) {
			return Gradient ? LucentStyle::Gradient::Sample(LucentStyle::Gradient::ControlOrigin + LucentStyle::Gradient::ControlSpan * Position, Time).WithAlpha(Opacity) : Accent(Opacity);
		};
		Left.Draw4(Color(0, 0), Color(.5f, Alpha), Color(0, 0), Color(.5f, Alpha), 0, 0);
		Right.Draw4(Color(.5f, Alpha), Color(1, 0), Color(.5f, Alpha), Color(1, 0), 0, 0);
		if(Glow)
		{
			Rect.y += Rect.h;
			Rect.h = 4;
			Rect.VSplitMid(&Left, &Right);
			Left.Draw4(Accent(0), Accent(.025f), Accent(0), Accent(0), 0, 0);
			Right.Draw4(Accent(.025f), Accent(0), Accent(0), Accent(0), 0, 0);
		}
	}
	// Filter masks contain exclusions: Active means allowed, including an empty mask.
	inline float FilterItemOpacity(const void *pId, bool Active, bool Hover)
	{
		const float Hot = Highlight(pId, Hover);
		const float Selected = Highlight(pId, Active, EChannel::ACTIVE);
		return LucentStyle::Motion::Lerp(LucentStyle::Motion::Lerp(.30f, .75f, Hot), 1.f, Selected);
	}
	inline bool FilterTab(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, const CUIRect &Rect, bool Selected)
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
	class CTextScope
	{
		ITextRender *m_pTextRender;
		ITextRender::FVertexColor m_Previous;
		ColorRGBA m_PreviousColor;

	public:
		CTextScope(ITextRender *pTextRender, float Width, float Active, float Hover, bool Enabled, float Opacity = 1.f, LucentStyle::Gradient::CParameters Parameters = {}) :
			m_pTextRender(pTextRender), m_Previous(pTextRender->TextVertexColor()), m_PreviousColor(pTextRender->GetTextColor())
		{
			const auto Idle = LucentStyle::Gradient::Blend(LucentStyle::Style::Text, LucentStyle::Gradient::White, Hover * LucentStyle::Style::TextHover).WithAlpha(Enabled ? Opacity : LucentStyle::Style::Disabled);
			pTextRender->TextColor(Idle);
			const double Time = LucentStyle::Motion::Clock().GradientTime();
			pTextRender->SetTextVertexColor(Enabled && Active > .001f ? ITextRender::FVertexColor([=](float X, ColorRGBA Base) {
				return LucentStyle::Gradient::TextSample(X, Width, Time, Active, Idle, Parameters).WithAlpha(Base.a);
			}) :
										    ITextRender::FVertexColor{});
		}
		~CTextScope()
		{
			m_pTextRender->SetTextVertexColor(m_Previous);
			m_pTextRender->TextColor(m_PreviousColor);
		}
	};
	inline void InputFrame(const CUIRect &Rect, const void *pId, bool Filled, bool Focus, bool Hover, bool Enabled, int Corners)
	{
		const float Active = Enabled ? Highlight(pId, Filled, EChannel::ACTIVE) : 0.f;
		const float Focused = Enabled ? Highlight(pId, Focus, EChannel::FOCUS) : 0.f;
		const float Hot = Enabled ? Highlight(pId, Hover) : 0.f;
		const float Alpha = Enabled ? 1.f : LucentStyle::Style::Disabled;
		Rect.Draw(Accent((LucentStyle::Style::BorderIdle + LucentStyle::Style::InputHover * Hot + LucentStyle::Style::InputFocus * Focused) * Alpha), Corners, RADIUS);
		if(Active > .001f)
			GradientFill(Rect, Active * (LucentStyle::Style::FilledBorder + (LucentStyle::Style::FocusBorder - LucentStyle::Style::FilledBorder) * Focused) * Alpha, Corners);
		CUIRect Inner;
		Rect.Margin(LucentStyle::Style::Hairline, &Inner);
		Inner.Draw(LucentStyle::Style::Surface, Corners, RADIUS);
	}
	inline void Surface(const CUIRect &Rect, bool Focus = false, bool Hover = false, int Corners = IGraphics::CORNER_ALL, float Highlight = -1)
	{
		const float GlowAlpha = Highlight >= 0 ? Highlight : float(Focus || Hover);
		if(GlowAlpha > .001f)
		{
			CUIRect Glow = Rect;
			Glow.Margin(-1, &Glow);
			Glow.Draw(Accent(GlowAlpha * LucentStyle::Style::SurfaceGlow), Corners, RADIUS + 1);
		}
		Rect.Draw(Accent(LucentStyle::Style::BorderIdle + (LucentStyle::Style::BorderActive - LucentStyle::Style::BorderIdle) * GlowAlpha),
			Corners, RADIUS);
		CUIRect Inner;
		Rect.Margin(LucentStyle::Style::Hairline, &Inner);
		Inner.Draw(LucentStyle::Style::Surface, Corners, RADIUS);
	}
	inline int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags = BUTTONFLAG_LEFT)
	{
		CUIRect Box, Label;
		Rect.VSplitLeft(LucentStyle::Style::ControlSize, &Box, &Label);
		Box.HMargin((Box.h - LucentStyle::Style::ControlSize) / 2, &Box);
		Label.VSplitLeft(LucentStyle::Style::LabelGap, nullptr, &Label);
		const float Light = pUi->Enabled() ? Highlight(pId, *pMark || pUi->MouseHovered(&Rect)) : 0.f;
		Surface(Box, false, false, IGraphics::CORNER_ALL, Light);
		const float Active = pUi->Enabled() ? Highlight(pId, *pMark != 0, EChannel::ACTIVE) : float(*pMark != 0);
		if(Active > .001f)
		{
			Box.Margin(LucentStyle::Style::CheckInset, &Box);
			// The mixed-state bar retains its shape while sharing the active gradient.
			if(*pMark == 'O')
				Box.HMargin((Box.h - LucentStyle::Style::MixedHeight) / 2, &Box);
			GradientFill(Box, Active * (pUi->Enabled() ? LucentStyle::Style::CheckAlpha : LucentStyle::Style::Disabled), IGraphics::CORNER_ALL, LucentStyle::Style::CheckRadius, pUi->Enabled());
		}
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = true;
		Props.m_EnableWidthCheck = false;
		CTextScope Text(pUi->TextRender(), std::min(Label.w, pUi->TextRender()->TextWidth(11, pText)), Active, Light, pUi->Enabled());
		pUi->DoLabel(&Label, pText, 11, TEXTALIGN_ML, Props);
		return pUi->DoButtonLogic(pId, 0, &Rect, Flags);
	}
	// One-pixel coverage fringe in the current UI projection. Geometry stays at
	// fractional coordinates: snapping each contour separately changes border widths.
	template<size_t N, typename FColor>
	inline void CheckboxMesh(IGraphics *pGraphics, vec2 Center, const std::array<vec2, N> &Inner, const std::array<vec2, N> &Outer, FColor Color)
	{
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(size_t I = 0; I < N; ++I)
		{
			const size_t J = (I + 1) % N;
			pGraphics->SetColor4(Color(Center), Color(Inner[I]), Color(Inner[J]), Color(Inner[J]));
			const IGraphics::CFreeformItem Triangle(Center, Inner[I], Inner[J], Inner[J]);
			pGraphics->QuadsDrawFreeform(&Triangle, 1);
			// SetColor4 takes TL, TR, BL, BR; freeform points are TL, TR, BR, BL.
			pGraphics->SetColor4(Color(Inner[I]), Color(Outer[I]).WithAlpha(0), Color(Outer[J]).WithAlpha(0), Color(Inner[J]));
			const IGraphics::CFreeformItem Edge(Inner[I], Outer[I], Inner[J], Outer[J]);
			pGraphics->QuadsDrawFreeform(&Edge, 1);
		}
		pGraphics->QuadsEnd();
	}

	// Dedicated Filters presentation; input and the full row hitbox stay in CUi.
	inline int FilledCheckBox(CUi *pUi, const void *pId, const char *pText, bool Checked, const CUIRect &Rect)
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
		const bool Enabled = pUi->Enabled();
		const float Fill = pState->m_Fill.Update(Checked, Clock.m_Now, Clock.m_Animate, LucentStyle::Checkbox::FillDuration);
		const float Hot = pState->m_Hover.Update(Enabled && pUi->MouseHovered(&Rect), Clock.m_Now, Clock.m_Animate);
		const float Alpha = Enabled ? 1.f : LucentStyle::Style::Disabled;
		CUIRect Label = Rect;
		Label.VSplitLeft(LucentStyle::Checkbox::LabelOffset, nullptr, &Label);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = true;
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
		const vec2 Cursor = CUi::CalcAlignedCursorPos(&Label, vec2(TextWidth, TextHeight), TEXTALIGN_ML, &MaxHeight);
		const float Baseline = round_to_int(Cursor.y / Pixel) * Pixel + AlignedSize;
		const vec2 Center = CheckboxCenter(vec2(Rect.x, Rect.y), Baseline - Rect.y, CapHeight);
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
		CTextScope Text(pUi->TextRender(), std::min(Label.w, pUi->TextRender()->TextWidth(11, pText)), Fill, Hot, Enabled, TextAlpha, {LucentStyle::Checkbox::Period});
		pUi->DoLabel(&Label, pText, 11, TEXTALIGN_ML, Props);
		return pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);
	}
	class CScope
	{
		CUi *m_pUi;
		bool m_Previous;

	public:
		explicit CScope(CUi *pUi) : m_pUi(pUi), m_Previous(pUi->LucentStyle()) { pUi->SetLucentStyle(true); }
		~CScope() { m_pUi->SetLucentStyle(m_Previous); }
	};
	class CInteractionScope
	{
		CUi *m_pUi;
		bool m_Previous;

	public:
		CInteractionScope(CUi *pUi, bool Enabled) : m_pUi(pUi), m_Previous(pUi->Enabled()) { pUi->SetEnabled(m_Previous && Enabled); }
		~CInteractionScope() { m_pUi->SetEnabled(m_Previous); }
	};
	class COpacityScope
	{
		ITextRender *m_pTextRender;
		float m_TextOpacity, m_RectOpacity;

	public:
		COpacityScope(ITextRender *pTextRender, float Opacity) :
			m_pTextRender(pTextRender), m_TextOpacity(pTextRender->RenderOpacity()), m_RectOpacity(CUIRect::DrawOpacity())
		{
			m_pTextRender->SetRenderOpacity(m_TextOpacity * Opacity);
			CUIRect::SetDrawOpacity(m_RectOpacity * Opacity);
		}
		~COpacityScope()
		{
			m_pTextRender->SetRenderOpacity(m_TextOpacity);
			CUIRect::SetDrawOpacity(m_RectOpacity);
		}
	};
	inline bool Button(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, bool Enabled = true)
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
	// Primary scene action: the same restrained hairline and edge light as the menu.
	inline bool SceneButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Enabled = true)
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
	// Text navigation shares control geometry, without a persistent button surface.
	inline bool TextButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, float Size = 11, int Align = TEXTALIGN_MC, bool Underline = true, const char *pIcon = nullptr)
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
	inline bool IconButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pIcon, const char *pHint, CUIRect Rect, bool Selected = false, bool Tooltip = true)
	{
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		const bool Clicked = Button(pUi, pTextRender, pId, pIcon, Rect, Selected);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		if(Tooltip && !Clicked && !pUi->IsPopupOpen())
			pUi->DoToolTip(pId, &Rect, pHint);
		return Clicked;
	}
}
#endif
