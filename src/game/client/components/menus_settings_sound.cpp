/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
#include "menus.h"

#include <engine/shared/config.h>
#include <engine/sound.h>

#include <game/client/lucent/settings/sound.h>
#include <game/client/ui.h>

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	using C = LucentSettings::ESoundControl;
	int *apValues[] = {&g_Config.m_SndEnable, &g_Config.m_SndGame, &g_Config.m_SndGun, &g_Config.m_SndMusic, &g_Config.m_SndVolume, &g_Config.m_SndGameVolume, &g_Config.m_SndChatVolume, &g_Config.m_SndMapVolume, &g_Config.m_SndBackgroundMusicVolume, &g_Config.m_SndNonactiveMute, &g_Config.m_SndLongPain, &g_Config.m_SndServerMessage, &g_Config.m_SndChat, &g_Config.m_SndTeamChat, &g_Config.m_SndHighlight};
	static_assert(std::size(apValues) == LucentSettings::SoundControlCount);
	const LucentSettings::CSoundState State(g_Config.m_SndEnable, g_Config.m_SndGame, g_Config.m_SndMusic);
	LucentSettings::RenderSound(Ui(), TextRender(), MainView, State, [&](C Control, CUIRect Row, bool VisualEnabled, const CUIRect *pValue) {
		int *pConfig = apValues[int(Control)];
		if(!Ui()->Enabled() && Ui()->CheckActiveItem(pConfig))
			Ui()->SetActiveItem(nullptr);
		if(LucentSettings::SoundSlider(Control))
		{
			int InactiveValue = *pConfig;
			Ui()->DoScrollbarOption(pConfig, Ui()->Enabled() ? pConfig : &InactiveValue, &Row, LucentSettings::SoundLabel(Control), 0, 100, &CUi::ms_LogarithmicScrollbarScale,
				CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT, "%", pValue);
		}
		else if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), pConfig, LucentSettings::SoundLabel(Control), *pConfig, &Row, VisualEnabled))
		{
			*pConfig ^= 1;
			if(Control == C::ENABLE || Control == C::MUSIC)
				UpdateMusicState();
		}
	});
	m_NeedRestartSound = g_Config.m_SndEnable && !Sound()->IsSoundEnabled();
}
