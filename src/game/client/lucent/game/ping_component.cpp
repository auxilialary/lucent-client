#include "ping_component.h"

#include "ping_geometry.h"
#include "ping_renderer.h"
#include "ping_sender.h"

#include <base/hash.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/map.h>

#include <game/client/gameclient.h>

#include <algorithm>
void CLucentPings::OnConsoleInit()
{
	Console()->Chain("lucent_ping_online", ConchainOnline, this);
	Console()->Register("lucent_ping_host", "?i[port]", CFGFLAG_CLIENT, ConHost, this, "Listen for one LAN ping peer (default UDP 18303)");
	Console()->Register("lucent_ping_join", "s[address:port] s[token]", CFGFLAG_CLIENT, ConJoin, this, "Connect to a numeric IPv4/IPv6 LAN ping peer");
	Console()->Register("lucent_ping_leave", "", CFGFLAG_CLIENT, ConLeave, this, "Close the local ping transport");
	Console()->Register("lucent_ping_status", "", CFGFLAG_CLIENT, ConStatus, this, "Show ping transport diagnostics without credentials");
	Console()->Register("lucent_ping", "", CFGFLAG_CLIENT, ConPing, this, "Place a local map ping at the cursor");
}
void CLucentPings::ConPing(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	auto *pGame = pSelf->GameClient();
	if(pSelf->Client()->State() != IClient::STATE_ONLINE ||
		pGame->m_Menus.IsActive() || pGame->m_Chat.IsActive() || pGame->m_GameConsole.IsActive() ||
		pGame->m_Emoticon.IsActive() || pGame->m_Spectator.IsActive() || pGame->m_Motd.IsActive() ||
		pGame->m_ImportantAlert.IsActive() || pGame->m_KeyBinder.IsActive() || pGame->m_Scoreboard.IsActive() || pGame->m_Statboard.IsActive())
		return;
	const double Now = double(time_get()) / time_freq();
	pSelf->UpdateTransport(Now);
	const auto Event = pSelf->m_State.Add(pGame->m_Controls.CursorWorldPos(), Now);
	const int SenderId = LucentPing::LocalSenderClientId(pGame->m_Snap.m_LocalClientId, pGame->m_aLocalIds[0], pGame->m_aLocalIds[1], g_Config.m_ClDummy != 0, pSelf->Client()->DummyConnected());
	if(!LucentPing::ValidSenderClientId(SenderId))
		return;
	if(g_Config.m_ClLucentPingOnline != 0)
		pSelf->m_Relay.SendPing(Event.m_Id, Event.m_Position, SenderId);
	else
		pSelf->m_pTransport->SendPing(Event.m_Id, Event.m_Position, Now, SenderId);
}
void CLucentPings::OnRender()
{
	const double Now = double(time_get()) / time_freq();
	m_State.Update(Now);
	UpdateTransport(Now);
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const auto &Camera = GameClient()->m_Camera;
	const auto World = Graphics()->MapScreenToWorld(Camera.m_Center.x, Camera.m_Center.y, 100, 100, 100, 0, 0, Graphics()->ScreenAspect(), Camera.m_Zoom);
	const auto &Snap = GameClient()->m_Snap;
	const int SpecId = Snap.m_SpecInfo.m_SpectatorId;
	const bool Freeview = Camera.CamType() == CCamera::CAMTYPE_SPEC;
	const auto View = Freeview ? LucentPing::EDistanceView::Freeview : Snap.m_SpecInfo.m_Active ? LucentPing::EDistanceView::Follow :
												      LucentPing::EDistanceView::Gameplay;
	const vec2 *pFollow = nullptr;
	if(Snap.m_SpecInfo.m_Active && SpecId >= 0 && SpecId < MAX_CLIENTS)
	{
		if(Snap.m_aCharacters[SpecId].m_Active)
			pFollow = &GameClient()->m_aClients[SpecId].m_RenderPos;
		else if(Snap.m_SpecInfo.m_UsePosition)
			pFollow = &Snap.m_SpecInfo.m_Position;
	}
	const vec2 Origin = LucentPing::DistanceOrigin(View, Camera.m_Center, Snap.m_pLocalCharacter ? &GameClient()->m_LocalCharacterPos : nullptr, pFollow);
	const auto Senders = LucentPing::ResolveSenders(*GameClient(), m_State);
	LucentPing::Render(Graphics(), TextRender(), RenderTools(), Senders, m_State, Now, World.m_TopLeft, World.Size(), Origin, g_Config.m_ClLucentLogoAnimation != 0);
}

