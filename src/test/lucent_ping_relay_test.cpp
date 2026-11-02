#include "test.h"

#include <base/secure.h>
#include <base/time.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/lucent/game/ping.h>
#include <game/client/lucent/game/ping_relay.h>
#include <game/client/lucent/game/ping_sender.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
using namespace LucentPing;
namespace
{
	CContext Context()
	{
		CContext C;
		C.m_Server[0] = 1;
		C.m_Map[0] = 2;
		return C;
	}
	template<class F>
	bool Wait(F Predicate)
	{
		const auto Until = std::chrono::steady_clock::now() + std::chrono::seconds(8);
		do
		{
			if(Predicate())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		} while(std::chrono::steady_clock::now() < Until);
		return false;
	}
}
TEST(LucentPingRelay, ProtocolEndpointAndBackoff)
{
	CRelayFrame F{1, 0, 42, vec2(12345.125f, -.03125f), 0}, Decoded;
	const auto Data = EncodeRelay(F);
	ASSERT_TRUE(DecodeRelay(Data.data(), Data.size(), Decoded));
	EXPECT_EQ(Decoded.m_Position, F.m_Position);
	EXPECT_EQ(Decoded.m_Id, 42u);
	EXPECT_EQ(Data[0], 'L');
	EXPECT_EQ(Data[23], 42);
	for(size_t N = 0; N < RelayPacketSize; ++N)
		EXPECT_FALSE(DecodeRelay(Data.data(), N, Decoded));
	auto Bad = Data;
	Bad[5] = 1;
	EXPECT_FALSE(DecodeRelay(Bad.data(), Bad.size(), Decoded));
	F.m_Position.x = std::numeric_limits<float>::quiet_NaN();
	Bad = EncodeRelay(F);
	EXPECT_FALSE(DecodeRelay(Bad.data(), Bad.size(), Decoded));
	EXPECT_STREQ(DefaultConfig::ClLucentPingRelayUrl, "wss://lucent-ping-relay.apolemia.workers.dev/v2");
	EXPECT_TRUE(ValidRelayEndpoint(DefaultConfig::ClLucentPingRelayUrl));
	EXPECT_FALSE(ValidRelayEndpoint(""));
	EXPECT_FALSE(ValidRelayEndpoint("ws://127.0.0.1:8888/v2"));
	EXPECT_TRUE(ValidRelayEndpoint("ws://127.0.0.1:8888/v2", true));
	EXPECT_FALSE(ValidRelayEndpoint("ws://example.invalid/v2", true));
	EXPECT_TRUE(ValidRelayEndpoint("wss://example.invalid/v2"));
	EXPECT_FALSE(ValidRelayEndpoint("wss://user:password@example.invalid/v2"));
	EXPECT_FALSE(ValidRelayEndpoint("wss://example.invalid/v2?token=secret"));
	for(unsigned I = 0; I < 100; ++I)
		EXPECT_LE(RelayReconnectDelay(I), 30);
	EXPECT_EQ(RelayReconnectDelay(0), 1);
	EXPECT_EQ(RelayReconnectDelay(1), 2);
	EXPECT_EQ(RelayReconnectDelay(5), 30);
	CRelayTransport Relay;
	EXPECT_EQ(Relay.Status().m_State, ETransportState::Disconnected);
	Relay.Configure(true, "", Context());
	EXPECT_EQ(Relay.Status().m_State, ETransportState::Error);
	EXPECT_FALSE(Relay.SendPing(1, vec2(0, 0)));
	Relay.Configure(false, "", std::nullopt);
	EXPECT_EQ(Relay.Status().m_Participants, 0u);
}
TEST(LucentPingRelay, TwoNativeClientsLocalRelay)
{
	const char *pUrl = std::getenv("LUCENT_PING_TEST_RELAY_URL");
	if(!pUrl)
		GTEST_SKIP() << "Run services/lucent-ping-relay npm test with LUCENT_PING_NATIVE_TEST pointing to testrunner.exe";
	CRelayTransport A(true), B(true);
	const auto C = Context();
	A.Configure(true, pUrl, C);
	B.Configure(true, pUrl, C);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 2 && B.Status().m_Participants == 2; }));
	ASSERT_EQ(A.Status().m_State, ETransportState::Connected);
	if(std::getenv("LUCENT_PING_TEST_RESTART"))
	{
		std::puts("NATIVE_RELAY_RECONNECT_READY");
		std::fflush(stdout);
		ASSERT_TRUE(Wait([&] { return A.Status().m_Reconnects > 0 && B.Status().m_Reconnects > 0 && A.Status().m_Participants == 2 && B.Status().m_Participants == 2; }));
	}
	const vec2 P(12345.125f, -9876.5f), Q(-.03125f, 42.5f);
	const double Before = double(time_get()) / time_freq();
	ASSERT_TRUE(A.SendPing(1, P, 3));
	ASSERT_TRUE(B.SendPing(1, Q, 7));
	CReceivedPing ToA{}, ToB{};
	ASSERT_TRUE(Wait([&] { return B.Receive(ToB); }));
	ASSERT_TRUE(Wait([&] { return A.Receive(ToA); }));
	EXPECT_EQ(ToA.m_Position, Q);
	EXPECT_EQ(ToA.m_SenderClientId, 7);
	EXPECT_EQ(ToB.m_Position, P);
	EXPECT_EQ(ToB.m_SenderClientId, 3);
	EXPECT_GE(ToB.m_Received, Before);
	ASSERT_TRUE(A.SendPing(1, P, 3)); // duplicate ID is filtered by the relay
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	CReceivedPing Extra;
	EXPECT_FALSE(A.Receive(Extra));
	EXPECT_FALSE(B.Receive(Extra));
	EXPECT_EQ(A.Status().m_Sent, 2u);
	EXPECT_EQ(B.Status().m_Sent, 1u);
	EXPECT_EQ(A.Status().m_Received, 1u);
	EXPECT_EQ(B.Status().m_Received, 1u);
	auto Other = C;
	Other.m_Map[0] = 3;
	B.Configure(true, pUrl, Other);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 1 && B.Status().m_Participants == 1; }));
	B.Configure(false, pUrl, std::nullopt);
	EXPECT_FALSE(B.Receive(Extra));
	EXPECT_EQ(B.Status().m_Participants, 0u);
	B.Configure(true, pUrl, C);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 2 && B.Status().m_Participants == 2; }));
	ASSERT_TRUE(B.SendPing(2, Q));
	ASSERT_TRUE(Wait([&] { return A.Receive(ToA); }));
	EXPECT_EQ(ToA.m_Id, 2u);
	A.Configure(true, pUrl, std::nullopt);
	B.Configure(true, pUrl, std::nullopt);
	EXPECT_EQ(A.Status().m_Participants, 0u);
	EXPECT_FALSE(A.Receive(Extra));
	EXPECT_FALSE(A.SendPing(3, P));
}

