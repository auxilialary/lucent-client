#ifndef GAME_CLIENT_LUCENT_LUCENT_MENU_H
#define GAME_CLIENT_LUCENT_LUCENT_MENU_H
#include "lucent_logo.h"
#include "lucent_menu_interaction.h"

#include <game/client/component.h>
#include <game/client/ui.h>

class CLucentMenu : public CComponentInterfaces
{
	CLucentLogo m_Logo;
	CLucentMenuInteraction m_Interaction;
	CButtonContainer m_aButtons[13];
	float m_Progress = 0;
	double m_LastFrame = -1;
	int m_Focus = -1;
	bool m_Extras = false;
	vec2 m_LastMouse = vec2(-1, -1);
	bool CheckHotKey(int Key) const;
	bool Button(int Index, const char *pLabel, const CUIRect &Rect, float Alpha, bool Enabled, bool Activate, float Offset = 0);

public:
	void RenderStartMenu(CUIRect MainView);
	void ResetLogo(ITextRender *pTextRender)
	{
		m_Logo.Reset(pTextRender);
		m_Interaction.Reset();
		m_LastFrame = -1;
	}
};
#endif
