#ifndef GAME_CLIENT_LUCENT_GAME_PING_TRANSPORT_H
#define GAME_CLIENT_LUCENT_GAME_PING_TRANSPORT_H
#include "ping_protocol.h"

#include <base/net.h>

#include <memory>
namespace LucentPing
{
	enum class ETransportState
	{
		Disconnected,
		Listening,
		Connecting,
		Connected,
		Error
	};
	struct CTransportStatus
	{
		ETransportState m_State = ETransportState::Disconnected;
		NETADDR m_Peer{};
		unsigned short m_ListenPort = 0;
		uint64_t m_SentPings = 0, m_ReceivedPings = 0, m_Dropped = 0;
		const char *m_pDetail = "Closed";
	};
	struct CReceivedPing
	{
		uint64_t m_Id;
		vec2 m_Position;
		double m_Received;
		int m_SenderClientId = -1;
	};
	// No rendering or game-client dependencies. A later secure ICE transport implements this boundary.
	class IPingTransport
	{
	public:
		virtual ~IPingTransport() = default;
		virtual bool Host(NETADDR Bind, const CContext &Context, double Now) = 0;
		virtual bool Join(NETADDR Peer, const CSecret &Token, const CContext &Context, double Now) = 0;
		virtual void Leave() = 0;
		// nullptr/changed context closes the socket, clears pending events and invalidates credentials.
		virtual void Update(const CContext *pContext, double Now) = 0;
		virtual bool SendPing(uint64_t Id, vec2 Position, double Now, int SenderClientId = 0) = 0;
		virtual bool Receive(CReceivedPing &Ping) = 0;
		virtual CTransportStatus Status() const = 0;
		// Only explicitly hosting exposes the token to the local console, never Status/logging.
		virtual CSecret HostToken() const = 0;
	};
	const char *TransportStateName(ETransportState State);
	std::unique_ptr<IPingTransport> CreateDirectPingTransport();
}
#endif
