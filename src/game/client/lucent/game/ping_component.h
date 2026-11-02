#ifndef GAME_CLIENT_LUCENT_GAME_PING_COMPONENT_H
#define GAME_CLIENT_LUCENT_GAME_PING_COMPONENT_H
#include "ping.h"
#include "ping_relay.h"
#include "ping_transport.h"

#include <engine/console.h>

#include <game/client/component.h>

#include <optional>
class CLucentPings : public CComponent
{
	LucentPing::CState m_State;
	LucentPing::CRelayTransport m_Relay;
	static void ConchainOnline(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	std::unique_ptr<LucentPing::IPingTransport> m_pTransport = LucentPing::CreateDirectPingTransport();
	std::optional<LucentPing::CContext> Context() const;
	void UpdateTransport(double Now);
	void PrintStatus();
	static void ConHost(IConsole::IResult *pResult, void *pUserData);
	static void ConJoin(IConsole::IResult *pResult, void *pUserData);
	static void ConLeave(IConsole::IResult *pResult, void *pUserData);
	static void ConStatus(IConsole::IResult *pResult, void *pUserData);
	static void ConPing(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnRender() override;
	void OnReset() override
	{
		m_Relay.Configure(false, "", std::nullopt);
		m_pTransport->Leave();
		m_State.Clear();
	}
	void OnMapLoad() override { OnReset(); }
	void OnShutdown() override { OnReset(); }
	void OnStateChange(int NewState, int OldState) override { OnReset(); }
};
#endif
