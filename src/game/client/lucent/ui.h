#ifndef GAME_CLIENT_LUCENT_UI_H
#define GAME_CLIENT_LUCENT_UI_H
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
	inline float Highlight(const void *pId, bool Target, EChannel Channel = EChannel::HOVER)
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
			Channel == EChannel::FOCUS ? LucentStyle::Motion::Focus : LucentStyle::Motion::Hover);
	}
	constexpr float RADIUS = LucentStyle::Style::Radius;
	inline ColorRGBA Accent(float Alpha) { return LucentStyle::Style::Accent.WithAlpha(Alpha); }
	// Four vertex colors, one draw: no scissor bands or temporary textures for controls.
	inline void GradientFill(const CUIRect &Rect, float Alpha, int Corners = IGraphics::CORNER_ALL, float Radius = RADIUS)
	{
		const double Time = LucentStyle::Motion::Clock().GradientTime();
		const auto Left = LucentStyle::Gradient::Sample(0, Time).WithAlpha(Alpha);
		const auto Right = LucentStyle::Gradient::Sample(.5f, Time).WithAlpha(Alpha);
		Rect.Draw4(Left, Right, Left, Right, Corners, Radius);
	}
	inline void Divider(CUIRect Rect, float Alpha = .25f, bool Glow = false)
	{
		CUIRect Left, Right;
		Rect.VSplitMid(&Left, &Right);
		Left.Draw4(Accent(0), Accent(Alpha), Accent(0), Accent(Alpha), 0, 0);
		Right.Draw4(Accent(Alpha), Accent(0), Accent(Alpha), Accent(0), 0, 0);
		if(Glow)
		{
			Rect.y += Rect.h;
			Rect.h = 4;
			Rect.VSplitMid(&Left, &Right);
			Left.Draw4(Accent(0), Accent(.025f), Accent(0), Accent(0), 0, 0);
			Right.Draw4(Accent(.025f), Accent(0), Accent(0), Accent(0), 0, 0);
		}
	}
	inline void InputFrame(const CUIRect &Rect, const void *pId, bool Filled, bool Focus, bool Hover, bool Enabled, int Corners)
	{
		const float Active = Highlight(pId, Enabled && Filled, EChannel::ACTIVE);
		const float Focused = Highlight(pId, Enabled && Focus, EChannel::FOCUS);
		const float Hot = Highlight(pId, Enabled && Hover);
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
		const float Light = Highlight(pId, *pMark || (pUi->Enabled() && pUi->MouseHovered(&Rect)));
		Surface(Box, false, false, IGraphics::CORNER_ALL, Light);
		const float Active = Highlight(pId, *pMark != 0, EChannel::ACTIVE);
		if(Active > .001f)
		{
			Box.Margin(LucentStyle::Style::CheckInset, &Box);
			// The mixed-state bar retains its shape while sharing the active gradient.
			if(*pMark == 'O')
				Box.HMargin((Box.h - LucentStyle::Style::MixedHeight) / 2, &Box);
			GradientFill(Box, Active * (pUi->Enabled() ? LucentStyle::Style::CheckAlpha : LucentStyle::Style::Disabled), IGraphics::CORNER_ALL, LucentStyle::Style::CheckRadius);
		}
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_StopAtEnd = true;
		Props.m_EnableWidthCheck = false;
		pUi->DoLabel(&Label, pText, 11, TEXTALIGN_ML, Props);
		return pUi->DoButtonLogic(pId, 0, &Rect, Flags);
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
