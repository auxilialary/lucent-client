#ifndef GAME_CLIENT_LUCENT_GAME_PING_SENDER_H
#define GAME_CLIENT_LUCENT_GAME_PING_SENDER_H
#include "ping.h"
#include "ping_identity.h"

#include <game/client/render.h>

#include <array>
#include <string>
class CGameClient;
namespace LucentPing
{
	struct CSenderAppearance
	{
		bool m_Available = false;
		std::string m_Name;
		CTeeRenderInfo m_Tee;
	};
	// Only current normalized DDNet state supplies appearance; network packets never supply names/skins.
	inline CSenderAppearance SenderAppearance(int Id, bool InSnapshot, bool Active, const char *pName, const CTeeRenderInfo *pTee)
	{
		if(!SenderPresent(Id, InSnapshot, Active) || !pName || !pTee)
			return {};
		return {true, pName, *pTee};
	}
	using CSenderAppearances = std::array<CSenderAppearance, 8>;
	CSenderAppearances ResolveSenders(const CGameClient &Game, const CState &State);
}
#endif
