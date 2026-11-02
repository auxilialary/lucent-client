#include "ping_relay.h"

#include <algorithm>
#include <bit>
#include <cstring>
namespace LucentPing
{
	namespace
	{
		void Put(uint8_t *p, uint64_t Value, size_t N)
		{
			for(size_t i = 0; i < N; ++i)
				p[N - 1 - i] = uint8_t(Value >> (i * 8));
		}
		uint64_t Get(const uint8_t *p, size_t N)
		{
			uint64_t V = 0;
			for(size_t i = 0; i < N; ++i)
				V = (V << 8) | p[i];
			return V;
		}
	}
	CRelayBytes EncodeRelay(const CRelayFrame &Frame)
	{
		CRelayBytes Bytes{};
		std::memcpy(Bytes.data(), "LPR2", 4);
		Bytes[4] = Frame.m_Type;
		Put(Bytes.data() + 6, uint16_t(Frame.m_SenderClientId + 1), 2);
		Put(Bytes.data() + 8, Frame.m_Sender, 8);
		Put(Bytes.data() + 16, Frame.m_Id, 8);
		Put(Bytes.data() + 24, std::bit_cast<uint32_t>(Frame.m_Position.x), 4);
		Put(Bytes.data() + 28, std::bit_cast<uint32_t>(Frame.m_Position.y), 4);
		return Bytes;
	}
	bool DecodeRelay(const uint8_t *p, size_t Size, CRelayFrame &Frame)
	{
		if(Size != RelayPacketSize || std::memcmp(p, "LPR2", 4) || p[5])
			return false;
		Frame.m_Type = p[4];
		Frame.m_SenderClientId = int(Get(p + 6, 2)) - 1;
		Frame.m_Sender = Get(p + 8, 8);
		Frame.m_Id = Get(p + 16, 8);
		Frame.m_Position = vec2(std::bit_cast<float>(uint32_t(Get(p + 24, 4))), std::bit_cast<float>(uint32_t(Get(p + 28, 4))));
		if(!ValidPosition(Frame.m_Position))
			return false;
		if(Frame.m_Type == 1)
			return Frame.m_Id != 0 && ValidSenderClientId(Frame.m_SenderClientId);
		return Frame.m_Type == 2 && Frame.m_SenderClientId == -1 && Frame.m_Sender == 0 && Frame.m_Id >= 1 && Frame.m_Id <= 64 && Frame.m_Position == vec2(0, 0);
	}
	bool ValidRelayEndpoint(const std::string &Endpoint, bool AllowLoopbackTest)
	{
		if(Endpoint.size() > 480 || Endpoint.find_first_of("?#@\\\r\n") != std::string::npos || !Endpoint.ends_with("/v2"))
			return false;
		if(Endpoint.starts_with("wss://") && Endpoint.size() > 9)
			return true;
		return AllowLoopbackTest && (Endpoint.starts_with("ws://127.0.0.1:") || Endpoint.starts_with("ws://[::1]:"));
	}
	double RelayReconnectDelay(unsigned Attempt) { return std::min(30.0, double(1u << std::min(Attempt, 5u))); }
}