TEST(LucentPingRelay, DisableOnlyClearsRemoteMarkers)
{
	CState Manager;
	Manager.Add(vec2(1, 2), 100);
	Manager.AddRemote(42, vec2(3, 4), 100);
	Manager.ClearRemote();
	ASSERT_EQ(Manager.Events().size(), 1u);
	EXPECT_FALSE(Manager.Events().front().m_Remote);
	EXPECT_EQ(Manager.Events().front().m_Position, vec2(1, 2));
}
TEST(LucentPingRelay, RejectsUntrustedTlsCertificate)
{
	const char *pUrl = std::getenv("LUCENT_PING_TEST_UNTRUSTED_WSS_URL");
	if(!pUrl)
		GTEST_SKIP() << "Local untrusted TLS fixture is started by npm test";
	CRelayTransport Relay;
	Relay.Configure(true, pUrl, Context());
	ASSERT_TRUE(Wait([&] { return Relay.Status().m_Reconnects >= 1; }));
	EXPECT_NE(Relay.Status().m_State, ETransportState::Connected);
	EXPECT_EQ(Relay.Status().m_Participants, 0u);
}

// Explicit opt-in only: normal unit test runs never contact the public service.
TEST(LucentPingRelay, TwoNativeClientsPublicRelay)
{
	if(!std::getenv("LUCENT_PING_TEST_PUBLIC"))
		GTEST_SKIP() << "Set LUCENT_PING_TEST_PUBLIC=1 for an isolated public-relay smoke test";
	CContext C;
	// Random synthetic hashes ensure test pings cannot enter a real players' room.
	secure_random_fill(C.m_Server.data(), unsigned(C.m_Server.size()));
	secure_random_fill(C.m_Map.data(), unsigned(C.m_Map.size()));
	CRelayTransport A, B;
	const char *pEndpoint = DefaultConfig::ClLucentPingRelayUrl;
	A.Configure(true, pEndpoint, C);
	B.Configure(true, pEndpoint, C);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 2 && B.Status().m_Participants == 2; })) << "A: " << A.Status().m_Detail << "; B: " << B.Status().m_Detail;
	ASSERT_EQ(A.Status().m_State, ETransportState::Connected);
	ASSERT_EQ(B.Status().m_State, ETransportState::Connected);
	const vec2 P(123.125f, -45.5f), Q(-.03125f, 42.5f);
	ASSERT_TRUE(A.SendPing(1, P, 3));
	ASSERT_TRUE(B.SendPing(1, Q, 7));
	CReceivedPing ToA{}, ToB{}, Extra{};
	ASSERT_TRUE(Wait([&] { return A.Receive(ToA); }));
	ASSERT_TRUE(Wait([&] { return B.Receive(ToB); }));
	EXPECT_EQ(ToA.m_Position, Q);
	EXPECT_EQ(ToA.m_SenderClientId, 7);
	EXPECT_EQ(ToB.m_Position, P);
	EXPECT_EQ(ToB.m_SenderClientId, 3);
	EXPECT_EQ(ToA.m_Id, 1u);
	EXPECT_EQ(ToB.m_Id, 1u);
	ASSERT_TRUE(A.SendPing(1, P, 3));
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	EXPECT_FALSE(A.Receive(Extra));
	EXPECT_FALSE(B.Receive(Extra));
	EXPECT_EQ(A.Status().m_Received, 1u);
	EXPECT_EQ(B.Status().m_Received, 1u);
	auto Other = C;
	Other.m_Map[0] ^= 1;
	B.Configure(true, pEndpoint, Other);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 1 && B.Status().m_Participants == 1; }));
	B.Configure(false, pEndpoint, std::nullopt);
	EXPECT_EQ(B.Status().m_Participants, 0u);
	EXPECT_FALSE(B.Receive(Extra));
	B.Configure(true, pEndpoint, C);
	ASSERT_TRUE(Wait([&] { return A.Status().m_Participants == 2 && B.Status().m_Participants == 2; }));
	ASSERT_TRUE(B.SendPing(2, Q));
	ASSERT_TRUE(Wait([&] { return A.Receive(ToA); }));
	EXPECT_EQ(ToA.m_Id, 2u);
	A.Configure(false, pEndpoint, std::nullopt);
	B.Configure(false, pEndpoint, std::nullopt);
	EXPECT_EQ(A.Status().m_Participants, 0u);
	EXPECT_EQ(B.Status().m_Participants, 0u);
}

