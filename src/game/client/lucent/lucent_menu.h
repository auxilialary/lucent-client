#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_H
#include "lucent_logo.h"
#include "lucent_menu_interaction.h"
#include "radial_transition.h"

#include <game/client/component.h>
#include <game/client/ui.h>

class CLucentMenu : public CComponentInterfaces
{
	CLucentLogo m_Logo;
	CLucentMenuInteraction m_Interaction;
	CButtonContainer m_aButtons[13];
	float m_Progress = 0;
	float m_aHighlight[13] = {};
	double m_aPulseStart[6] = {};
	bool m_aWasHot[6] = {};
	double m_LastFrame = -1;
	double m_AnimationEpoch = -1;
	double m_EffectTime = 0;
	float m_LogoEnergy = 0;
	vec2 m_OrbitOffset = vec2(0, 0);
	int m_Focus = -1;
	CLucentSecondaryNavigation m_Secondary;
	vec2 m_LastMouse = vec2(-1, -1);
	CRadialTransition m_RadialTransition;
	STextContainerIndex m_aDestinationText[4];
	vec2 m_DestinationSize = vec2(0, 0);
	CButtonContainer m_BackButton, m_BrowserButton;
	bool m_BlockInput = false;
	void RenderMainMenu(CUIRect MainView);
	void RenderRadialDestination(bool Interactive);
	bool CheckHotKey(int Key) const;
	bool Button(int Index, const char *pLabel, const CUIRect &Rect, float Alpha, bool Enabled, bool Activate, float Offset = 0);

public:
	void RenderStartMenu(CUIRect MainView);
	void ResetLogo(ITextRender *pTextRender)
	{
		m_Logo.Reset(pTextRender);
		m_Interaction.Reset();
		// Resizing invalidates source geometry. Finish navigation directly;
		// a completed destination keeps ownership while its text is rebuilt.
		m_RadialTransition.Update(0, false);
		for(auto &Text : m_aDestinationText)
			if(Text.Valid())
				pTextRender->DeleteTextContainer(Text);
		m_DestinationSize = vec2(0, 0);
		m_BlockInput = false;
		m_LastFrame = -1;
	}
};
#endif
