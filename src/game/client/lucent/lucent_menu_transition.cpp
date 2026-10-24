#include "lucent_menu.h"

#include <base/dbg.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

void CLucentMenu::ReleaseScenes()
{
	Graphics()->UnloadTexture(&m_SourceScene);
}

void CLucentMenu::CancelTransition()
{
	ReleaseScenes();
	m_RadialTransition.Reset();
	m_Interaction.Reset();
	m_Secondary.Reset();
	m_ServerControls.Reset();
	m_LastFrame = -1;
	m_Focus = -1;
}

void CLucentMenu::RenderStartMenu(CUIRect MainView)
{
	const bool WasActive = m_RadialTransition.Active();
	if(WasActive && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
	{
		CancelTransition();
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
		Input()->Clear();
		RenderMainMenu(MainView);
		return;
	}
	m_RadialTransition.Update(static_cast<double>(time_get()) / time_freq(), g_Config.m_ClLucentLogoAnimation != 0);
	if(WasActive && m_SceneSize != Graphics()->ScreenSize())
		m_RadialTransition.Update(0, false);
	const bool UiEnabled = Ui()->Enabled();
	if(!WasActive && !m_RadialTransition.Completed())
		RenderMainMenu(MainView);

	if(m_RadialTransition.Active() && !m_SourceScene.IsValid())
	{
		// Copy the complete source at its exact click-frame animation state.
		// The destination stays live, with its normal buffered text renderer.
		m_SceneSize = Graphics()->ScreenSize();
		m_SourceScene = Graphics()->CreateSceneTexture();
		if(!m_SourceScene.IsValid())
			m_RadialTransition.Update(0, false);
	}
	if(m_RadialTransition.Active() || m_RadialTransition.Completed())
	{
		Ui()->SetEnabled(false);
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
		Input()->Clear(); // Also blocks the browser's direct keyboard shortcuts.
		GameClient()->m_Menus.RenderLucentBrowser(MainView);
		if(m_RadialTransition.Completed())
		{
			// Live browser in the completion frame; no cached destination or delay.
			ReleaseScenes();
			m_RadialTransition.Reset();
			GameClient()->m_Menus.SetShowStart(false);
		}
		else
			RenderRadialComposition();
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
	}
	Ui()->SetEnabled(UiEnabled);
	m_BlockInput = false;
}

void CLucentMenu::RenderRadialComposition()
{
	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const vec2 TopLeft(Screen.x, Screen.y), Size(Screen.w, Screen.h);
	const auto Mask = m_RadialTransition.Mask(TopLeft, Size, 24.0f);
	dbg_assert(!Ui()->IsClipped(), "Scene composition requires a balanced UI clip stack");
	Graphics()->ClipDisable();
	Graphics()->BlendNormal();
	Graphics()->WrapClamp();
	// One shader invocation per pixel applies coverage to the complete opaque
	// source, including text. No CPU mask, mesh sampling or layer overdraw.
	Graphics()->RenderSceneTexture(m_SourceScene, Mask.m_Origin, Mask.m_Radius, Mask.m_Feather);
	Graphics()->WrapNormal();
}
