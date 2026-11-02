#include <base/secure.h>

#include <game/client/lucent/game/ping.h>
#include <game/client/lucent/game/ping_protocol.h>
#include <game/client/lucent/game/ping_transport.h>

#include <gtest/gtest.h>

#include <bit>
#include <chrono>
#include <limits>
#include <thread>
using namespace LucentPing;
namespace
{
	CContext GameContext()
	{
		CContext C;
		C.m_Server[0] = 1;
		C.m_Map[0] = 2;
		return C;
	}
	NETADDR Loopback(unsigned short Port)
	{
		NETADDR A{};
		A.type = NETTYPE_IPV4;
		A.ip[0] = 127;
		A.ip[3] = 1;
		A.port = Port;
		return A;
	}
	struct CPair
	{
		std::unique_ptr<IPingTransport> m_Host = CreateDirectPingTransport(), m_Join = CreateDirectPingTransport();
		CContext m_Context = GameContext();
		NETADDR m_Address{};
		double m_Now = 100;
		bool Host()
		{
			for(int i = 0; i < 16; ++i)
			{
				m_Address = Loopback((unsigned short)(40000 + secure_rand_below(20000)));
				if(m_Host->Host(m_Address, m_Context, m_Now))
					return true;
			}
			return false;
		}
		void Pump(int Count = 8)
		{
			for(int i = 0; i < Count; ++i)
			{
				m_Host->Update(&m_Context, m_Now);
				m_Join->Update(&m_Context, m_Now);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				m_Now += .01;
			}
		}
		bool Connect()
		{
			if(!Host() || !m_Join->Join(m_Address, m_Host->HostToken(), m_Context, m_Now))
				return false;
			Pump();
			return m_Host->Status().m_State == ETransportState::Connected && m_Join->Status().m_State == ETransportState::Connected;
		}
	};
	struct CRawPeer
	{
		NETSOCKET m_Socket = net_udp_create(Loopback(0));
		CPacket m_Packet;
		NETADDR m_LastFrom{};
		CRawPeer()
		{
			m_Packet.m_Context = GameContext();
			m_Packet.m_Sequence = 1;
			m_Packet.m_Type = EPacketType::Hello;
			secure_random_fill(m_Packet.m_Nonce.data(), unsigned(m_Packet.m_Nonce.size()));
		}
		~CRawPeer()
		{
			if(m_Socket)
				net_udp_close(m_Socket);
		}
		void Send(const NETADDR &Address, const CWirePacket &Wire)
		{
			ASSERT_TRUE(m_Socket);
			EXPECT_EQ(net_udp_send(m_Socket, &Address, Wire.data(), int(Wire.size())), int(Wire.size()));
		}
		void Send(const NETADDR &Address) { Send(Address, Encode(m_Packet)); }
		bool Read(CPacket &Packet)
		{
			for(int i = 0; i < 20; ++i)
			{
				NETADDR From{};
				unsigned char *pData = nullptr;
				const int Size = net_udp_recv(m_Socket, &From, &pData);
				if(Size > 0)
				{
					m_LastFrom = From;
					return Decode(pData, size_t(Size), Packet) == EDecode::Ok;
				}
				net_socket_read_wait(m_Socket, std::chrono::milliseconds(2));
			}
			return false;
		}
		bool Connect(CPair &Pair)
		{
			if(!Pair.Host() || !m_Socket)
				return false;
			m_Packet.m_Auth = Pair.m_Host->HostToken();
			Send(Pair.m_Address);
			Pair.Pump();
			CPacket Accept;
			if(!Read(Accept) || Accept.m_Type != EPacketType::Accept)
				return false;
			m_Packet.m_Auth = Accept.m_Auth;
			m_Packet.m_Type = EPacketType::Ping;
			m_Packet.m_SenderClientId = 0;
			m_Packet.m_EventId = 1;
			m_Packet.m_Sequence = 2;
			m_Packet.m_Position = vec2(12.5f, -45.125f);
			return true;
		}
	};
}
TEST(LucentPingTransport, FixedWireFormatAndValidation)
{
	CPacket P;
	P.m_Context = GameContext();
	P.m_Type = EPacketType::Ping;
	P.m_SenderClientId = 0;
	P.m_Sequence = 0x0102030405060708ULL;
	P.m_EventId = 0x1020304050607080ULL;
	P.m_Position = vec2(1.f, -2.5f);
	auto Wire = Encode(P);
	EXPECT_EQ(Wire.size(), 128u);
	EXPECT_EQ(Wire[104], 1);
	EXPECT_EQ(Wire[111], 8);
	EXPECT_EQ(Wire[112], 0x10);
	EXPECT_EQ(Wire[119], 0x80);
	EXPECT_EQ(Wire[120], 0x3f);
	EXPECT_EQ(Wire[121], 0x80);
	EXPECT_EQ(Wire[124], 0xc0);
	EXPECT_EQ(Wire[125], 0x20);
	CPacket Decoded;
	ASSERT_EQ(Decode(Wire.data(), Wire.size(), Decoded), EDecode::Ok);
	EXPECT_EQ(Decoded.m_Sequence, P.m_Sequence);
	EXPECT_EQ(Decoded.m_EventId, P.m_EventId);
	EXPECT_EQ(Decoded.m_Position, P.m_Position);
	for(size_t Size = 0; Size < PacketSize; ++Size)
		EXPECT_EQ(Decode(Wire.data(), Size, Decoded), EDecode::Invalid);
	EXPECT_EQ(Decode(Wire.data(), PacketSize + 1, Decoded), EDecode::Invalid);
	for(int Offset : {0, 5, 6, 7})
	{
		auto Bad = Wire;
		Bad[Offset] = 255;
		EXPECT_EQ(Decode(Bad.data(), Bad.size(), Decoded), EDecode::Invalid);
	}
	auto Version = Wire;
	++Version[4];
	EXPECT_EQ(Decode(Version.data(), Version.size(), Decoded), EDecode::Version);
	for(float Bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 16777218.f})
	{
		for(int Axis = 0; Axis < 2; ++Axis)
		{
			P.m_Position = Axis ? vec2(0, Bad) : vec2(Bad, 0);
			auto Bytes = Encode(P);
			EXPECT_EQ(Decode(Bytes.data(), Bytes.size(), Decoded), EDecode::Invalid);
		}
	}
	P.m_Position = vec2(0, 0);
	P.m_EventId = 0;
	Wire = Encode(P);
	EXPECT_EQ(Decode(Wire.data(), Wire.size(), Decoded), EDecode::Invalid);
	P.m_Type = EPacketType::Heartbeat;
	P.m_EventId = 1;
	Wire = Encode(P);
	EXPECT_EQ(Decode(Wire.data(), Wire.size(), Decoded), EDecode::Invalid);
	P.m_EventId = 0;
	P.m_Sequence = 0;
	Wire = Encode(P);
	EXPECT_EQ(Decode(Wire.data(), Wire.size(), Decoded), EDecode::Invalid);
	CSecret Token{};
	EXPECT_FALSE(ParseToken("invalid", Token));
	EXPECT_FALSE(ParseToken("zz000000000000000000000000000000", Token));
	secure_random_fill(Token.data(), unsigned(Token.size()));
	CSecret Parsed{};
	ASSERT_TRUE(ParseToken(FormatToken(Token).data(), Parsed));
	EXPECT_TRUE(SecretEqual(Parsed, Token));
}
TEST(LucentPingTransport, TwoSocketsBidirectionalPrecisionAndNoEcho)
{
	CPair Pair;
	ASSERT_TRUE(Pair.Connect());
	EXPECT_NE(Pair.m_Host->Status().m_Peer.port, Pair.m_Address.port);
	const vec2 A(12345.125f, -9876.5f), B(-.03125f, 16777216.f);
	ASSERT_TRUE(Pair.m_Host->SendPing(10, A, Pair.m_Now, 3));
	ASSERT_TRUE(Pair.m_Join->SendPing(20, B, Pair.m_Now, 7));
	Pair.Pump();
	CReceivedPing Incoming;
	CState Manager;
	ASSERT_TRUE(Pair.m_Join->Receive(Incoming));
	EXPECT_EQ(Incoming.m_Position, A);
	EXPECT_EQ(Incoming.m_SenderClientId, 3);
	EXPECT_EQ(Incoming.m_Id, 10u);
	EXPECT_GE(Incoming.m_Received, 100);
	Manager.AddRemote(Incoming.m_Id, Incoming.m_Position, Incoming.m_Received);
	ASSERT_TRUE(Pair.m_Host->Receive(Incoming));
	EXPECT_EQ(Incoming.m_Position, B);
	EXPECT_EQ(Incoming.m_SenderClientId, 7);
	EXPECT_EQ(Incoming.m_Id, 20u);
	EXPECT_TRUE(Manager.Events().front().m_Remote);
	EXPECT_EQ(Manager.Events().front().m_Id, 10u);
	EXPECT_EQ(Manager.Events().front().Alpha(Manager.Events().front().m_Created), 1);
	Pair.Pump();
	EXPECT_FALSE(Pair.m_Host->Receive(Incoming));
	EXPECT_FALSE(Pair.m_Join->Receive(Incoming));
	EXPECT_EQ(Pair.m_Host->Status().m_SentPings, 1u);
	EXPECT_EQ(Pair.m_Join->Status().m_SentPings, 1u);
}
TEST(LucentPingTransport, WrongTokenAndContextAreRejected)
{
	for(int Case = 0; Case < 3; ++Case)
	{
		CPair Pair;
		ASSERT_TRUE(Pair.Host());
		auto Context = Pair.m_Context;
		auto Token = Pair.m_Host->HostToken();
		if(Case == 0)
			Token[0] ^= 1;
		if(Case == 1)
			Context.m_Map[0] ^= 4;
		if(Case == 2)
			Context.m_Server[0] ^= 4;
		ASSERT_TRUE(Pair.m_Join->Join(Pair.m_Address, Token, Context, Pair.m_Now));
		for(int i = 0; i < 8; ++i)
		{
			Pair.m_Host->Update(&Pair.m_Context, Pair.m_Now);
			Pair.m_Join->Update(&Context, Pair.m_Now);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Error);
		EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Listening);
	}
	CPair Pair;
	auto Missing = Pair.m_Context;
	Missing.m_Map.fill(0);
	EXPECT_FALSE(Pair.m_Host->Host(Loopback(18303), Missing, 100));
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Error);
}
TEST(LucentPingTransport, WrongProtocolAndMalformedHandshake)
{
	CPair Pair;
	ASSERT_TRUE(Pair.Host());
	CRawPeer Raw;
	ASSERT_TRUE(Raw.m_Socket);
	Raw.m_Packet.m_Auth = Pair.m_Host->HostToken();
	auto Version = Encode(Raw.m_Packet);
	Version[4]++;
	Raw.Send(Pair.m_Address, Version);
	Pair.Pump();
	CPacket Reply;
	ASSERT_TRUE(Raw.Read(Reply));
	EXPECT_EQ(Reply.m_Type, EPacketType::Reject);
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Listening);
	auto Bad = Encode(Raw.m_Packet);
	Bad[0] = 0;
	Raw.Send(Pair.m_Address, Bad);
	ASSERT_EQ(net_udp_send(Raw.m_Socket, &Pair.m_Address, Bad.data(), 12), 12);
	std::array<uint8_t, 129> Oversize{};
	ASSERT_EQ(net_udp_send(Raw.m_Socket, &Pair.m_Address, Oversize.data(), 129), 129);
	Pair.Pump();
	EXPECT_GE(Pair.m_Host->Status().m_Dropped, 4u);
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Listening);
}
TEST(LucentPingTransport, ReplayDuplicateSourceAndSessionChecks)
{
	CPair Pair;
	CRawPeer Raw;
	ASSERT_TRUE(Raw.Connect(Pair));
	Raw.Send(Pair.m_Address);
	Pair.Pump();
	CReceivedPing Ping;
	ASSERT_TRUE(Pair.m_Host->Receive(Ping));
	Raw.Send(Pair.m_Address); // exact replay
	++Raw.m_Packet.m_Sequence;
	Raw.Send(Pair.m_Address); // same event ID with a fresh sequence
	++Raw.m_Packet.m_Sequence;
	++Raw.m_Packet.m_EventId;
	Raw.m_Packet.m_Nonce[0] ^= 1;
	Raw.Send(Pair.m_Address);
	Raw.m_Packet.m_Nonce[0] ^= 1;
	++Raw.m_Packet.m_Sequence;
	Raw.m_Packet.m_Context.m_Map[0] ^= 4;
	Raw.Send(Pair.m_Address);
	Raw.m_Packet.m_Context.m_Map[0] ^= 4;
	++Raw.m_Packet.m_Sequence;
	Raw.m_Packet.m_Auth[0] ^= 1;
	Raw.Send(Pair.m_Address);
	Raw.m_Packet.m_Auth[0] ^= 1;
	CRawPeer Third;
	Third.m_Packet = Raw.m_Packet;
	Third.Send(Pair.m_Address);
	Pair.Pump();
	EXPECT_FALSE(Pair.m_Host->Receive(Ping));
	EXPECT_GE(Pair.m_Host->Status().m_Dropped, 6u);
	++Raw.m_Packet.m_Sequence;
	Raw.Send(Pair.m_Address);
	Pair.Pump();
	EXPECT_TRUE(Pair.m_Host->Receive(Ping));
}
TEST(LucentPingTransport, IncomingFloodAndBoundedQueue)
{
	CPair Pair;
	CRawPeer Raw;
	ASSERT_TRUE(Raw.Connect(Pair));
	for(int i = 0; i < 100; ++i)
	{
		Raw.m_Packet.m_EventId = uint64_t(i + 1);
		Raw.m_Packet.m_Sequence = uint64_t(i + 2);
		Raw.Send(Pair.m_Address);
	}
	// Same monotonic timestamp: bounded receive work, at most burst=4 accepted.
	for(int i = 0; i < 4; ++i)
		Pair.m_Host->Update(&Pair.m_Context, Pair.m_Now);
	EXPECT_LE(Pair.m_Host->Status().m_ReceivedPings, 4u);
	EXPECT_GT(Pair.m_Host->Status().m_Dropped, 0u);
	for(int Batch = 0; Batch < 4; ++Batch)
	{
		Pair.m_Now += 1;
		for(int i = 0; i < 4; ++i)
		{
			++Raw.m_Packet.m_EventId;
			++Raw.m_Packet.m_Sequence;
			Raw.Send(Pair.m_Address);
		}
		Pair.m_Host->Update(&Pair.m_Context, Pair.m_Now);
	}
	CReceivedPing Ping;
	int Count = 0;
	while(Pair.m_Host->Receive(Ping))
		++Count;
	EXPECT_LE(Count, 8);
	EXPECT_GE(Count, 4);
}
TEST(LucentPingTransport, OutgoingRateLimitAndManagerLimit)
{
	CPair Pair;
	ASSERT_TRUE(Pair.Connect());
	int Sent = 0;
	for(uint64_t i = 1; i <= 20; ++i)
		Sent += Pair.m_Host->SendPing(i, vec2(float(i), 0), Pair.m_Now);
	EXPECT_EQ(Sent, 4);
	Pair.Pump();
	CState Manager;
	CReceivedPing Ping;
	while(Pair.m_Join->Receive(Ping))
		Manager.AddRemote(Ping.m_Id, Ping.m_Position, Ping.m_Received);
	for(int Batch = 0; Batch < 3; ++Batch)
	{
		Pair.m_Now += 1;
		for(int i = 0; i < 4; ++i)
			EXPECT_TRUE(Pair.m_Host->SendPing(100 + Batch * 4 + i, vec2(float(i), 0), Pair.m_Now));
		Pair.Pump();
		while(Pair.m_Join->Receive(Ping))
			Manager.AddRemote(Ping.m_Id, Ping.m_Position, Ping.m_Received);
	}
	EXPECT_EQ(Manager.Events().size(), 8u);
	EXPECT_GE(Manager.Events().front().m_Id, 104u);
	Manager.Clear();
	EXPECT_TRUE(Manager.Events().empty());
}
TEST(LucentPingTransport, HeartbeatTimeoutDisconnectAndReconnect)
{
	CPair Pair;
	ASSERT_TRUE(Pair.Connect());
	for(int i = 0; i < 12; ++i)
	{
		Pair.m_Now += 1;
		Pair.Pump();
	}
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Connected);
	EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Connected);
	Pair.m_Join->Leave();
	Pair.Pump();
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Disconnected);
	ASSERT_TRUE(Pair.Connect());
	Pair.m_Now += 9;
	Pair.m_Host->Update(&Pair.m_Context, Pair.m_Now);
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Error);
	Pair.m_Join->Leave();
	ASSERT_TRUE(Pair.Connect());
	EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Connected);
}
TEST(LucentPingTransport, ConnectionTimeoutAndLostAcceptRetry)
{
	CPair Pair;
	CRawPeer Silent;
	ASSERT_TRUE(Pair.Host());
	// Close the host; joining its explicitly selected loopback address cannot complete.
	const auto Token = Pair.m_Host->HostToken();
	Pair.m_Host->Leave();
	ASSERT_TRUE(Pair.m_Join->Join(Pair.m_Address, Token, Pair.m_Context, Pair.m_Now));
	Pair.m_Now += 5.1;
	Pair.m_Join->Update(&Pair.m_Context, Pair.m_Now);
	EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Error);
	CRawPeer Raw;
	ASSERT_TRUE(Pair.Host());
	Raw.m_Packet.m_Auth = Pair.m_Host->HostToken();
	Raw.Send(Pair.m_Address);
	Pair.Pump();
	CPacket First;
	ASSERT_TRUE(Raw.Read(First));
	ASSERT_EQ(First.m_Type, EPacketType::Accept);
	++Raw.m_Packet.m_Sequence;
	Raw.Send(Pair.m_Address);
	Pair.Pump();
	CPacket Second;
	ASSERT_TRUE(Raw.Read(Second));
	EXPECT_EQ(Second.m_Type, EPacketType::Accept);
	EXPECT_EQ(Second.m_Auth, First.m_Auth);
	EXPECT_GT(Second.m_Sequence, First.m_Sequence);
}
TEST(LucentPingTransport, GameContextChangeClosesAndClearsPending)
{
	for(int Case = 0; Case < 3; ++Case)
	{
		CPair Pair;
		ASSERT_TRUE(Pair.Connect());
		ASSERT_TRUE(Pair.m_Host->SendPing(1, vec2(1, 2), Pair.m_Now));
		Pair.Pump();
		auto Changed = Pair.m_Context;
		if(Case == 0)
			Changed.m_Map[0] ^= 4;
		if(Case == 1)
			Changed.m_Server[0] ^= 4;
		Pair.m_Join->Update(Case == 2 ? nullptr : &Changed, Pair.m_Now);
		EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Disconnected);
		CReceivedPing Ping;
		EXPECT_FALSE(Pair.m_Join->Receive(Ping));
		EXPECT_EQ(Pair.m_Join->HostToken(), CSecret{});
		Pair.Pump();
		EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Disconnected);
		// Socket was released and credentials rotated; explicit reconnect works.
		ASSERT_TRUE(Pair.Connect());
	}
}

