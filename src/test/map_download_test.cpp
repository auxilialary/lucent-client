#include "test.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/console.h>
#include <engine/message.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/uuid_manager.h>

#include <generated/protocolglue.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

// Exercise the production handler and repacker, isolating transport and final map
// loading. Real packers, unpackers and file writes are used throughout.
class CMapDownloadHarness
{
public:
	static constexpr int CONN_MAIN = 0;
	bool m_Sixup;
	const char *m_aMapdownloadFilenameTemp = nullptr;
	struct CStorageStub
	{
		IOHANDLE OpenFile(const char *pName, int Flags, int Type) { return io_open(pName, Flags); }
	} m_Storage;
	CStorageStub *Storage() { return &m_Storage; }
	void SendMapRequest();
	IOHANDLE m_MapdownloadFileTemp = nullptr;
	int m_MapdownloadCrc = 12345;
	int m_MapdownloadChunk = 0;
	int m_MapdownloadAmount = 0;
	int m_MapdownloadTotalsize = 1;
	struct
	{
		int m_MapdownloadTotalsize;
		int m_MapDownloadChunkSize;
		int m_MapDownloadChunksPerRequest;
	} m_TranslationContext;
	IConsole *m_pConsole = nullptr; // Debug logging disabled by the fixture.
	bool m_Finished = false;
	bool m_Disconnected = false;
	enum class EIoFailure { NONE, WRITE, ERROR, CLOSE };
	EIoFailure m_IoFailure = EIoFailure::NONE;
	unsigned Write(IOHANDLE File, const void *pData, unsigned Size)
	{
		return m_IoFailure == EIoFailure::WRITE ? 0 : io_write(File, pData, Size);
	}
	int Error(IOHANDLE File) { return m_IoFailure == EIoFailure::ERROR ? 1 : io_error(File); }
	int Close(IOHANDLE File)
	{
		const int Result = io_close(File);
		return m_IoFailure == EIoFailure::CLOSE ? -1 : Result;
	}
	void DisconnectWithReason(const char *)
	{
		m_Disconnected = true;
		if(m_MapdownloadFileTemp)
			io_close(m_MapdownloadFileTemp);
		m_MapdownloadFileTemp = nullptr;
		(void)fs_remove(m_aMapdownloadFilenameTemp);
	}
	std::vector<std::vector<unsigned char>> m_Messages;

	bool IsSixup() const { return m_Sixup; }
	void FinishMapDownload() { m_Finished = true; }
	void Receive(CUnpacker &Unpacker);
	void SendMsg(int Conn, CMsgPacker *pMsg, int Flags);
	~CMapDownloadHarness()
	{
		if(m_MapdownloadFileTemp)
			io_close(m_MapdownloadFileTemp);
	}
};

#define io_write Write
#define io_error Error
#define io_close Close
#include <generated/map_download_test.inc>
#undef io_write
#undef io_error
#undef io_close

void CMapDownloadHarness::SendMsg(int Conn, CMsgPacker *pMsg, int Flags)
{
	EXPECT_EQ(Conn, CONN_MAIN);
	EXPECT_EQ(Flags, MSGFLAG_VITAL | MSGFLAG_FLUSH);
	CPacker Wire;
	ASSERT_TRUE(RepackMsg(pMsg, Wire, IsSixup()));
	ASSERT_FALSE(Wire.Error());
	m_Messages.emplace_back(Wire.Data(), Wire.Data() + Wire.Size());
}

