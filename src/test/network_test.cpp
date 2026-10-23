#include <engine/shared/network.h>

#include <gtest/gtest.h>

static int UnpackUncompressedPacket(int Size, CNetPacketConstruct *pPacket, bool AllowDecompression = true)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = 0; // no flags, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, Size, pPacket, Sixup, AllowDecompression);
}

static int UnpackCompressedPacket(CNetPacketConstruct *pPacket, bool AllowDecompression, bool *pDecompressed = nullptr, int PayloadSize = 64)
{
	CNetBase::Init();

	const unsigned char aPayload[4 * NET_MAX_PACKETSIZE] = {};
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = (NET_PACKETFLAG_COMPRESSION << 2) & 0xfc; // compression, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	const int CompressedSize = CNetBase::Compress(aPayload, PayloadSize,
		&aBuffer[NET_PACKETHEADERSIZE], NET_MAX_PACKETSIZE - NET_PACKETHEADERSIZE);
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, NET_PACKETHEADERSIZE + CompressedSize, pPacket, Sixup, AllowDecompression, nullptr, nullptr, pDecompressed);
}

TEST(Network, UnpackMaximumUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData), &Packet), 0);
	EXPECT_EQ(Packet.m_DataSize, (int)sizeof(Packet.m_aChunkData));
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet), 0);
}

TEST(Network, UnpackOversizedUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData) + 1, &Packet), -1);
}

TEST(Network, UnpackCompressedPacket)
{
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed), 0);
	EXPECT_EQ(Packet.m_DataSize, 64);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackCompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	bool Decompressed = true;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, false, &Decompressed), -1);
	EXPECT_FALSE(Decompressed);
}

TEST(Network, UnpackOversizedCompressedPacket)
{
	// Decoding data that does not fit the packet buffer costs the same, so it counts
	// as a decompression all the same.
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed, (int)sizeof(Packet.m_aChunkData) + 1), -1);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackUncompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet, false), 0);
}

TEST(Network, UnpackChunks)
{
	// Three non-vital chunks of different sizes, each filled with its own byte.
	constexpr int NumChunks = 3;
	const int aSizes[NumChunks] = {1, 100, 7};

	CNetPacketConstruct Packet = {};
	Packet.m_NumChunks = NumChunks;
	unsigned char *pData = Packet.m_aChunkData;
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunkHeader Header;
		Header.m_Flags = 0;
		Header.m_Size = aSizes[i];
		Header.m_Sequence = 0;
		pData = Header.Pack(pData);
		memset(pData, i, aSizes[i]);
		pData += aSizes[i];
	}
	Packet.m_DataSize = (int)(pData - Packet.m_aChunkData);

	CNetConnection Connection;
	Connection.m_Sixup = false;

	CPacketChunkUnpacker Unpacker;
	Unpacker.FeedPacket(NETADDR{}, Packet, &Connection, 0);
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunk Chunk;
		ASSERT_TRUE(Unpacker.UnpackNextChunk(&Chunk));
		EXPECT_EQ(Chunk.m_DataSize, aSizes[i]);
		for(int j = 0; j < Chunk.m_DataSize; j++)
		{
			EXPECT_EQ(((const unsigned char *)Chunk.m_pData)[j], i);
		}
	}
	CNetChunk Chunk;
	EXPECT_FALSE(Unpacker.UnpackNextChunk(&Chunk));
}
#include <engine/shared/config.h>
#include <base/net.h>
#include <base/secure.h>
#include <base/time.h>
#include <engine/shared/protocol7.h>
#include <chrono>
#include <thread>

TEST(Network, RetryToken7)
{
	using namespace std::chrono_literals;
	struct CTimeoutScope
	{
		int m_Previous = g_Config.m_ConnTimeout;
		CTimeoutScope() { g_Config.m_ConnTimeout = 100; }
		~CTimeoutScope() { g_Config.m_ConnTimeout = m_Previous; }
	} TimeoutScope;
	NETADDR Peer{};
	ASSERT_EQ(net_addr_from_str(&Peer, "127.0.0.1"), 0);
	NETSOCKET Server = nullptr;
	for(int i = 0; i < 100 && !Server; ++i)
	{
		Peer.port = 20000 + secure_rand_below(40000);
		Server = net_udp_create(Peer);
	}
	ASSERT_NE(Server, nullptr);
	NETADDR Bind{};
	Bind.type = NETTYPE_IPV4;
	NETSOCKET Socket = net_udp_create(Bind);
	ASSERT_NE(Socket, nullptr);
	CNetConnection Connection;
	Connection.Init(Socket, false);
	ASSERT_EQ(Connection.Connect7(&Peer, 1), 0);
	NETADDR From;
	unsigned char *pData;
	ASSERT_EQ(net_socket_read_wait(Server, 1s), 1);
	int Size = net_udp_recv(Server, &From, &pData);
	ASSERT_GT(Size, 0);
	const std::vector<unsigned char> Request(pData, pData + Size);
	// Drop the initial request, then drop the reply to the first retry.
	for(int Loss = 0; Loss < 2; ++Loss)
	{
		set_new_tick();
		Connection.Update();
		EXPECT_LE(net_udp_recv(Server, &From, &pData), 0);
		std::this_thread::sleep_for(550ms);
		set_new_tick();
		Connection.Update();
		ASSERT_EQ(net_socket_read_wait(Server, 1s), 1);
		ASSERT_EQ(net_udp_recv(Server, &From, &pData), Size);
		EXPECT_EQ(std::vector<unsigned char>(pData, pData + Size), Request);
	}
	CNetPacketConstruct Reply{};
	Reply.m_Flags = NET_PACKETFLAG_CONTROL;
	Reply.m_DataSize = 1;
	Reply.m_aChunkData[0] = protocol7::NET_CTRLMSG_TOKEN;
	NETADDR WrongPeer = Peer;
	++WrongPeer.port;
	Connection.Feed(&Reply, &WrongPeer, Connection.m_Token, 123);
	EXPECT_EQ(Connection.State(), CNetConnection::EState::WANT_TOKEN);
	Connection.Feed(&Reply, &Peer, Connection.m_Token ^ 1, 123);
	EXPECT_EQ(Connection.State(), CNetConnection::EState::WANT_TOKEN);
	Connection.Feed(&Reply, &Peer, Connection.m_Token, 123);
	EXPECT_EQ(Connection.State(), CNetConnection::EState::CONNECT);
	Connection.Disconnect(nullptr);
	EXPECT_EQ(Connection.State(), CNetConnection::EState::OFFLINE);
	net_udp_close(Socket);
	net_udp_close(Server);
}
