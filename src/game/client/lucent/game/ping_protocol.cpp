#include "ping_protocol.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
namespace LucentPing
{
	static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
	bool CContext::Valid() const
	{
		return std::any_of(m_Server.begin(), m_Server.end(), [](uint8_t B) { return B != 0; }) && std::any_of(m_Map.begin(), m_Map.end(), [](uint8_t B) { return B != 0; });
	}
	bool ValidPosition(vec2 P) { return std::isfinite(P.x) && std::isfinite(P.y) && std::abs(P.x) <= 16777216.f && std::abs(P.y) <= 16777216.f; }
	bool SecretEqual(const CSecret &A, const CSecret &B)
	{
		unsigned Difference = 0;
		for(size_t i = 0; i < A.size(); ++i)
			Difference |= A[i] ^ B[i];
		return Difference == 0;
	}
	bool ParseToken(const char *pText, CSecret &Token)
	{
		if(std::strlen(pText) != 32)
			return false;
		const auto Hex = [](char C) { return C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 :
									      C >= 'A' && C <= 'F'         ? C - 'A' + 10 :
													     -1; };
		CSecret Parsed{};
		for(size_t i = 0; i < Parsed.size(); ++i)
		{
			const int A = Hex(pText[i * 2]), B = Hex(pText[i * 2 + 1]);
			if(A < 0 || B < 0)
				return false;
			Parsed[i] = uint8_t(A * 16 + B);
		}
		Token = Parsed;
		return true;
	}
	std::array<char, 33> FormatToken(const CSecret &Token)
	{
		std::array<char, 33> Text{};
		constexpr char Hex[] = "0123456789abcdef";
		for(size_t i = 0; i < Token.size(); ++i)
		{
			Text[i * 2] = Hex[Token[i] >> 4];
			Text[i * 2 + 1] = Hex[Token[i] & 15];
		}
		return Text;
	}
	static void Write(uint8_t *pDst, uint64_t Value, size_t Bytes)
	{
		for(size_t i = 0; i < Bytes; ++i)
			pDst[Bytes - 1 - i] = uint8_t(Value >> (8 * i));
	}
	static uint64_t Read(const uint8_t *pSrc, size_t Bytes)
	{
		uint64_t Value = 0;
		for(size_t i = 0; i < Bytes; ++i)
			Value = (Value << 8) | pSrc[i];
		return Value;
	}
	CWirePacket Encode(const CPacket &P)
	{
		CWirePacket Data{};
		std::memcpy(Data.data(), "LPNG", 4);
		Data[4] = ProtocolVersion;
		Data[5] = uint8_t(P.m_Type);
		Write(Data.data() + 6, uint16_t(P.m_SenderClientId + 1), 2);
		std::copy(P.m_Auth.begin(), P.m_Auth.end(), Data.begin() + 8);
		std::copy(P.m_Nonce.begin(), P.m_Nonce.end(), Data.begin() + 24);
		std::copy(P.m_Context.m_Server.begin(), P.m_Context.m_Server.end(), Data.begin() + 40);
		std::copy(P.m_Context.m_Map.begin(), P.m_Context.m_Map.end(), Data.begin() + 72);
		Write(Data.data() + 104, P.m_Sequence, 8);
		Write(Data.data() + 112, P.m_EventId, 8);
		Write(Data.data() + 120, std::bit_cast<uint32_t>(P.m_Position.x), 4);
		Write(Data.data() + 124, std::bit_cast<uint32_t>(P.m_Position.y), 4);
		return Data;
	}
	EDecode Decode(const uint8_t *pData, size_t Size, CPacket &P)
	{
		if(Size != PacketSize || std::memcmp(pData, "LPNG", 4) || pData[5] < 1 || pData[5] > 6)
			return EDecode::Invalid;
		P.m_Type = EPacketType(pData[5]);
		P.m_SenderClientId = int(Read(pData + 6, 2)) - 1;
		std::copy_n(pData + 8, 16, P.m_Auth.begin());
		std::copy_n(pData + 24, 16, P.m_Nonce.begin());
		std::copy_n(pData + 40, 32, P.m_Context.m_Server.begin());
		std::copy_n(pData + 72, 32, P.m_Context.m_Map.begin());
		P.m_Sequence = Read(pData + 104, 8);
		P.m_EventId = Read(pData + 112, 8);
		P.m_Position = vec2(std::bit_cast<float>(uint32_t(Read(pData + 120, 4))), std::bit_cast<float>(uint32_t(Read(pData + 124, 4))));
		if(!P.m_Sequence || !P.m_Context.Valid() || !ValidPosition(P.m_Position))
			return EDecode::Invalid;
		if(P.m_Type == EPacketType::Ping ? !P.m_EventId || !ValidSenderClientId(P.m_SenderClientId) : P.m_EventId != 0 || P.m_Position != vec2(0, 0) || P.m_SenderClientId != -1)
			return EDecode::Invalid;
		return pData[4] == ProtocolVersion ? EDecode::Ok : EDecode::Version;
	}
}
