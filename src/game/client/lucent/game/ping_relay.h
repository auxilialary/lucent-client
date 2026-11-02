#ifndef GAME_CLIENT_LUCENT_GAME_PING_RELAY_H
#define GAME_CLIENT_LUCENT_GAME_PING_RELAY_H
#include "ping_transport.h"

#include <array>
#include <optional>
#include <string>
namespace LucentPing
{
	constexpr size_t RelayPacketSize = 32;
	struct CRelayFrame
	{
		uint8_t m_Type = 1;
		uint64_t m_Sender = 0, m_Id = 0;
		vec2 m_Position = vec2(0, 0);
		int m_SenderClientId = -1;
	};
	using CRelayBytes = std::array<uint8_t, RelayPacketSize>;
	CRelayBytes EncodeRelay(const CRelayFrame &Frame);
	bool DecodeRelay(const uint8_t *pData, size_t Size, CRelayFrame &Frame);
	bool ValidRelayEndpoint(const std::string &Endpoint, bool AllowLoopbackTest = false);
	double RelayReconnectDelay(unsigned Attempt);
	struct CRelayStatus
	{
		ETransportState m_State = ETransportState::Disconnected;
		unsigned m_Participants = 0;
		uint64_t m_Sent = 0, m_Received = 0, m_Dropped = 0, m_Reconnects = 0;
		std::string m_Detail = "Disabled";
	};
	// Configure/Send/Receive/Status only touch bounded queues, never do network I/O.
	// Server/map are unverified routing claims; they do not establish player identity.
	class CRelayTransport
	{
		class CImpl;
		std::unique_ptr<CImpl> m_pImpl;

	public:
		explicit CRelayTransport(bool AllowLoopbackTest = false);
		~CRelayTransport();
		void Configure(bool Enabled, const std::string &Endpoint, const std::optional<CContext> &Context);
		bool SendPing(uint64_t Id, vec2 Position, int SenderClientId = 0);
		bool Receive(CReceivedPing &Ping);
		CRelayStatus Status() const;
	};
}
#endif
