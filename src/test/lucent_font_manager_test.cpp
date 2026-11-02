#include "test.h"

#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/shared/font_manager.h>

#include <game/client/lucent/settings/workspace.h>

#include <gtest/gtest.h>

#if defined(CONF_SCOREBOARD_FREETYPE_TEST)
#include <engine/client/custom_font.h>
#include <engine/client/legacy_fonts.h>

class LucentFontManager : public ::testing::Test
{
protected:
	CTestInfo m_Info;
	std::unique_ptr<IStorage> m_pStorage;
	FT_Library m_Library = nullptr;
	void SetUp() override
	{
		ASSERT_EQ(FT_Init_FreeType(&m_Library), 0);
		m_Info.m_DeleteTestStorageFilesOnSuccess = true;
		m_pStorage = m_Info.CreateTestStorage();
		ASSERT_NE(m_pStorage, nullptr);
	}
	void TearDown() override
	{
		if(m_Library)
			FT_Done_FreeType(m_Library);
	}
	std::string Fixture(const char *pName)
	{
		char aCwd[IO_MAX_PATH_LENGTH];
		if(!fs_getcwd(aCwd, sizeof(aCwd)))
			return "";
		return std::string(aCwd) + "/data/fonts/" + pName;
	}
};

TEST_F(LucentFontManager, TrueTypeOpenTypeAndRepeatedFaceRetirement)
{
	CCustomFontFace Font, Icons;
	ASSERT_TRUE(Icons.Load(m_pStorage.get(), m_Library, Fixture("Font_Awesome_6_Free-Solid-900.otf").c_str(), IStorage::TYPE_ABSOLUTE));
	const auto IconGlyph = FT_Get_Char_Index(Icons.Face(), 0xF031);
	ASSERT_NE(IconGlyph, 0u);
	for(int i = 0; i < 12; ++i)
	{
		ASSERT_TRUE(Font.Load(m_pStorage.get(), m_Library, Fixture("DejaVuSans.ttf").c_str(), IStorage::TYPE_ABSOLUTE));
		EXPECT_NE(FT_Get_Char_Index(Font.Face(), 'A'), 0u);
		EXPECT_NE(FT_Get_Char_Index(Font.Face(), 0x0416), 0u); // Cyrillic Zhe
		EXPECT_EQ(FT_Set_Pixel_Sizes(Font.Face(), 0, 18), 0);
		EXPECT_EQ(FT_Load_Char(Font.Face(), 'A', FT_LOAD_RENDER), 0);
		EXPECT_GT(Font.Face()->glyph->bitmap.width, 0u);
		Font.Reset();
		EXPECT_EQ(Font.Face(), nullptr);
		EXPECT_TRUE(Font.Data().empty());
		EXPECT_EQ(FT_Get_Char_Index(Icons.Face(), 0xF031), IconGlyph);
	}
	ASSERT_TRUE(Font.Load(m_pStorage.get(), m_Library, Fixture("GlowSansJ-Compressed-Book.otf").c_str(), IStorage::TYPE_ABSOLUTE));
	EXPECT_TRUE(FT_IS_SCALABLE(Font.Face()));
}

TEST_F(LucentFontManager, MissingCorruptAndOversizedFilesDoNotLeaveFaces)
{
	CCustomFontFace Font;
	EXPECT_FALSE(Font.Load(m_pStorage.get(), m_Library, "missing.ttf", IStorage::TYPE_SAVE));
	IOHANDLE File = m_pStorage->OpenFile("broken.ttf", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_write(File, "not a font", 10), 10u);
	ASSERT_EQ(io_close(File), 0);
	EXPECT_FALSE(Font.Load(m_pStorage.get(), m_Library, "broken.ttf", IStorage::TYPE_SAVE));
	EXPECT_EQ(Font.Face(), nullptr);
	EXPECT_TRUE(Font.Data().empty());
	File = m_pStorage->OpenFile("large.otf", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_seek(File, LucentFonts::MaxFileSize, EIoSeekOrigin::START), 0);
	ASSERT_EQ(io_write(File, "x", 1), 1u);
	ASSERT_EQ(io_close(File), 0);
	EXPECT_FALSE(Font.Load(m_pStorage.get(), m_Library, "large.otf", IStorage::TYPE_SAVE));
	EXPECT_EQ(Font.Face(), nullptr);
	EXPECT_TRUE(Font.Data().empty());
}