std::optional<LucentPing::CContext> CLucentPings::Context() const
{
	if(Client()->State() != IClient::STATE_ONLINE || !GameClient()->Map()->IsLoaded())
		return std::nullopt;
	NETADDR Server = Client()->ServerAddress();
	Server.type &= NETTYPE_IPV4 | NETTYPE_IPV6;
	if(!Server.port || (Server.type != NETTYPE_IPV4 && Server.type != NETTYPE_IPV6))
		return std::nullopt;
	// Hash a canonical address string, never NETADDR padding or a DNS alias.
	char aAddress[NETADDR_MAXSTRSIZE];
	net_addr_str(&Server, aAddress, sizeof(aAddress), true);
	const auto ServerHash = sha256(aAddress, str_length(aAddress));
	const auto MapHash = GameClient()->Map()->Sha256();
	LucentPing::CContext Result;
	std::copy_n(ServerHash.data, 32, Result.m_Server.begin());
	std::copy_n(MapHash.data, 32, Result.m_Map.begin());
	if(!Result.Valid())
		return std::nullopt;
	return Result;
}
void CLucentPings::UpdateTransport(double Now)
{
	const auto Current = Context();
	m_pTransport->Update(Current ? &*Current : nullptr, Now);
	m_Relay.Configure(g_Config.m_ClLucentPingOnline != 0, g_Config.m_ClLucentPingRelayUrl, Current);
	LucentPing::CReceivedPing Ping;
	while(m_pTransport->Receive(Ping))
		m_State.AddRemote(Ping.m_Id, Ping.m_Position, Ping.m_Received, Ping.m_SenderClientId);
	while(m_Relay.Receive(Ping))
		m_State.AddRemote(Ping.m_Id, Ping.m_Position, Ping.m_Received, Ping.m_SenderClientId);
}
void CLucentPings::PrintStatus()
{
	if(g_Config.m_ClLucentPingOnline != 0)
	{
		const auto Status = m_Relay.Status();
		char aLine[512];
		str_format(aLine, sizeof(aLine), "[ping relay] %s; participants=%u; sent=%llu received=%llu dropped=%llu; %s", LucentPing::TransportStateName(Status.m_State), Status.m_Participants, (unsigned long long)Status.m_Sent, (unsigned long long)Status.m_Received, (unsigned long long)Status.m_Dropped, Status.m_Detail.c_str());
		GameClient()->m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_LOCAL, aLine);
		return;
	}
	const auto Status = m_pTransport->Status();
	char aPeer[NETADDR_MAXSTRSIZE] = "none", aLine[512];
	if(Status.m_Peer.port)
		net_addr_str(&Status.m_Peer, aPeer, sizeof(aPeer), true);
	str_format(aLine, sizeof(aLine), "[ping UDP/relay disabled] %s: %s; participants=%u; port=%u peer=%s; sent=%llu received=%llu dropped=%llu", LucentPing::TransportStateName(Status.m_State), Status.m_pDetail, Status.m_State == LucentPing::ETransportState::Connected ? 2u : Status.m_State == LucentPing::ETransportState::Listening ? 1u :
																																										  0u,
		unsigned(Status.m_ListenPort), aPeer, (unsigned long long)Status.m_SentPings, (unsigned long long)Status.m_ReceivedPings, (unsigned long long)Status.m_Dropped);
	GameClient()->m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_LOCAL, aLine);
}
void CLucentPings::ConHost(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	const auto Current = pSelf->Context();
	const int Port = pResult->NumArguments() ? pResult->GetInteger(0) : 18303;
	if(!Current || Port < 1 || Port > 65535)
	{
		pSelf->GameClient()->m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_LOCAL, "[ping] Host requires an online game with map SHA256 and port 1..65535.");
		return;
	}
	g_Config.m_ClLucentPingOnline = 0;
	pSelf->m_Relay.Configure(false, "", std::nullopt);
	NETADDR Bind{};
	Bind.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	Bind.port = (unsigned short)Port;
	if(pSelf->m_pTransport->Host(Bind, *Current, double(time_get()) / time_freq()))
	{
		const auto Token = LucentPing::FormatToken(pSelf->m_pTransport->HostToken());
		char aLine[160];
		str_format(aLine, sizeof(aLine), "[ping] LAN token: %s (plaintext UDP; share privately)", Token.data());
		// Direct local backlog only: Console()->Print would also write the secret to loggers.
		pSelf->GameClient()->m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_LOCAL, aLine);
	}
	pSelf->PrintStatus();
}
void CLucentPings::ConJoin(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	const auto Current = pSelf->Context();
	NETADDR Peer{};
	LucentPing::CSecret Token{};
	// Numeric addresses avoid blocking DNS on the render/input thread. IPv6 uses [address]:port.
	if(!Current || net_addr_from_str(&Peer, pResult->GetString(0)) != 0 || !Peer.port || !LucentPing::ParseToken(pResult->GetString(1), Token))
	{
		pSelf->GameClient()->m_GameConsole.PrintLine(CGameConsole::CONSOLETYPE_LOCAL, "[ping] Join requires an online game, numeric address:port and 32 hex token characters.");
		return;
	}
	g_Config.m_ClLucentPingOnline = 0;
	pSelf->m_Relay.Configure(false, "", std::nullopt);
	pSelf->m_pTransport->Join(Peer, Token, *Current, double(time_get()) / time_freq());
	pSelf->PrintStatus();
}
void CLucentPings::ConLeave(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	g_Config.m_ClLucentPingOnline = 0;
	pSelf->m_Relay.Configure(false, "", std::nullopt);
	pSelf->m_pTransport->Leave();
	pSelf->PrintStatus();
}
void CLucentPings::ConStatus(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	pSelf->UpdateTransport(double(time_get()) / time_freq());
	pSelf->PrintStatus();
}

void CLucentPings::ConchainOnline(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	// Keep standard config parsing/serialization. Lifecycle resets never overwrite this preference.
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 0)
		return;
	auto *pSelf = static_cast<CLucentPings *>(pUserData);
	pSelf->m_pTransport->Leave();
	pSelf->UpdateTransport(double(time_get()) / time_freq());
	if(!g_Config.m_ClLucentPingOnline)
		pSelf->m_State.ClearRemote();
	pSelf->PrintStatus();
}