static void TestMapDownload(int Window)
{
	constexpr int ChunkSize = 31;
	for(bool Sixup : {false, true})
	{
		for(int Total : {1, Window * ChunkSize - 1, Window * ChunkSize, Window * ChunkSize + 1, 2 * Window * ChunkSize + 13})
		{
			SCOPED_TRACE(::testing::Message() << "sixup=" << Sixup << " window=" << Window << " bytes=" << Total);
			CTestInfo Info;
			Info.Filename(Info.m_aFilename, sizeof(Info.m_aFilename), ".map");
			CMapDownloadHarness Client;
			Client.m_Sixup = Sixup;
			Client.m_TranslationContext = {Total, ChunkSize, Window};
			Client.m_MapdownloadTotalsize = Total;
			Client.m_aMapdownloadFilenameTemp = Info.m_aFilename;
			Client.SendMapRequest();
			ASSERT_NE(Client.m_MapdownloadFileTemp, nullptr);
			std::vector<unsigned char> Expected(Total);
			for(int i = 0; i < Total; ++i)
				Expected[i] = (i * 97 + 11) & 0xff;

			const int Chunks = (Total + ChunkSize - 1) / ChunkSize;
			for(int Chunk = 0, Offset = 0; Chunk < Chunks; ++Chunk)
			{
				int Size = std::min(ChunkSize, Total - Offset);
				CPacker Packet;
				Packet.Reset();
				if(!Sixup)
				{
					Packet.AddInt(Chunk == Chunks - 1);
					Packet.AddInt(Client.m_MapdownloadCrc);
					Packet.AddInt(Chunk);
					Packet.AddInt(Size);
				}
				Packet.AddRaw(Expected.data() + Offset, Size);
				CUnpacker Unpacker;
				Unpacker.Reset(Packet.Data(), Packet.Size());
				Client.Receive(Unpacker);
				Offset += Size;
				const int Requests = Sixup ? std::min(Chunk + 1, Chunks - 1) / Window : std::min(Chunk + 1, Chunks - 1);
				ASSERT_EQ(Client.m_Messages.size(), Requests + 1);
				EXPECT_EQ(Client.m_Finished, Chunk == Chunks - 1);
				EXPECT_EQ(Client.m_MapdownloadAmount, Offset);
				EXPECT_EQ(Client.m_MapdownloadChunk, std::min(Chunk + 1, Chunks - 1));
			}
			for(size_t i = 0; i < Client.m_Messages.size(); ++i)
			{
				const auto &Wire = Client.m_Messages[i];
				CUnpacker Unpacker;
				Unpacker.Reset(Wire.data(), Wire.size());
				int Header = Unpacker.GetInt();
				EXPECT_EQ(Header & 1, 1);
				EXPECT_EQ(Header >> 1, Sixup ? 23 : 19);
				if(Sixup)
				{
					EXPECT_NE(Header >> 1, protocol7::NETMSG_ENTERGAME);
					EXPECT_EQ(Wire.size(), 1);
				}
				else
				{
					EXPECT_EQ(Unpacker.GetInt(), i);
					EXPECT_EQ(Wire.size(), 2);
				}
				EXPECT_FALSE(Unpacker.Error());
			}
			IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_READ);
			ASSERT_NE(File, nullptr);
			std::vector<unsigned char> Actual(Total + 1);
			unsigned Read = io_read(File, Actual.data(), Actual.size());
			io_close(File);
			ASSERT_EQ(Read, Total);
			Actual.resize(Read);
			EXPECT_EQ(Actual, Expected);
			EXPECT_EQ(sha256(Actual.data(), Actual.size()), sha256(Expected.data(), Expected.size()));
			EXPECT_FALSE(fs_remove(Info.m_aFilename));
		}
	}
}

TEST(MapDownload, Window1) { TestMapDownload(1); }
TEST(MapDownload, Window2) { TestMapDownload(2); }
TEST(MapDownload, Window8) { TestMapDownload(8); }

TEST(MapDownload, OpenFailure)
{
	CMapDownloadHarness Client;
	Client.m_aMapdownloadFilenameTemp = "";
	Client.SendMapRequest();
	EXPECT_TRUE(Client.m_Disconnected);
	EXPECT_TRUE(Client.m_Messages.empty());
}

TEST(MapDownload, IoFailures)
{
	for(auto Failure : {CMapDownloadHarness::EIoFailure::WRITE, CMapDownloadHarness::EIoFailure::ERROR, CMapDownloadHarness::EIoFailure::CLOSE})
	{
		CTestInfo Info;
		CMapDownloadHarness Client;
		Client.m_Sixup = false;
		Client.m_aMapdownloadFilenameTemp = Info.m_aFilename;
		Client.m_IoFailure = Failure;
		Client.SendMapRequest();
		ASSERT_NE(Client.m_MapdownloadFileTemp, nullptr);
		CPacker Packet;
		Packet.Reset();
		Packet.AddInt(1);
		Packet.AddInt(Client.m_MapdownloadCrc);
		Packet.AddInt(0);
		Packet.AddInt(1);
		Packet.AddRaw("a", 1);
		CUnpacker Unpacker;
		Unpacker.Reset(Packet.Data(), Packet.Size());
		Client.Receive(Unpacker);
		EXPECT_TRUE(Client.m_Disconnected);
		EXPECT_FALSE(Client.m_Finished);
		EXPECT_EQ(Client.m_MapdownloadFileTemp, nullptr);
		EXPECT_EQ(Client.m_Messages.size(), 1);
		EXPECT_EQ(fs_is_file(Info.m_aFilename), 0);
	}
}

TEST(MapDownload, RejectInvalidLegacySize)
{
	// Overrun, premature Last, and exhausted budget without Last.
	for(int Case = 0; Case < 3; ++Case)
	{
		CTestInfo Info;
		CMapDownloadHarness Client;
		Client.m_Sixup = false;
		Client.m_aMapdownloadFilenameTemp = Info.m_aFilename;
		Client.m_MapdownloadTotalsize = Case == 1 ? 2 : 1;
		Client.m_MapdownloadAmount = Case == 2 ? 1 : 0;
		Client.SendMapRequest();
		CPacker Packet;
		Packet.Reset();
		Packet.AddInt(Case == 1);
		Packet.AddInt(Client.m_MapdownloadCrc);
		Packet.AddInt(0);
		Packet.AddInt(Case == 0 ? 2 : 1);
		Packet.AddRaw("ab", Case == 0 ? 2 : 1);
		CUnpacker Unpacker;
		Unpacker.Reset(Packet.Data(), Packet.Size());
		const int PreviousAmount = Client.m_MapdownloadAmount;
		Client.Receive(Unpacker);
		EXPECT_TRUE(Client.m_Disconnected);
		EXPECT_FALSE(Client.m_Finished);
		EXPECT_EQ(Client.m_MapdownloadAmount, PreviousAmount);
		EXPECT_EQ(Client.m_Messages.size(), 1);
	}
}