TEST(LucentPingIdentity, ValidationSelectionAndSnapshotPresence)
{
	for(int Id : {-100, -1, 128, 65535})
		EXPECT_FALSE(ValidSenderClientId(Id));
	EXPECT_TRUE(ValidSenderClientId(0));
	EXPECT_TRUE(ValidSenderClientId(127));
	// Both normalized 0.6 and 0.7 snapshots select the controlled connection, never the followed spectator target.
	EXPECT_EQ(LocalSenderClientId(3, 3, 7, false, true), 3);
	EXPECT_EQ(LocalSenderClientId(7, 3, 7, true, true), 7);
	EXPECT_EQ(LocalSenderClientId(3, 3, 7, true, false), 3);
	EXPECT_EQ(LocalSenderClientId(3, 3, 7, true, true), -1);
	EXPECT_EQ(LocalSenderClientId(-1, -1, -1, false, false), -1);
	CTeeRenderInfo Tee;
	Tee.m_ColorBody = ColorRGBA(.2f, .3f, .4f, 1);
	Tee.m_ColorFeet = ColorRGBA(.5f, .6f, .7f, 1);
	Tee.m_aSixup[0].m_aColors[0] = ColorRGBA(.1f, .2f, .3f, 1);
	Tee.m_aSixup[1].m_aColors[0] = ColorRGBA(.7f, .8f, .9f, 1);
	for(int Id : {0, 3, 7, 127})
	{
		const auto Appearance = SenderAppearance(Id, true, true, "Player", &Tee);
		ASSERT_TRUE(Appearance.m_Available);
		EXPECT_EQ(Appearance.m_Name, "Player");
		EXPECT_EQ(Appearance.m_Tee.m_ColorBody.r, Tee.m_ColorBody.r);
		EXPECT_EQ(Appearance.m_Tee.m_ColorFeet.g, Tee.m_ColorFeet.g);
		EXPECT_EQ(Appearance.m_Tee.m_aSixup[0].m_aColors[0].r, .1f);
		EXPECT_EQ(Appearance.m_Tee.m_aSixup[1].m_aColors[0].r, .7f);
	}
	EXPECT_FALSE(SenderAppearance(3, false, true, "Stale", &Tee).m_Available);
	EXPECT_TRUE(SenderAppearance(3, false, true, "Stale", &Tee).m_Name.empty());
	EXPECT_FALSE(SenderAppearance(3, true, false, "Stale", &Tee).m_Available);
	EXPECT_FALSE(SenderAppearance(128, true, true, "Invalid", &Tee).m_Available);
	EXPECT_FALSE(SenderAppearance(-1, true, true, "Invalid", &Tee).m_Available);
	// Spectators still have player info/skin despite not having a character object.
	EXPECT_TRUE(SenderAppearance(7, true, true, "Spectator", &Tee).m_Available);
}
TEST(LucentPingIdentity, WireIdsOldVersionAndMultipleSenders)
{
	for(int Id : {-1, 0, 3, 127, 128, 65534})
	{
		CRelayFrame F{1, 42, 10, vec2(12.5f, -.25f), Id}, Parsed;
		const auto Wire = EncodeRelay(F);
		EXPECT_EQ(DecodeRelay(Wire.data(), Wire.size(), Parsed), ValidSenderClientId(Id));
		CPacket P;
		P.m_Context = Context();
		P.m_Type = EPacketType::Ping;
		P.m_Sequence = 1;
		P.m_EventId = 1;
		P.m_SenderClientId = Id;
		const auto Udp = Encode(P);
		CPacket Decoded;
		EXPECT_EQ(Decode(Udp.data(), Udp.size(), Decoded) == EDecode::Ok, ValidSenderClientId(Id));
	}
	CRelayFrame F{1, 42, 1, vec2(0, 0), 3}, Parsed;
	auto Old = EncodeRelay(F);
	Old[3] = '1';
	EXPECT_FALSE(DecodeRelay(Old.data(), Old.size(), Parsed));
	CState State;
	State.Add(vec2(0, 0), 100);
	State.AddRemote(1, vec2(1, 2), 100, 3);
	State.AddRemote(1, vec2(3, 4), 100, 7);
	ASSERT_EQ(State.Events().size(), 3u);
	EXPECT_FALSE(State.Events()[0].m_Remote);
	EXPECT_EQ(State.Events()[1].m_SenderClientId, 3);
	EXPECT_EQ(State.Events()[2].m_SenderClientId, 7);
	EXPECT_FLOAT_EQ(State.Events()[1].Alpha(103.7), .5f);
	State.Update(104);
	EXPECT_TRUE(State.Events().empty());
}

