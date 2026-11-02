#ifndef GAME_CLIENT_LUCENT_GAME_PING_PROTOCOL_H
#define GAME_CLIENT_LUCENT_GAME_PING_PROTOCOL_H
#include "ping_identity.h"

#include <base/vmath.h>

#include <array>
#include <cstddef>
#include <cstdint>
namespace LucentPing
{
	// LAN MVP: plaintext bearer credentials, NOT an encrypted/authenticated Internet transport.
	// All integers and IEEE-754 float32 bits are big-endian. Every datagram is exactly 128 bytes.
	constexpr size_t PacketSize = 128;
	constexpr uint8_t ProtocolVersion = 2;
	using CSecret = std::array<uint8_t, 16>;
	struct CContext
	{
		std::array<uint8_t, 32> m_Server{}, m_Map{};
		bool Valid() const;
		bool operator==(const CContext &) const = default;
	};
	enum class EPacketType : uint8_t
	{
		Hello = 1,
		Accept,
		Reject,
		Ping,
		Heartbeat,
		Disconnect
	};
	struct CPacket
	{
		EPacketType m_Type = EPacketType::Heartbeat;
		CSecret m_Auth{}, m_Nonce{};
		CContext m_Context;
		uint64_t m_Sequence = 0, m_EventId = 0;
		vec2 m_Position = vec2(0, 0);
		int m_SenderClientId = -1;
	};
	enum class EDecode
	{
		Ok,
		Version,
		Invalid
	};
	using CWirePacket = std::array<uint8_t, PacketSize>;
	CWirePacket Encode(const CPacket &Packet);
	EDecode Decode(const uint8_t *pData, size_t Size, CPacket &Packet);
	bool ValidPosition(vec2 Position);
	bool SecretEqual(const CSecret &A, const CSecret &B);
	bool ParseToken(const char *pText, CSecret &Token);
	std::array<char, 33> FormatToken(const CSecret &Token);
}
#endif