TEST(LucentPingTransport, JoinRetriesHelloWithSameNonceAndFreshSequence)
{
	CPair Pair;
	CRawPeer Relay;
	ASSERT_TRUE(Relay.m_Socket);
	// Use a known listening raw socket as the host and deliberately discard the first HELLO.
	// Bind a new socket to a chosen port without contacting any non-loopback address.
	net_udp_close(Relay.m_Socket);
	Relay.m_Socket = nullptr;
	NETADDR Host{};
	for(int i = 0; i < 16 && !Relay.m_Socket; ++i)
	{
		Host = Loopback((unsigned short)(40000 + secure_rand_below(20000)));
		Relay.m_Socket = net_udp_create(Host);
	}
	ASSERT_TRUE(Relay.m_Socket);
	CSecret Token{};
	secure_random_fill(Token.data(), unsigned(Token.size()));
	ASSERT_TRUE(Pair.m_Join->Join(Host, Token, Pair.m_Context, Pair.m_Now));
	CPacket First;
	ASSERT_TRUE(Relay.Read(First));
	ASSERT_EQ(First.m_Type, EPacketType::Hello);
	Pair.m_Now += .6;
	Pair.m_Join->Update(&Pair.m_Context, Pair.m_Now);
	CPacket Retry;
	ASSERT_TRUE(Relay.Read(Retry));
	EXPECT_EQ(Retry.m_Nonce, First.m_Nonce);
	EXPECT_GT(Retry.m_Sequence, First.m_Sequence);
	EXPECT_EQ(Retry.m_Auth, Token);
	CPacket Accept;
	Accept.m_Type = EPacketType::Accept;
	Accept.m_Nonce = Retry.m_Nonce;
	Accept.m_Context = Pair.m_Context;
	Accept.m_Sequence = 1;
	Accept.m_Auth[0] = 42;
	Relay.Send(Relay.m_LastFrom, Encode(Accept));
	Pair.Pump();
	EXPECT_EQ(Pair.m_Join->Status().m_State, ETransportState::Connected);
}
TEST(LucentPingTransport, ThirdParticipantCannotReplacePeerAndOldSessionCannotReplay)
{
	CPair Pair;
	ASSERT_TRUE(Pair.Host());
	const auto Token = Pair.m_Host->HostToken();
	ASSERT_TRUE(Pair.m_Join->Join(Pair.m_Address, Token, Pair.m_Context, Pair.m_Now));
	Pair.Pump();
	ASSERT_EQ(Pair.m_Host->Status().m_State, ETransportState::Connected);
	const auto Peer = Pair.m_Host->Status().m_Peer;
	auto Third = CreateDirectPingTransport();
	ASSERT_TRUE(Third->Join(Pair.m_Address, Token, Pair.m_Context, Pair.m_Now));
	for(int i = 0; i < 6; ++i)
	{
		Pair.Pump();
		Third->Update(&Pair.m_Context, Pair.m_Now);
		Pair.m_Now += 1;
	}
	EXPECT_EQ(Third->Status().m_State, ETransportState::Error);
	const auto CurrentPeer = Pair.m_Host->Status().m_Peer;
	EXPECT_EQ(net_addr_comp(&Peer, &CurrentPeer), 0);
	Pair.m_Join->Leave();
	Pair.Pump();
	Pair.m_Host->Leave();
	ASSERT_TRUE(Pair.m_Host->Host(Pair.m_Address, Pair.m_Context, Pair.m_Now));
	EXPECT_NE(Pair.m_Host->HostToken(), Token);
	ASSERT_TRUE(Third->Join(Pair.m_Address, Token, Pair.m_Context, Pair.m_Now));
	for(int i = 0; i < 8; ++i)
	{
		Pair.Pump();
		Third->Update(&Pair.m_Context, Pair.m_Now);
	}
	EXPECT_EQ(Third->Status().m_State, ETransportState::Error);
	EXPECT_EQ(Pair.m_Host->Status().m_State, ETransportState::Listening);
}
