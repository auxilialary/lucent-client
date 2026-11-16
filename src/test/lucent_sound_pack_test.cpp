#include "test.h"

#include <engine/shared/sound_pack.h>

#include <game/client/lucent/settings/workspace.h>

#include <gtest/gtest.h>

using namespace LucentSoundPacks;
namespace
{
	void Put(CBytes &B, uint32_t V, int N)
	{
		for(int i = 0; i < N; ++i)
			B.push_back(V >> (8 * i));
	}
	CBytes Zip(const std::vector<CEntry> &Files, bool Deflate = false)
	{
		CBytes B, Central;
		for(const auto &File : Files)
		{
			const auto Offset = B.size();
			CBytes Data = File.m_Data;
			if(Deflate)
			{
				Data.resize(compressBound(File.m_Data.size()));
				z_stream S{};
				S.next_in = const_cast<Bytef *>(File.m_Data.data());
				S.avail_in = File.m_Data.size();
				S.next_out = Data.data();
				S.avail_out = Data.size();
				deflateInit2(&S, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
				deflate(&S, Z_FINISH);
				Data.resize(S.total_out);
				deflateEnd(&S);
			}
			const auto Crc = crc32(0, File.m_Data.data(), File.m_Data.size());
			Put(B, 0x04034b50, 4);
			Put(B, 20, 2);
			Put(B, 0, 2);
			Put(B, Deflate ? 8 : 0, 2);
			Put(B, 0, 4);
			Put(B, Crc, 4);
			Put(B, Data.size(), 4);
			Put(B, File.m_Data.size(), 4);
			Put(B, File.m_Path.size(), 2);
			Put(B, 0, 2);
			B.insert(B.end(), File.m_Path.begin(), File.m_Path.end());
			B.insert(B.end(), Data.begin(), Data.end());
			Put(Central, 0x02014b50, 4);
			Put(Central, 20, 2);
			Put(Central, 20, 2);
			Put(Central, 0, 2);
			Put(Central, Deflate ? 8 : 0, 2);
			Put(Central, 0, 4);
			Put(Central, Crc, 4);
			Put(Central, Data.size(), 4);
			Put(Central, File.m_Data.size(), 4);
			Put(Central, File.m_Path.size(), 2);
			Put(Central, 0, 4);
			Put(Central, 0, 4);
			Put(Central, 0, 4);
			Put(Central, Offset, 4);
			Central.insert(Central.end(), File.m_Path.begin(), File.m_Path.end());
		}
		const auto Offset = B.size();
		B.insert(B.end(), Central.begin(), Central.end());
		Put(B, 0x06054b50, 4);
		Put(B, 0, 4);
		Put(B, Files.size(), 2);
		Put(B, Files.size(), 2);
		Put(B, Central.size(), 4);
		Put(B, Offset, 4);
		Put(B, 0, 2);
		return B;
	}
}
TEST(LucentSoundPacks, CategoriesAndReadableLabels)
{
	EXPECT_EQ(Describe("gun_fire").m_Category, WEAPONS);
	EXPECT_EQ(Describe("hook_attach_player").m_Category, HOOK);
	EXPECT_EQ(Describe("player_airjump").m_Category, JUMPS);
	EXPECT_EQ(Describe("body_land").m_Category, MOVEMENT);
	EXPECT_EQ(Describe("menu").m_Category, MUSIC);
	std::set<std::string> Names;
	for(const auto &Description : Descriptions)
	{
		EXPECT_TRUE(Names.insert(Description.m_pSet).second);
		EXPECT_GT(Description.m_Category, ALL);
		EXPECT_LT(Description.m_Category, CATEGORY_COUNT);
		EXPECT_NE(Description.m_pLabel[0], 0);
	}
	EXPECT_EQ(Names.size(), 41u);
}

TEST(LucentSoundPacks, MergedPresetIsSelfContainedAndSupportsUtf8Names)
{
	const std::vector<CEntry> Current = {{"audio/wp_gun_fire-01.opus", {1}}, {"audio/music_menu.wv", {2}}};
	const auto Merged = Merge(Current, {{"audio/WP_GUN_FIRE-01.wv", {3}}, {"audio/hook_loop-01.wv", {4}}});
	ASSERT_EQ(Merged.size(), 3u);
	EXPECT_EQ(Merged[0].m_Data, (CBytes{3}));
	EXPECT_EQ(Current[0].m_Data, (CBytes{1}));
	CBytes Bytes;
	ASSERT_TRUE(Encode(Merged, Bytes));
	std::vector<CEntry> Reloaded;
	ASSERT_TRUE(Parse(Bytes, Reloaded));
	std::map<size_t, const CEntry *> Matches;
	ASSERT_TRUE(Match(Reloaded, {"audio/wp_gun_fire-01.wv", "audio/music_menu.wv", "audio/hook_loop-01.wv", "audio/hook_loop-02.wv"}, Matches));
	EXPECT_EQ(Matches.size(), 3u);
	EXPECT_EQ(Matches.at(0)->m_Data, (CBytes{3}));
	EXPECT_EQ(Matches.at(1)->m_Data, (CBytes{2}));
	EXPECT_EQ(Matches.at(2)->m_Data, (CBytes{4}));
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	const auto Label = Slug("Мой пресет");
	const auto Name = Label + "-" + std::string(64, 'a') + ".zip";
	EXPECT_TRUE(Managed(Name.c_str()));
	EXPECT_EQ(DisplayName(Name), "Мой_пресет");
	ASSERT_TRUE(Save(Storage.get(), Name.c_str(), Bytes));
	CBytes Persisted;
	ASSERT_TRUE(Read(Storage.get(), (std::string(Directory) + "/" + Name).c_str(), IStorage::TYPE_SAVE, Persisted));
	ASSERT_TRUE(Parse(Persisted, Reloaded));
	EXPECT_EQ(Reloaded.size(), 3u);
	EXPECT_TRUE(str_utf8_check(Slug("ДлинноеимяДлинноеимяДлинноеимяДлинноеимя").c_str()));
	EXPECT_FALSE(BuildSingleImport(Current, "unknown.wv", {1}, {"audio/wp_gun_fire-01.wv", "audio/music_menu.wv"}, Bytes));
	ASSERT_TRUE(BuildSingleImport(Current, "WP_GUN_FIRE-01.WV", {9}, {"audio/wp_gun_fire-01.wv", "audio/music_menu.wv"}, Bytes));
	ASSERT_TRUE(Parse(Bytes, Reloaded));
	EXPECT_EQ(Reloaded.size(), 2u);
	EXPECT_EQ(Reloaded[0].m_Path, "audio/wp_gun_fire-01.wv");
	EXPECT_EQ(Reloaded[0].m_Data, (CBytes{9}));
	EXPECT_EQ(Reloaded[1].m_Data, (CBytes{2}));
	EXPECT_FALSE(Encode({}, Bytes));
	EXPECT_FALSE(Encode({{"../evil.wv", {1}}}, Bytes));
}

TEST(LucentSoundPacks, ReviewCancelAndApplyTransferOwnershipAtomically)
{
	CSelection Active, Review;
	CEntry A{"a.wv", {1}}, B{"b.wv", {2}};
	int Next = 10;
	std::vector<int> Freed;
	auto Load = [&](const auto &) { return Next++; };
	auto Unload = [&](int Id) { Freed.push_back(Id); };
	auto Prepare = [](const auto &) { return true; };
	ASSERT_TRUE(Active.Replace({{0, &A}}, Load, Unload, Prepare));
	ASSERT_TRUE(Review.Replace({{1, &B}}, Load, Unload, Prepare));
	EXPECT_EQ(Active.Samples().at(0), 10);
	EXPECT_TRUE(Freed.empty());
	EXPECT_FALSE(Active.Adopt(Review, Unload, [](const auto &) { return false; }));
	EXPECT_EQ(Active.Samples().at(0), 10);
	EXPECT_EQ(Review.Samples().at(1), 11);
	ASSERT_TRUE(Review.Replace({}, Load, Unload, Prepare));
	EXPECT_EQ(Freed, (std::vector<int>{11}));
	ASSERT_TRUE(Review.Replace({{1, &B}}, Load, Unload, Prepare));
	ASSERT_TRUE(Active.Adopt(Review, Unload, Prepare));
	EXPECT_TRUE(Review.Samples().empty());
	EXPECT_EQ(Active.Samples().at(1), 12);
	EXPECT_EQ(Freed, (std::vector<int>{11, 10}));
	ASSERT_TRUE(Active.Replace({}, Load, Unload, Prepare));
	EXPECT_EQ(Freed, (std::vector<int>{11, 10, 12}));
}

TEST(LucentSoundPacks, StandaloneWvImportAndPersistence)
{
	EXPECT_TRUE(ImportSupported("wp_gun_fire-01.WV"));
	EXPECT_TRUE(ImportSupported("pack.ZIP"));
	EXPECT_FALSE(ImportSupported("test.wav"));
	CBytes Archive;
	ASSERT_TRUE(WrapWv("WP_GUN_FIRE-01.WV", {1, 2, 3}, Archive));
	std::vector<CEntry> Entries;
	ASSERT_TRUE(Parse(Archive, Entries));
	std::map<size_t, const CEntry *> Matches;
	ASSERT_TRUE(Match(Entries, {"audio/wp_gun_fire-01.wv", "audio/wp_gun_fire-02.wv"}, Matches));
	ASSERT_EQ(Matches.size(), 1u);
	EXPECT_EQ(Matches.at(0)->m_Data, (CBytes{1, 2, 3}));
	EXPECT_FALSE(Match(Entries, {"audio/unknown.wv"}, Matches));
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	const auto Name = "Single-" + std::string(64, 'a') + ".zip";
	ASSERT_TRUE(Save(Storage.get(), Name.c_str(), Archive));
	CBytes Reloaded;
	ASSERT_TRUE(Read(Storage.get(), (std::string(Directory) + "/" + Name).c_str(), IStorage::TYPE_SAVE, Reloaded));
	ASSERT_TRUE(Parse(Reloaded, Entries));
	EXPECT_EQ(Entries.at(0).m_Data, (CBytes{1, 2, 3}));
	EXPECT_FALSE(Read(Storage.get(), (std::string(Directory) + "/" + Name).c_str(), IStorage::TYPE_SAVE, Reloaded, 1));
	EXPECT_FALSE(WrapWv("../escape.wv", {1}, Archive));
	EXPECT_FALSE(WrapWv("sound.wv", {}, Archive));
	EXPECT_FALSE(WrapWv("sound.wav", {1}, Archive));
	EXPECT_FALSE(WrapWv("sound.wv", CBytes(MaxFile + 1), Archive));
}

TEST(LucentSoundPacks, StoredDeflatedMappingAndPartialPacks)
{
	for(bool Deflate : {false, true})
	{
		std::vector<CEntry> Entries;
		ASSERT_TRUE(Parse(Zip({{"MyPack/data/audio/WP_GUN_FIRE-01.opus", {1, 2, 3}}, {"readme.txt", {4}}}, Deflate), Entries));
		std::map<size_t, const CEntry *> Matches;
		ASSERT_TRUE(Match(Entries, {"audio/wp_gun_fire-01.wv", "audio/wp_gun_fire-02.wv"}, Matches));
		ASSERT_EQ(Matches.size(), 1u);
		EXPECT_EQ(Matches.begin()->first, 0u);
		EXPECT_EQ(Matches.begin()->second->m_Data, (CBytes{1, 2, 3}));
	}
}
TEST(LucentSoundPacks, RejectTraversalCorruptionDuplicatesAndLimits)
{
	std::vector<CEntry> Entries;
	for(const char *Path : {"../evil.wv", "/audio/test.wv", "C:/test.wv", "audio/../test.wv", "a\\test.wv", "a//test.wv", "a/./test.wv", "a./test.wv"})
		EXPECT_FALSE(Parse(Zip({{Path, {1}}}), Entries)) << Path;
	EXPECT_FALSE(Parse(Zip({{"test.wv", {1}}, {"test.wv", {2}}}), Entries));
	EXPECT_FALSE(Parse(Zip({{"test.wv", {}}}), Entries));
	EXPECT_FALSE(Parse(Zip({{"readme.txt", {1}}}), Entries));
	auto B = Zip({{"test.wv", {1, 2}}});
	B[30 + 7] ^= 1;
	EXPECT_FALSE(Parse(B, Entries));
	B = Zip({{"test.wv", {1}}});
	B[22] = 0xff;
	B[23] = 0xff;
	B[24] = 0xff;
	B[25] = 0x7f;
	EXPECT_FALSE(Parse(B, Entries));
	B = Zip({{"test.wv", {1}}});
	B[6] = 1;
	EXPECT_FALSE(Parse(B, Entries));
	B = Zip({{"test.wv", {1}}});
	for(size_t Size = 0; Size < B.size(); ++Size)
		EXPECT_FALSE(Parse(CBytes(B.begin(), B.begin() + Size), Entries));
	ASSERT_TRUE(Parse(Zip({{"one/test.wv", {1}}, {"two/test.opus", {2}}}), Entries));
	std::map<size_t, const CEntry *> Matches;
	EXPECT_FALSE(Match(Entries, {"audio/test.wv"}, Matches));
	EXPECT_FALSE(Match(Entries, {"audio/other.wv"}, Matches));
}
TEST(LucentSoundPacks, TransactionRollbackRetirementAndDefault)
{
	CSelection State;
	CEntry A{"a.wv", {1}}, B{"b.wv", {2}};
	std::vector<int> Freed;
	int Next = 10, Fail = -1, Commits = 0;
	auto Load = [&](const CEntry &) { return Next == Fail ? -1 : Next++; };
	auto Unload = [&](int Id) { Freed.push_back(Id); };
	auto Commit = [&](const auto &) { ++Commits; return true; };
	ASSERT_TRUE(State.Replace({{0, &A}}, Load, Unload, Commit));
	EXPECT_EQ(State.Samples().at(0), 10);
	Fail = 12;
	EXPECT_FALSE(State.Replace({{0, &A}, {1, &B}}, Load, Unload, Commit));
	EXPECT_EQ(State.Samples().at(0), 10);
	EXPECT_EQ(Freed, (std::vector<int>{11}));
	EXPECT_EQ(Commits, 1);
	Fail = -1;
	EXPECT_FALSE(State.Replace({{0, &A}}, Load, Unload, [](const auto &) { return false; }));
	EXPECT_EQ(State.Samples().at(0), 10);
	ASSERT_TRUE(State.Replace({{1, &B}}, Load, Unload, Commit));
	EXPECT_EQ(State.Samples().size(), 1u);
	EXPECT_EQ(Freed.back(), 10);
	ASSERT_TRUE(State.Replace({}, Load, Unload, Commit));
	EXPECT_TRUE(State.Samples().empty());
	EXPECT_EQ(Freed.back(), 13);
}
TEST(LucentSoundPacks, StorageRoundTripAndNames)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	const auto Bytes = Zip({{"audio/test.wv", {1, 2}}}, true);
	auto File = Storage->OpenFile("pack.zip", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	EXPECT_EQ(io_write(File, Bytes.data(), Bytes.size()), Bytes.size());
	EXPECT_EQ(io_close(File), 0);
	CBytes ReadBack;
	ASSERT_TRUE(Read(Storage.get(), "pack.zip", IStorage::TYPE_SAVE, ReadBack));
	EXPECT_EQ(ReadBack, Bytes);
	std::vector<CEntry> Entries;
	EXPECT_TRUE(Parse(ReadBack, Entries));
	const auto Name = std::string("MyPack-") + std::string(64, 'a') + ".zip";
	EXPECT_TRUE(Managed(Name.c_str()));
	ASSERT_TRUE(Save(Storage.get(), Name.c_str(), Bytes));
	ASSERT_TRUE(Save(Storage.get(), Name.c_str(), Bytes)); // Existing imported pack can be repaired atomically.
	const std::string Path = std::string(Directory) + "/" + Name;
	ASSERT_TRUE(Read(Storage.get(), Path.c_str(), IStorage::TYPE_SAVE, ReadBack));
	EXPECT_EQ(ReadBack, Bytes);
	EXPECT_FALSE(Storage->FileExists((Path + ".tmp").c_str(), IStorage::TYPE_SAVE));
	EXPECT_FALSE(Save(Storage.get(), "../escape.zip", Bytes));
	EXPECT_FALSE(Storage->FileExists("lucent/escape.zip", IStorage::TYPE_SAVE));
	EXPECT_EQ(DisplayName(Name), "MyPack");
	EXPECT_FALSE(Managed(("../" + Name).c_str()));
	EXPECT_FALSE(Managed("unknown.zip"));
	EXPECT_EQ(DisplayName(""), "Default");
	int Workspace = LucentSettings::WorkspaceLucent, Page = LucentSettings::PAGE_SOUNDS, DDNet = 0;
	LucentSettings::NormalizeWorkspace(Workspace, Page, DDNet);
	EXPECT_EQ(Page, LucentSettings::PAGE_SOUNDS);
}

#if defined(CONF_SOUND_PACK_AUDIO_TEST)
#include <base/fs.h>

#include <engine/client/sound.h>
#include <engine/shared/config.h>

TEST(LucentSoundPacks, RealDecoderHotSwapRollbackDefaultAndResourceReuse)
{
	const CConfig Saved = g_Config;
	struct CRestore
	{
		CConfig m_Saved;
		~CRestore() { g_Config = m_Saved; }
	} Restore{Saved};
	g_Config.m_SndEnable = 0; // No audio device or window is created by this test.
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	auto Kernel = std::unique_ptr<IKernel>(IKernel::Create());
	Kernel->RegisterInterface(Storage.get(), false);
	CSound Sound;
	Kernel->RegisterInterface<IEngineSound>(&Sound, false);
	ASSERT_EQ(Sound.Init(), 0);
	struct CShutdown
	{
		CSound &m_Sound;
		~CShutdown() { m_Sound.Shutdown(); }
	} Shutdown{Sound};
	CBytes Wv;
	char aCwd[IO_MAX_PATH_LENGTH];
	ASSERT_TRUE(fs_getcwd(aCwd, sizeof(aCwd)));
	ASSERT_TRUE(Read(Storage.get(), (std::string(aCwd) + "/data/audio/wp_gun_fire-01.wv").c_str(), IStorage::TYPE_ABSOLUTE, Wv));
	// 50 ms synthetic 440 Hz tone encoded with libopus; no external test dependency.
	const CBytes Opus = {
		79,
		103,
		103,
		83,
		0,
		2,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		49,
		119,
		233,
		115,
		0,
		0,
		0,
		0,
		3,
		120,
		238,
		73,
		1,
		19,
		79,
		112,
		117,
		115,
		72,
		101,
		97,
		100,
		1,
		1,
		56,
		1,
		128,
		187,
		0,
		0,
		0,
		0,
		0,
		79,
		103,
		103,
		83,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		49,
		119,
		233,
		115,
		1,
		0,
		0,
		0,
		139,
		93,
		172,
		158,
		1,
		60,
		79,
		112,
		117,
		115,
		84,
		97,
		103,
		115,
		12,
		0,
		0,
		0,
		76,
		97,
		118,
		102,
		54,
		51,
		46,
		49,
		46,
		49,
		48,
		49,
		1,
		0,
		0,
		0,
		28,
		0,
		0,
		0,
		101,
		110,
		99,
		111,
		100,
		101,
		114,
		61,
		76,
		97,
		118,
		99,
		54,
		51,
		46,
		49,
		46,
		49,
		48,
		49,
		32,
		108,
		105,
		98,
		111,
		112,
		117,
		115,
		79,
		103,
		103,
		83,
		0,
		4,
		152,
		10,
		0,
		0,
		0,
		0,
		0,
		0,
		49,
		119,
		233,
		115,
		2,
		0,
		0,
		0,
		15,
		126,
		83,
		67,
		5,
		255,
		52,
		168,
		255,
		59,
		248,
		114,
		73,
		71,
		39,
		16,
		228,
		200,
		91,
		120,
		39,
		139,
		135,
		23,
		65,
		65,
		221,
		40,
		177,
		227,
		236,
		54,
		230,
		17,
		242,
		16,
		39,
		218,
		38,
		136,
		181,
		56,
		202,
		206,
		242,
		244,
		81,
		66,
		211,
		189,
		168,
		187,
		4,
		87,
		119,
		244,
		188,
		11,
		101,
		219,
		89,
		58,
		159,
		10,
		218,
		240,
		143,
		0,
		44,
		218,
		118,
		114,
		230,
		168,
		74,
		150,
		151,
		25,
		88,
		123,
		253,
		84,
		130,
		230,
		39,
		150,
		14,
		215,
		200,
		69,
		215,
		78,
		248,
		105,
		186,
		11,
		17,
		46,
		221,
		23,
		16,
		209,
		49,
		187,
		225,
		159,
		230,
		62,
		55,
		20,
		63,
		203,
		161,
		152,
		250,
		226,
		229,
		119,
		231,
		134,
		104,
		88,
		52,
		37,
		233,
		170,
		236,
		28,
		52,
		247,
		170,
		25,
		125,
		239,
		107,
		222,
		183,
		207,
		12,
		161,
		109,
		180,
		75,
		115,
		100,
		56,
		26,
		185,
		96,
		99,
		36,
		185,
		50,
		227,
		60,
		205,
		119,
		81,
		96,
		51,
		210,
		126,
		86,
		235,
		222,
		111,
		229,
		1,
		164,
		95,
		102,
		195,
		158,
		241,
		87,
		136,
		239,
		57,
		137,
		215,
		215,
		137,
		184,
		153,
		109,
		112,
		253,
		155,
		61,
		173,
		82,
		0,
		63,
		101,
		185,
		94,
		223,
		126,
		242,
		112,
		162,
		33,
		207,
		164,
		181,
		110,
		58,
		33,
		207,
		165,
		44,
		221,
		145,
		124,
		169,
		237,
		234,
		145,
		42,
		123,
		88,
		94,
		238,
		236,
		171,
		31,
		131,
		35,
		169,
		80,
		215,
		10,
		196,
		14,
		54,
		109,
		126,
		24,
		251,
		144,
		156,
		106,
		78,
		126,
		25,
		130,
		115,
		0,
		101,
		144,
		118,
		85,
		95,
		235,
		224,
		125,
		66,
		10,
		215,
		149,
		250,
		6,
		255,
		178,
		221,
		165,
		98,
		147,
		10,
		150,
		141,
		214,
		238,
		184,
		138,
		203,
		203,
		62,
		74,
		68,
		238,
		180,
		90,
		44,
		139,
		145,
		119,
		214,
		243,
		232,
		16,
		206,
		33,
		13,
		51,
		51,
		154,
		79,
		145,
		210,
		50,
		22,
		211,
		236,
		173,
		58,
		206,
		115,
		132,
		171,
		240,
		160,
		198,
		79,
		36,
		50,
		37,
		248,
		175,
		233,
		29,
		237,
		216,
		138,
		218,
		94,
		19,
		106,
		92,
		172,
		136,
		191,
		206,
		36,
		192,
		18,
		188,
		164,
		109,
		44,
		194,
		153,
		134,
		199,
		148,
		220,
		154,
		67,
		81,
		210,
		115,
		218,
		226,
		251,
		178,
		1,
		45,
		194,
		248,
		49,
		85,
		219,
		234,
		107,
		235,
		231,
		235,
		9,
		145,
		221,
		163,
		135,
		137,
		236,
		226,
		6,
		51,
		53,
		45,
		152,
		117,
		226,
		116,
		36,
		27,
		130,
		56,
		44,
		169,
		141,
		143,
		114,
		145,
		13,
		169,
		199,
		140,
		223,
		168,
		77,
		178,
		22,
		229,
		59,
		44,
		78,
		194,
		106,
		209,
		47,
		185,
		56,
		201,
		242,
		193,
		124,
		97,
		12,
		130,
		88,
		66,
		234,
		138,
		21,
		6,
		134,
		52,
		55,
		102,
		114,
		155,
		39,
		183,
		129,
		114,
		148,
		85,
		9,
		170,
		44,
		205,
		252,
		22,
		187,
		132,
		247,
		191,
		52,
		187,
		87,
		76,
		17,
		95,
		82,
		86,
		87,
		26,
		58,
		57,
		74,
		111,
		103,
		43,
		211,
		238,
		166,
		94,
		29,
		105,
		129,
		183,
		144,
		94,
		13,
		235,
		249,
		171,
		2,
		122,
		232,
		59,
		205,
		190,
		19,
		238,
		248,
		180,
		230,
		13,
		231,
		193,
		10,
		220,
		141,
		13,
		170,
		136,
		191,
		117,
		57,
		8,
		63,
		204,
		247,
		149,
		205,
		232,
		67,
		118,
		116,
		48,
		84,
		48,
		254,
		204,
		167,
		117,
		254,
		158,
		12,
		122,
		200,
		7,
		19,
		172,
		64,
		199,
		196,
		214,
		71,
		29,
		137,
		89,
		172,
		26,
		217,
		176,
		196,
		150,
		55,
		16,
		24,
		38,
		29,
		102,
		157,
		176,
		51,
		33,
		252,
		244,
		146,
		49,
		33,
		40,
		199,
		86,
		88,
		123,
		135,
		30,
		220,
		87,
		147,
		191,
		168,
		100,
		121,
		9,
		141,
		45,
		23,
		75,
		172,
		89,
		119,
		103,
		10,
		106,
		43,
		153,
		122,
		227,
		181,
		47,
		140,
		134,
		80,
		183,
		241,
		94,
		238,
		129,
		200,
		202,
		104,
		1,
		142,
		1,
		16,
		29,
		76,
		238,
		41,
		94,
		77,
		57,
		107,
		141,
		12,
		220,
		214,
		42,
		52,
		48,
		79,
		139,
		239,
		58,
		174,
		110,
		0,
		178,
		88,
		179,
		44,
		19,
		167,
		95,
		139,
		173,
		242,
		136,
		7,
		225,
		212,
		6,
		171,
		100,
		211,
		62,
		16,
		178,
		119,
		193,
		25,
		194,
		179,
		228,
		228,
		163,
		44,
		250,
		239,
		13,
		184,
		175,
		221,
		135,
		123,
		207,
		224,
		245,
		254,
		99,
		58,
		159,
		181,
		170,
		60,
		110,
		82,
		96,
		50,
		158,
		52,
		116,
		155,
		33,
		143,
		160,
		170,
		181,
		140,
		179,
		240,
		40,
		73,
		248,
		50,
		252,
		210,
		119,
		7,
		212,
		78,
		232,
		78,
		229,
		147,
		150,
		109,
		187,
		90,
		194,
		44,
		101,
		123,
		110,
		48,
		156,
		192,
		48,
		176,
		125,
		175,
		191,
		69,
		204,
		215,
		125,
		129,
		105,
		66,
		130,
		65,
		237,
		232,
		101,
		199,
		4,
		17,
		42,
		230,
		215,
		69,
		190,
		200,
		196,
		132,
		108,
		154,
		131,
		182,
		160,
		36,
		166,
		155,
		212,
		29,
		164,
		0,
		83,
		242,
		180,
		51,
		217,
		71,
		3,
		20,
		78,
		180,
		95,
		199,
		148,
		228,
		55,
		7,
		28,
		148,
		233,
		250,
		76,
		243,
		116,
		24,
		226,
		251,
		86,
		241,
		74,
		252,
		9,
		254,
		69,
		197,
		189,
		131,
		10,
		139,
		154,
		112,
		246,
		170,
		198,
		221,
		167,
		21,
		44,
	};
	CBytes SingleArchive;
	ASSERT_TRUE(WrapWv("wp_gun_fire-01.wv", Wv, SingleArchive));
	std::vector<CEntry> SingleEntries;
	ASSERT_TRUE(Parse(SingleArchive, SingleEntries));
	const int SingleId = Sound.LoadWVFromMem(SingleEntries[0].m_Data.data(), SingleEntries[0].m_Data.size(), true, "standalone WV", 32 * 1024 * 1024);
	ASSERT_GE(SingleId, 0);
	Sound.UnloadSample(SingleId);
	const int Original = Sound.LoadWVFromMem(Wv.data(), Wv.size(), true, "default");
	ASSERT_GE(Original, 0);
	EXPECT_EQ(Sound.LoadWVFromMem(Wv.data(), Wv.size(), true, "bounded", 16), -1);
	EXPECT_EQ(Sound.LoadOpusFromMem(Opus.data(), Opus.size(), true, "bounded", 16), -1);
	CSelection Selection;
	int Published = Original;
	auto Load = [&](const CEntry &Entry) {
		return str_endswith(Entry.m_Path.c_str(), ".opus") ?
			       Sound.LoadOpusFromMem(Entry.m_Data.data(), Entry.m_Data.size(), true, "pack", 32 * 1024 * 1024) :
			       Sound.LoadWVFromMem(Entry.m_Data.data(), Entry.m_Data.size(), true, "pack", 32 * 1024 * 1024);
	};
	auto Unload = [&](int Id) { Sound.UnloadSample(Id); };
	auto Commit = [&](const auto &Pending) {
		Sound.Stop(Published);
		Published = Pending.empty() ? Original : Pending.begin()->second;
		return true;
	};
	CEntry Invalid{"test.wv", {1, 2, 3}};
	for(int i = 0; i < 600; ++i) // More switches than the engine's sample-slot capacity.
	{
		std::vector<CEntry> Entries;
		ASSERT_TRUE(Parse(Zip({{i % 2 ? "test.wv" : "test.opus", i % 2 ? Wv : Opus}}, i % 2), Entries));
		ASSERT_TRUE(Selection.Replace({{0, &Entries[0]}}, Load, Unload, Commit));
		ASSERT_NE(Published, Original);
		EXPECT_GT(Sound.GetSampleTotalTime(Published), 0.0f);
		const auto Voice = Sound.Play(0, Published, ISound::FLAG_PREVIEW, 1.0f);
		ASSERT_TRUE(Voice.IsValid());
		EXPECT_TRUE(Sound.IsPlaying(Published));
		if(i == 0)
		{
			const int Previous = Published;
			EXPECT_FALSE(Selection.Replace({{0, &Invalid}}, Load, Unload, Commit));
			EXPECT_EQ(Published, Previous);
			EXPECT_TRUE(Sound.IsPlaying(Published));
		}
	}
	ASSERT_TRUE(Selection.Replace({}, Load, Unload, Commit));
	EXPECT_EQ(Published, Original);
	EXPECT_GT(Sound.GetSampleTotalTime(Original), 0.0f);
	EXPECT_TRUE(Sound.Play(0, Original, ISound::FLAG_PREVIEW, 1.0f).IsValid());
	EXPECT_TRUE(Sound.IsPlaying(Original));
	// A/B preview uses independent voices and staged samples without replacing active audio.
	CSelection Review;
	CEntry NewSound{"test.opus", Opus};
	ASSERT_TRUE(Review.Replace({{0, &NewSound}}, Load, Unload, [](const auto &) { return true; }));
	const int Candidate = Review.Samples().at(0);
	EXPECT_EQ(Published, Original);
	const auto Preview = Sound.Play(0, Candidate, ISound::FLAG_PREVIEW, 1.0f);
	EXPECT_TRUE(Preview.IsValid());
	EXPECT_TRUE(Sound.IsPlaying(Original));
	EXPECT_FALSE(Selection.Adopt(Review, Unload, [](const auto &) { return false; }));
	EXPECT_EQ(Published, Original);
	Sound.StopVoice(Preview);
	ASSERT_TRUE(Selection.Adopt(Review, Unload, Commit));
	EXPECT_EQ(Published, Candidate);
	EXPECT_TRUE(Review.Samples().empty());
	ASSERT_TRUE(Selection.Replace({}, Load, Unload, Commit));
	EXPECT_EQ(Published, Original);
	EXPECT_EQ(g_Config.m_SndVolume, Saved.m_SndVolume);
	EXPECT_EQ(g_Config.m_SndGameVolume, Saved.m_SndGameVolume);
	EXPECT_EQ(g_Config.m_SndRate, Saved.m_SndRate);
}
#endif
