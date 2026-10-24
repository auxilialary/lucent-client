#include "lucent_menu.h"

#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

void CLucentMenu::RenderStartMenu(CUIRect MainView)
{
	const bool WasActive = m_RadialTransition.Active();
	m_RadialTransition.Update(static_cast<double>(time_get()) / time_freq(), g_Config.m_ClLucentLogoAnimation != 0);
	m_BlockInput = WasActive;
	const bool UiEnabled = Ui()->Enabled();
	if(!m_RadialTransition.Completed())
	{
		if(WasActive)
			Ui()->SetEnabled(false);
		RenderMainMenu(MainView);
	}
	Ui()->SetEnabled(UiEnabled);
	if(m_RadialTransition.Active() || m_RadialTransition.Completed())
	{
		// Both scene paths coexist until completion. The source timeline and
		// interaction state stay frozen; only the spatial mask advances.
		const bool Focused = Kernel()->RequestInterface<IEngineGraphics>()->WindowActive() != 0 && !GameClient()->m_GameConsole.IsActive();
		RenderRadialDestination(m_RadialTransition.Completed() && !WasActive && UiEnabled && Focused);
		if(WasActive || m_RadialTransition.Active())
		{
			Ui()->SetActiveItem(nullptr);
			Ui()->SetHotItem(nullptr);
			Ui()->ClearHotkeys();
		}
	}
	m_BlockInput = false;
}

void CLucentMenu::RenderRadialDestination(bool Interactive)
{
	const CUIRect Screen = *Ui()->Screen();
	const vec2 TopLeft(Screen.x, Screen.y), Size(Screen.w, Screen.h);
	const auto Mask = m_RadialTransition.Mask(TopLeft, Size, 40.0f);
	const auto Coverage = [&](vec2 Point) {
		return m_RadialTransition.Completed() ? 1.0f : Mask.Coverage(Point);
	};
	// Backend-independent geometry compositing: smoothstep coverage is sampled
	// on a fine mesh, including the destination glyphs. No frame captures,
	// render-target allocations, or graphics-backend-specific state are needed.
	const auto Rect = [&](CUIRect Area, ColorRGBA Color) {
		const int Columns = m_RadialTransition.Active() ? std::max(1, static_cast<int>(std::ceil(Area.w / 8))) : 1;
		const int Rows = m_RadialTransition.Active() ? std::max(1, static_cast<int>(std::ceil(Area.h / 8))) : 1;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int y = 0; y < Rows; ++y)
			for(int x = 0; x < Columns; ++x)
			{
				const vec2 TL(Area.x + Area.w * x / Columns, Area.y + Area.h * y / Rows);
				const vec2 BR(Area.x + Area.w * (x + 1) / Columns, Area.y + Area.h * (y + 1) / Rows);
				const auto Tint = [&](vec2 Point) { return ColorRGBA(Color.r, Color.g, Color.b, Color.a * Coverage(Point)); };
				const ColorRGBA A = Tint(TL), B = Tint(vec2(BR.x, TL.y)), C = Tint(vec2(TL.x, BR.y)), D = Tint(BR);
				if(A.a == 0 && B.a == 0 && C.a == 0 && D.a == 0)
					continue;
				Graphics()->SetColor4(A, B, C, D);
				const IGraphics::CQuadItem Quad(TL, BR - TL);
				Graphics()->QuadsDrawTL(&Quad, 1);
			}
		Graphics()->QuadsEnd();
		Graphics()->SetColor(1, 1, 1, 1);
	};
	Rect(Screen, ColorRGBA(0.055f, 0.040f, 0.075f, 1));
	CUIRect Back = {Screen.x + 32, Screen.y + Screen.h - 72, 120, 32};
	CUIRect Browser = {Screen.x + Screen.w - 292, Back.y, 260, 32};
	Rect(Back, ColorRGBA(0.13f, 0.09f, 0.16f, 1));
	Rect(Browser, ColorRGBA(0.18f, 0.11f, 0.17f, 1));
	if(m_DestinationSize != Size)
	{
		m_DestinationSize = Size;
		const char *apText[] = {"Server Browser Placeholder", "Choose a server in the existing browser to start playing.", "Back (Esc)", "Open existing server browser"};
		const vec2 aPositions[] = {TopLeft + vec2(32, 64), TopLeft + vec2(32, 110), vec2(Back.x + 12, Back.y + 8), vec2(Browser.x + 12, Browser.y + 8)};
		const ColorRGBA Previous = TextRender()->GetTextColor();
		const unsigned Flags = TextRender()->GetRenderFlags();
		TextRender()->TextColor(1, 1, 1, 1);
		TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_KERNING | TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);
		for(int i = 0; i < 4; ++i)
		{
			if(m_aDestinationText[i].Valid())
				TextRender()->DeleteTextContainer(m_aDestinationText[i]);
			CTextCursor Cursor;
			Cursor.m_FontSize = i == 0 ? 26 : 14;
			Cursor.SetPosition(aPositions[i]);
			TextRender()->CreateTextContainer(m_aDestinationText[i], &Cursor, apText[i]);
		}
		TextRender()->SetRenderFlags(Flags);
		TextRender()->TextColor(Previous);
	}
	for(const auto &Text : m_aDestinationText)
	{
		if(m_RadialTransition.Active())
			TextRender()->RenderTextContainerSoftMask(Text, ColorRGBA(0.96f, 0.89f, 0.94f, 1), ColorRGBA(0, 0, 0, 0), Coverage);
		else
			TextRender()->RenderTextContainer(Text, ColorRGBA(0.96f, 0.89f, 0.94f, 1), ColorRGBA(0, 0, 0, 0));
	}
	if(!Interactive)
		return;
	if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE) || Ui()->DoButtonLogic(&m_BackButton, 0, &Back, BUTTONFLAG_LEFT))
	{
		m_RadialTransition.Reset();
		m_Interaction.Reset();
		m_Secondary.Reset();
		m_LastFrame = -1;
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
	}
	else if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || Ui()->DoButtonLogic(&m_BrowserButton, 0, &Browser, BUTTONFLAG_LEFT))
	{
		m_RadialTransition.Reset();
		GameClient()->m_Menus.SetShowStart(false);
		const int Page = g_Config.m_UiPage;
		GameClient()->m_Menus.SetMenuPage(Page >= CMenus::PAGE_INTERNET && Page <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? Page : CMenus::PAGE_INTERNET);
		Ui()->SetActiveItem(nullptr);
		Ui()->ClearHotkeys();
	}
}
