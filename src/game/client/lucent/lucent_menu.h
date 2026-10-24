#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_H
#include "lucent_logo.h"
#include "lucent_menu_interaction.h"
#include "radial_transition.h"
#include "server_controls.h"

#include <game/client/component.h>
#include <game/client/ui.h>

class CLucentMenu : public CComponentInterfaces
{
	CLucentLogo m_Logo;
	CLucentMenuInteraction m_Interaction;
	CButtonContainer m_aButtons[12];
	float m_Progress = 0;
	float m_aHighlight[12] = {};
	double m_aPulseStart[6] = {};
	bool m_aWasHot[6] = {};
	double m_LastFrame = -1;
	double m_AnimationEpoch = -1;
	double m_EffectTime = 0;
	float m_LogoEnergy = 0;
	vec2 m_OrbitOffset = vec2(0, 0);
	int m_Focus = -1;
	CLucentSecondaryNavigation m_Secondary;
	CLucentServerControls m_ServerControls;
	vec2 m_LastMouse = vec2(-1, -1);
	CRadialTransition m_RadialTransition;
	IGraphics::CTextureHandle m_SourceScene;
	vec2 m_SceneSize = vec2(0, 0);
	bool m_BlockInput = false;
	bool m_UiWasEnabled = true;
	vec2 m_NavigationCursor = vec2(0, 0);
	vec2 m_UiSize = vec2(0, 0);
	void RenderMainMenu(CUIRect MainView);
	void RenderRadialComposition();
	void ReleaseScenes();
	bool CheckHotKey(int Key) const;
	bool Button(int Index, const char *pLabel, const CUIRect &Rect, float Alpha, bool Enabled, bool Activate, float Offset = 0);

public:
	void RenderStartMenu(CUIRect MainView);
	void BeginNavigationFrame(int Screen);
	void EndNavigationFrame(int Screen);
	bool NavigationBlocked() const { return m_RadialTransition.Active(); }
	void NavigationCursor(vec2 Position) { m_NavigationCursor = Position; }
	void CancelTransition();
	void ResetLogo(ITextRender *pTextRender)
	{
		m_Logo.Reset(pTextRender);
		m_Interaction.Reset();
		m_ServerControls.Reset();
		// Complete navigation on resize; cached pixels must never be stretched.
		m_RadialTransition.Update(0, false);
		ReleaseScenes();
		m_BlockInput = false;
		m_LastFrame = -1;
	}
};
#endif