TEST_F(LucentFontManager, ImportRoundTripDeduplicationAndCollisionProtection)
{
	char aName[256], aAgain[256], aPath[IO_MAX_PATH_LENGTH];
	const auto Source = Fixture("DejaVuSans.ttf");
	ASSERT_TRUE(ImportCustomFontFile(m_pStorage.get(), m_Library, Source.c_str(), aName, sizeof(aName)));
	EXPECT_TRUE(LucentFonts::ManagedFilename(aName));
	ASSERT_TRUE(ImportCustomFontFile(m_pStorage.get(), m_Library, Source.c_str(), aAgain, sizeof(aAgain)));
	EXPECT_STREQ(aName, aAgain);
	str_format(aPath, sizeof(aPath), "%s/%s", LucentFonts::Directory, aName);
	CCustomFontFace Font;
	ASSERT_TRUE(Font.Load(m_pStorage.get(), m_Library, aPath, IStorage::TYPE_SAVE));
	EXPECT_NE(FT_Get_Char_Index(Font.Face(), 0x044F), 0u);
	Font.Reset();
	// A damaged existing destination must never be overwritten by re-import.
	IOHANDLE File = m_pStorage->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_write(File, "damaged", 7), 7u);
	ASSERT_EQ(io_close(File), 0);
	EXPECT_FALSE(ImportCustomFontFile(m_pStorage.get(), m_Library, Source.c_str(), aAgain, sizeof(aAgain)));
	EXPECT_FALSE(Font.Load(m_pStorage.get(), m_Library, aPath, IStorage::TYPE_SAVE));
	EXPECT_TRUE(m_pStorage->RemoveFile(aPath, IStorage::TYPE_SAVE));
	EXPECT_FALSE(Font.Load(m_pStorage.get(), m_Library, aPath, IStorage::TYPE_SAVE));
	ASSERT_TRUE(ImportCustomFontFile(m_pStorage.get(), m_Library, Fixture("GlowSansJ-Compressed-Book.otf").c_str(), aName, sizeof(aName)));
	EXPECT_TRUE(LucentFonts::Supported(aName));
}

TEST_F(LucentFontManager, UnicodeFilenameImportsIntoSaveDirectory)
{
	CCustomFontFace Source;
	ASSERT_TRUE(Source.Load(m_pStorage.get(), m_Library, Fixture("DejaVuSans.ttf").c_str(), IStorage::TYPE_ABSOLUTE));
	const auto &Data = Source.Data();
	IOHANDLE File = m_pStorage->OpenFile("Шрифт.ttf", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(File, nullptr);
	ASSERT_EQ(io_write(File, Data.data(), Data.size()), Data.size());
	ASSERT_EQ(io_close(File), 0);
	char aRelative[IO_MAX_PATH_LENGTH], aCwd[IO_MAX_PATH_LENGTH], aAbsolute[IO_MAX_PATH_LENGTH], aName[256];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "Шрифт.ttf", aRelative, sizeof(aRelative));
	ASSERT_NE(fs_getcwd(aCwd, sizeof(aCwd)), nullptr);
	if(fs_is_relative_path(aRelative))
		str_format(aAbsolute, sizeof(aAbsolute), "%s/%s", aCwd, aRelative);
	else
		str_copy(aAbsolute, aRelative);
	ASSERT_TRUE(ImportCustomFontFile(m_pStorage.get(), m_Library, aAbsolute, aName, sizeof(aName)));
	EXPECT_EQ(str_comp_num(aName, "Шрифт-", str_length("Шрифт-")), 0);
	EXPECT_TRUE(str_utf8_check(aName));
}

