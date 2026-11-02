#include "ping_sender.h"

#include <game/client/gameclient.h>
namespace LucentPing
{
	static_assert(MaxSenderClients == MAX_CLIENTS);
	CSenderAppearances ResolveSenders(const CGameClient &Game, const CState &State)
	{
		CSenderAppearances Result;
		for(size_t i = 0; i < State.Events().size(); ++i)
		{
			const auto &Event = State.Events()[i];
			const int Id = Event.m_SenderClientId;
			if(!Event.m_Remote || !ValidSenderClientId(Id))
				continue;
			const auto &Client = Game.m_aClients[Id];
			Result[i] = SenderAppearance(Id, Game.m_Snap.m_apPlayerInfos[Id] != nullptr, Client.m_Active, Client.m_aName, &Client.m_RenderInfo);
		}
		return Result;
	}
}
