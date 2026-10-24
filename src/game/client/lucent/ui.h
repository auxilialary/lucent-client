#ifndef GAME_CLIENT_LUCENT_UI_H
#define GAME_CLIENT_LUCENT_UI_H
#include <base/time.h>

#include <engine/shared/config.h>

#include <game/client/ui.h>

#include <array>

// Lucent surfaces share UI coordinates, so radii and hairlines follow UI scale.
namespace LucentUi
{
	// Fixed storage avoids allocations and stale-pointer growth in immediate UI.
	inline float Highlight(const void *pId, bool Target)
	{
		struct CState
		{
			const void *m_pId = nullptr;
			float m_Value = 0, m_From = 0, m_Elapsed = 0;
			bool m_Target = false;
			double m_Time = 0;
		};
		static std::array<CState, 256> s_aStates;
		const double Now = double(time_get()) / time_freq();
		auto *pState = &s_aStates[0];
		for(auto &State : s_aStates)
		{
			if(State.m_pId == pId)
			{
				pState = &State;
				break;
			}
			if(State.m_Time < pState->m_Time)
				pState = &State;
		}
		if(pState->m_pId != pId)
		{
			*pState = {};
			pState->m_pId = pId;
			pState->m_Time = Now;
		}
		if(Target != pState->m_Target)
		{
			pState->m_From = pState->m_Value;
			pState->m_Target = Target;
			pState->m_Elapsed = 0;
		}
		pState->m_Elapsed = std::min(.18f, pState->m_Elapsed + float(std::max(0.0, Now - pState->m_Time)));
		const float T = g_Config.m_ClLucentLogoAnimation ? 1 - std::pow(1 - pState->m_Elapsed / .18f, 3.f) : 1;
		pState->m_Value = pState->m_From + (float(Target) - pState->m_From) * T;
		pState->m_Time = Now;
		return pState->m_Value;
	}
	constexpr float RADIUS = 3.0f;
	inline ColorRGBA Accent(float Alpha) { return ColorRGBA(1, .82f, .91f, Alpha); }
	inline void Surface(const CUIRect &Rect, bool Focus = false, bool Hover = false, int Corners = IGraphics::CORNER_ALL, float Highlight = -1)
	{
		const float GlowAlpha = Highlight >= 0 ? Highlight : float(Focus || Hover);
		if(GlowAlpha > .001f)
		{
			CUIRect Glow = Rect;
			Glow.Margin(-1, &Glow);
			Glow.Draw(Accent(GlowAlpha * .07f), Corners, RADIUS + 1);
		}
		Rect.Draw(Accent(.12f + .28f * GlowAlpha),
			Corners, RADIUS);
		CUIRect Inner;
		Rect.Margin(.5f, &Inner);
		Inner.Draw(ColorRGBA(.025f, .021f, .035f, .98f), Corners, RADIUS);
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
			Rect.Draw(Accent(Light * (Selected ? .13f : .045f) + (Interactive && pUi->CheckActiveItem(pId) ? .07f : 0.f)), 0, 0);
		if(Selected)
		{
			CUIRect Underline = {Rect.x + 5, Rect.y + Rect.h - 1, std::max(0.f, Rect.w - 10), .6f};
			Underline.Draw(Accent(.7f), 0, 0);
		}
		pTextRender->TextColor(Accent(Enabled ? 1.f : .35f));
		const float Size = std::min(11.f, 11.f * std::max(1.f, Rect.w - 6) / std::max(1.f, pTextRender->TextWidth(11, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return Interactive && pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}
	// Primary scene action: the same restrained hairline and edge light as the menu.
	inline bool SceneButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect)
	{
		const bool Hover = pUi->Enabled() && pUi->MouseHovered(&Rect);
		const bool Pressed = pUi->Enabled() && pUi->CheckActiveItem(pId);
		const float Light = Highlight(pId, Hover || Pressed);
		Surface(Rect, false, false, IGraphics::CORNER_ALL, .25f + .75f * Light);
		CUIRect Inner;
		Rect.Margin(1, &Inner);
		Inner.Draw(Accent((Pressed ? .10f : .025f) + .025f * Light), IGraphics::CORNER_ALL, RADIUS);
		pTextRender->TextColor(Accent(1));
		const float Size = std::min(12.f, 12.f * std::max(1.f, Rect.w - 12) / std::max(1.f, pTextRender->TextWidth(12, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT);
	}
	// Text navigation shares control geometry, without a persistent button surface.
	inline bool TextButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, float Size = 11, int Align = TEXTALIGN_MC, bool Underline = true)
	{
		const float Light = Highlight(pId, pUi->Enabled() && pUi->MouseHovered(&Rect));
		const bool Pressed = pUi->CheckActiveItem(pId);
		if(Selected && Underline)
		{
			CUIRect Line = {Rect.x + 4, Rect.y + Rect.h - 2, std::max(0.f, Rect.w - 8), 1};
			Line.Draw(Accent(Pressed ? .95f : .7f), 0, 0);
		}
		pTextRender->TextColor(Selected ? Accent(Pressed ? .8f : 1.f) : ColorRGBA(.66f + .25f * Light, .63f + .20f * Light, .69f + .19f * Light, 1));
		SLabelProperties Props;
		Props.m_MaxWidth = std::max(0.f, Rect.w - 6);
		Props.m_StopAtEnd = true;
		pUi->DoLabel(&Rect, pText, Size, Align, Props);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}
	inline bool IconButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pIcon, const char *pHint, CUIRect Rect, bool Selected = false)
	{
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		const bool Clicked = Button(pUi, pTextRender, pId, pIcon, Rect, Selected);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		pUi->DoToolTip(pId, &Rect, pHint);
		return Clicked;
	}
}
#endif
