#ifndef GAME_CLIENT_LUCENT_UI_SCOPES_H
#define GAME_CLIENT_LUCENT_UI_SCOPES_H

#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/style/gradient.h>
#include <game/client/ui.h>

namespace LucentUi
{
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
}
#endif
