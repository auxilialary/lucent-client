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
			float m_Value = 0;
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
			*pState = {pId, 0, Now - .016};
		const float Response = g_Config.m_ClLucentLogoAnimation ? 1 - std::exp(-float(std::clamp(Now - pState->m_Time, 0.0, .1)) * 18) : 1;
		pState->m_Value += (float(Target) - pState->m_Value) * Response;
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
		Rect.Draw(Accent(Focus ? .4f : Hover ? .23f :
						       .12f),
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
	inline bool Button(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, bool Enabled = true)
	{
		const bool Hover = Enabled && pUi->MouseHovered(&Rect);
		const float Light = Highlight(pId, Selected || Hover || pUi->CheckActiveItem(pId));
		if(Light > .001f)
			Rect.Draw(Accent(Light * (Selected ? .13f : .045f)), 0, 0);
		if(Selected)
		{
			CUIRect Underline = {Rect.x + 5, Rect.y + Rect.h - 1, std::max(0.f, Rect.w - 10), .6f};
			Underline.Draw(Accent(.7f), 0, 0);
		}
		pTextRender->TextColor(Accent(Enabled ? 1.f : .35f));
		const float Size = std::min(11.f, 11.f * std::max(1.f, Rect.w - 6) / std::max(1.f, pTextRender->TextWidth(11, pText)));
		pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_MC);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return Enabled && pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
	}
	// Text navigation shares control geometry, without a persistent button surface.
	inline bool TextButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, float Size = 11, int Align = TEXTALIGN_MC)
	{
		const float Light = Highlight(pId, pUi->Enabled() && pUi->MouseHovered(&Rect));
		const bool Pressed = pUi->CheckActiveItem(pId);
		if(Selected)
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
