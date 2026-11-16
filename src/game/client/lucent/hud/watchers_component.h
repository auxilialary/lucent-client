#ifndef GAME_CLIENT_LUCENT_HUD_WATCHERS_COMPONENT_H
#define GAME_CLIENT_LUCENT_HUD_WATCHERS_COMPONENT_H

#include "tee_alignment.h"
#include <engine/shared/protocol.h>
#include <game/client/component.h>

class CLucentWatchers : public CComponent
{
	bool m_aSpectatorWatchers[MAX_CLIENTS] = {};
	int m_Count = 0;
	LucentStyle::Motion::CTransition m_LucentWatcherVisibility;
	LucentHud::Alignment::CRowPosition m_LucentWatcherHeight, m_LucentWatcherWidth;
	char m_aaLucentWatcherNames[MAX_CLIENTS][128] = {};
	LucentStyle::Motion::CTransition m_aLucentWatcherRows[MAX_CLIENTS];
	void ResetPresentation();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnNewSnapshot() override;
	bool IsWatchingLocal(int ClientId) const;
	void Render(float Width, float Height, const LucentHud::CRect &TimerBounds);
};
#endif
