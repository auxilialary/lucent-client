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
	RenderMainMenu(MainView);
}

void CLucentMenu::BeginNavigationFrame(int Screen)
{
	m_RadialTransition.BeginFrame(Screen);
	m_UiWasEnabled = Ui()->Enabled();
	if(Screen == -3)
		CancelTransition();
	if(m_RadialTransition.Active())
	{
		const CUIRect Rect = *Ui()->Screen();
		if(m_SceneSize != Graphics()->ScreenSize() || m_UiSize != vec2(Rect.w, Rect.h))
			m_RadialTransition.Update(0, false);
		else
			m_RadialTransition.Update(double(time_get()) / time_freq(), g_Config.m_ClLucentLogoAnimation != 0);
		Ui()->SetEnabled(false);
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
		Input()->Clear();
	}
}

void CLucentMenu::EndNavigationFrame(int Screen)
{
	if(m_RadialTransition.Active())
		RenderRadialComposition();
	else if(m_RadialTransition.Completed())
	{
		ReleaseScenes();
		m_RadialTransition.Reset();
	}
	else if(m_RadialTransition.CaptureNeeded(Screen, g_Config.m_ClLucentLogoAnimation != 0))
	{
		// Navigation changes state while drawing the source. Capture once, after
		// its complete render; the destination is rendered live next frame.
		m_SourceScene = Graphics()->CreateSceneTexture();
		if(m_SourceScene.IsValid())
		{
			const CUIRect Rect = *Ui()->Screen();
			m_SceneSize = Graphics()->ScreenSize();
			m_UiSize = vec2(Rect.w, Rect.h);
			const vec2 Origin = vec2(Rect.x, Rect.y) + m_NavigationCursor * m_UiSize;
			m_RadialTransition.Start(double(time_get()) / time_freq(), Origin, true);
			if(auto *pActive = CLineInput::GetActiveInput())
				pActive->Deactivate();
			Ui()->SetActiveItem(nullptr);
			Ui()->SetHotItem(nullptr);
			Ui()->ClearHotkeys();
			Input()->Clear();
		}
	}
	Ui()->SetEnabled(m_UiWasEnabled);
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