namespace
{
	struct CConfigRestore
	{
		CConfig m_Saved = g_Config;
		~CConfigRestore() { g_Config = m_Saved; }
	};
	struct CPingConfigSession
	{
		std::unique_ptr<IKernel> m_pKernel{IKernel::Create()};
		IConsole *m_pConsole;
		IConfigManager *m_pConfig;
		explicit CPingConfigSession(IStorage *pStorage)
		{
			m_pKernel->RegisterInterface(pStorage, false);
			m_pConsole = CreateConsole(CFGFLAG_CLIENT).release();
			m_pKernel->RegisterInterface(m_pConsole);
			m_pConfig = CreateConfigManager();
			m_pKernel->RegisterInterface(m_pConfig);
			m_pConsole->Init();
			m_pConfig->Init();
		}
	};
}
TEST(LucentPingRelay, PreferencePersistsThroughFreshConfigSessions)
{
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	EXPECT_EQ(DefaultConfig::ClLucentPingOnline, 1);
	{
		CPingConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_ClLucentPingOnline, 1);
		Session.m_pConsole->ExecuteLine("lucent_ping_online 0", IConsole::CLIENT_ID_UNSPECIFIED);
		ASSERT_EQ(g_Config.m_ClLucentPingOnline, 0);
		ASSERT_TRUE(Session.m_pConfig->Save());
		char *pSaved = Storage->ReadFileStr(CONFIG_FILE, IStorage::TYPE_SAVE);
		ASSERT_NE(pSaved, nullptr);
		EXPECT_NE(std::string(pSaved).find("lucent_ping_online 0"), std::string::npos);
		free(pSaved);
	}
	{
		CPingConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_ClLucentPingOnline, 1);
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
		EXPECT_EQ(g_Config.m_ClLucentPingOnline, 0);
		Session.m_pConsole->ExecuteLine("lucent_ping_online 1", IConsole::CLIENT_ID_UNSPECIFIED);
		ASSERT_EQ(g_Config.m_ClLucentPingOnline, 1);
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CPingConfigSession Session(Storage.get());
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
		EXPECT_EQ(g_Config.m_ClLucentPingOnline, 1);
	}
}
TEST(LucentPingRelay, LocalPingsSurviveDisabledAndUnavailableRelay)
{
	CState State;
	State.Add(vec2(3, 4), 100);
	CRelayTransport Relay;
	Relay.Configure(false, DefaultConfig::ClLucentPingRelayUrl, Context());
	EXPECT_FALSE(Relay.SendPing(1, vec2(3, 4), 3));
	Relay.Configure(true, "", Context());
	EXPECT_EQ(Relay.Status().m_State, ETransportState::Error);
	EXPECT_FALSE(Relay.SendPing(1, vec2(3, 4), 3));
	ASSERT_EQ(State.Events().size(), 1u);
	EXPECT_FALSE(State.Events()[0].m_Remote);
	EXPECT_FLOAT_EQ(State.Events()[0].Alpha(103.7), .5f);
	State.Update(104);
	EXPECT_TRUE(State.Events().empty());
}
TEST(LucentPingRelay, PersistentAutoClientProcess)
{
	const char *pRole = std::getenv("LUCENT_PING_AUTO_PEER");
	const char *pUrl = std::getenv("LUCENT_PING_TEST_RELAY_URL");
	if(!pRole || !pUrl)
		GTEST_SKIP() << "npm test starts two concurrent native client-transport processes";
	const bool First = std::string(pRole) == "A";
	const int Self = First ? 3 : 7, Peer = First ? 7 : 3;
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	CPingConfigSession Session(Storage.get());
	ASSERT_EQ(g_Config.m_ClLucentPingOnline, 1);
	CRelayTransport Relay(true);
	// No game context (offline/menu/demo): preference stays enabled but no connection exists.
	Relay.Configure(g_Config.m_ClLucentPingOnline != 0, pUrl, std::nullopt);
	EXPECT_EQ(Relay.Status().m_State, ETransportState::Disconnected);
	EXPECT_EQ(Relay.Status().m_Participants, 0u);
	auto Current = Context();
	CReceivedPing Incoming;
	for(int Stage = 0; Stage < 3; ++Stage)
	{
		if(Stage == 1)
			Current.m_Server[0] = 9;
		if(Stage == 2)
			Current.m_Map[0] = 7;
		for(int Frame = 0; Frame < 30; ++Frame)
			Relay.Configure(g_Config.m_ClLucentPingOnline != 0, pUrl, Current);
		ASSERT_TRUE(Wait([&] { return Relay.Status().m_Participants == 2; }));
		ASSERT_TRUE(Wait([&] { return Relay.SendPing(uint64_t(Stage * 2 + 1), vec2(float(Self), float(Stage)), Self); }));
		ASSERT_TRUE(Wait([&] { return Relay.Receive(Incoming); }));
		EXPECT_EQ(Incoming.m_SenderClientId, Peer);
		EXPECT_EQ(Incoming.m_Id, uint64_t(Stage * 2 + 1));
		EXPECT_EQ(Incoming.m_Position, vec2(float(Peer), float(Stage)));
		// Confirm the partner consumed this stage before either process changes context.
		ASSERT_TRUE(Wait([&] { return Relay.SendPing(uint64_t(Stage * 2 + 2), vec2(float(Self), float(Stage)), Self); }));
		ASSERT_TRUE(Wait([&] { return Relay.Receive(Incoming); }));
		EXPECT_EQ(Incoming.m_Id, uint64_t(Stage * 2 + 2));
		ASSERT_TRUE(Wait([&] { return Relay.Status().m_Sent >= uint64_t((Stage + 1) * 2); }));
	}
	Relay.Configure(true, pUrl, std::nullopt);
	EXPECT_EQ(Relay.Status().m_Participants, 0u);
	EXPECT_FALSE(Relay.Receive(Incoming));
	EXPECT_EQ(g_Config.m_ClLucentPingOnline, 1);
	Session.m_pConsole->ExecuteLine("lucent_ping_online 0", IConsole::CLIENT_ID_UNSPECIFIED);
	Relay.Configure(g_Config.m_ClLucentPingOnline != 0, pUrl, Current);
	EXPECT_EQ(Relay.Status().m_State, ETransportState::Disconnected);
	EXPECT_EQ(Relay.Status().m_Participants, 0u);
	ASSERT_TRUE(Session.m_pConfig->Save());
}