TEST_F(LucentFontManager, OtherClientInitializationDeduplicatesAndLeavesSourcesUntouched)
{
	for(const char *pDir : {"eclient", "eclient/data", "eclient/data/entity", "eclient/data/entity/fonts", "BestClient", "BestClient/fonts"})
		ASSERT_TRUE(m_pStorage->CreateFolder(pDir, IStorage::TYPE_SAVE));
	CCustomFontFace Source;
	ASSERT_TRUE(Source.Load(m_pStorage.get(), m_Library, Fixture("DejaVuSans.ttf").c_str(), IStorage::TYPE_ABSOLUTE));
	const auto Hash = sha256(Source.Data().data(), Source.Data().size());
	for(const char *pFile : {"eclient/data/entity/fonts/Custom.ttf", "BestClient/fonts/Duplicate.ttf", "BestClient/fonts/DejaVuSans.ttf"})
	{
		IOHANDLE File = m_pStorage->OpenFile(pFile, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		ASSERT_EQ(io_write(File, Source.Data().data(), Source.Data().size()), Source.Data().size());
		ASSERT_EQ(io_close(File), 0);
	}
	IOHANDLE Broken = m_pStorage->OpenFile("eclient/data/entity/fonts/Broken.otf", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	ASSERT_NE(Broken, nullptr);
	ASSERT_EQ(io_write(Broken, "invalid font", 12), 12u);
	ASSERT_EQ(io_close(Broken), 0);
	char aCwd[IO_MAX_PATH_LENGTH], aRoot[IO_MAX_PATH_LENGTH];
	ASSERT_NE(fs_getcwd(aCwd, sizeof(aCwd)), nullptr);
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "", aRoot, sizeof(aRoot));
	const std::string Root = fs_is_relative_path(aRoot) ? std::string(aCwd) + "/" + aRoot : aRoot;
	const std::vector<std::string> Directories{Root + "/eclient/data/entity/fonts", Root + "/BestClient/fonts", Root + "/missing"};
	EXPECT_EQ(LucentFonts::ImportOtherClientFonts(m_pStorage.get(), m_Library, Directories), 1);
	EXPECT_EQ(LucentFonts::ImportOtherClientFonts(m_pStorage.get(), m_Library, Directories), 0);
	SHA256_DIGEST SourceAfter;
	ASSERT_TRUE(m_pStorage->CalculateHashes("eclient/data/entity/fonts/Custom.ttf", IStorage::TYPE_SAVE, &SourceAfter));
	EXPECT_EQ(SourceAfter, Hash);
	EXPECT_NE(FT_Get_Char_Index(Source.Face(), 'A'), 0u);
	const auto Found = LucentFonts::OtherClientFontDirectories(m_pStorage.get());
	EXPECT_NE(std::find(Found.begin(), Found.end(), Directories[0]), Found.end());
	EXPECT_NE(std::find(Found.begin(), Found.end(), Directories[1]), Found.end());
	// CTestInfo's generic cleanup intentionally caps fixture entries at ten.
	// Retire our extra peer fixture files/directories before that cleanup.
	for(const char *pFile : {"BestClient/fonts/Duplicate.ttf", "BestClient/fonts/DejaVuSans.ttf", "eclient/data/entity/fonts/Broken.otf"})
		EXPECT_TRUE(m_pStorage->RemoveFile(pFile, IStorage::TYPE_SAVE));
	EXPECT_TRUE(m_pStorage->RemoveFolder("BestClient/fonts", IStorage::TYPE_SAVE));
	EXPECT_TRUE(m_pStorage->RemoveFolder("BestClient", IStorage::TYPE_SAVE));
}

TEST_F(LucentFontManager, ImportRejectsTruncatedOutputAndInvalidInputs)
{
	char aName[256];
	EXPECT_FALSE(ImportCustomFontFile(m_pStorage.get(), m_Library, Fixture("DejaVuSans.ttf").c_str(), aName, 2));
	EXPECT_FALSE(ImportCustomFontFile(m_pStorage.get(), m_Library, Fixture("index.json").c_str(), aName, sizeof(aName)));
	EXPECT_FALSE(ImportCustomFontFile(m_pStorage.get(), m_Library, Fixture("missing.ttf").c_str(), aName, sizeof(aName)));
}
#endif

TEST(LucentFontManagerPolicy, ManagedPathsAndStableSettingsIds)
{
	for(const char *pName : {"Face.ttf", "Face.OTF", "Шрифт.ttf"})
		EXPECT_TRUE(LucentFonts::ManagedFilename(pName));
	for(const char *pName : {"", ".ttf", ".hidden.ttf", "../face.ttf", "path/face.ttf", "path\\face.ttf", "C:face.ttf", "face.ttf.exe", "face.ttc"})
		EXPECT_FALSE(LucentFonts::ManagedFilename(pName));
	int Workspace = LucentSettings::WorkspaceLucent, Page = LucentSettings::PAGE_FONTS, DDNet = 6;
	LucentSettings::NormalizeWorkspace(Workspace, Page, DDNet);
	EXPECT_EQ(Page, LucentSettings::PAGE_FONTS);
	EXPECT_EQ(DDNet, 6);
	EXPECT_EQ(LucentSettings::PAGE_CURSOR, 0);
	EXPECT_EQ(LucentSettings::PAGE_WEAPONS, 1);
	EXPECT_EQ(LucentSettings::PAGE_CAMERA, 2);
}

TEST(LucentFontManagerPolicy, ReadableNamesHideImportMetadata)
{
	EXPECT_EQ(LucentFonts::DisplayName("ComicSansMS-d2177b6128bfd868.ttf"), "Comic Sans MS");
	EXPECT_EQ(LucentFonts::DisplayName("Montserrat-Regular-c3fb0280e4339f6c.ttf"), "Montserrat Regular");
	EXPECT_EQ(LucentFonts::DisplayName("Inter_24pt-SemiBold-10238d4c2f85914e.ttf"), "Inter SemiBold");
	EXPECT_EQ(LucentFonts::DisplayName("GoogleSans-Regular-974ae8402aed7391.ttf"), "Google Sans Regular");
	EXPECT_EQ(LucentFonts::DisplayName("Шрифт-Regular-0123456789abcdef.OTF"), "Шрифт Regular");
	EXPECT_EQ(LucentFonts::DisplayName("Font-1234.ttf"), "Font 1234");
	EXPECT_EQ(LucentFonts::DisplayName("Inter ExtraBold.otf"), "Inter ExtraBold");
}
