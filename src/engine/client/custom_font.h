#ifndef ENGINE_CLIENT_CUSTOM_FONT_H
#define ENGINE_CLIENT_CUSTOM_FONT_H

#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>

#include <engine/shared/font_manager.h>
#include <engine/storage.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vector>

// One independently owned FreeType face. Does not own the library or an atlas.
// A candidate is validated before the renderer changes its active face.
class CCustomFontFace
{
	FT_Face m_Face = nullptr;
	std::vector<unsigned char> m_Data;

public:
	CCustomFontFace() = default;
	CCustomFontFace(const CCustomFontFace &) = delete;
	CCustomFontFace &operator=(const CCustomFontFace &) = delete;
	~CCustomFontFace() { Reset(); }
	void Reset()
	{
		if(m_Face)
			FT_Done_Face(m_Face);
		m_Face = nullptr;
		m_Data.clear();
	}
	FT_Face Face() const { return m_Face; }
	const std::vector<unsigned char> &Data() const { return m_Data; }
	bool Load(IStorage *pStorage, FT_Library Library, const char *pPath, int StorageType)
	{
		Reset();
		IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_READ, StorageType);
		if(!File)
			return false;
		const int64_t Length = io_length(File);
		if(Length <= 0 || Length > LucentFonts::MaxFileSize)
		{
			io_close(File);
			return false;
		}
		m_Data.resize(Length);
		const bool Read = io_read(File, m_Data.data(), m_Data.size()) == m_Data.size();
		const bool Closed = io_close(File) == 0;
		if(!Read || !Closed || FT_New_Memory_Face(Library, m_Data.data(), m_Data.size(), 0, &m_Face) ||
			!FT_IS_SCALABLE(m_Face) || FT_Select_Charmap(m_Face, FT_ENCODING_UNICODE))
		{
			Reset();
			return false;
		}
		return true;
	}
};

inline bool ImportCustomFontFile(IStorage *pStorage, FT_Library Library, const char *pAbsolutePath, char *pFilename, size_t FilenameSize)
{
	if(fs_is_relative_path(pAbsolutePath) || !LucentFonts::Supported(pAbsolutePath))
		return false;
	CCustomFontFace Candidate;
	if(!Candidate.Load(pStorage, Library, pAbsolutePath, IStorage::TYPE_ABSOLUTE))
		return false;
	const auto &Data = Candidate.Data();
	const auto Hash = sha256(Data.data(), Data.size());
	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(Hash, aHash, sizeof(aHash));
	// Keep a readable name; never trust an imported name as a path.
	char aStem[128];
	str_utf8_truncate(aStem, sizeof(aStem), fs_filename(pAbsolutePath), 100);
	if(LucentFonts::Supported(aStem))
		aStem[str_length(aStem) - 4] = 0;
	if(aStem[0] == '.')
		aStem[0] = '_';
	for(char *p = aStem; *p; ++p)
		if(*p == ':' || *p == '/' || *p == '\\' || (unsigned char)*p < 32)
			*p = '_';
	char aName[256], aPath[IO_MAX_PATH_LENGTH];
	str_format(aName, sizeof(aName), "%s-%.16s%s", aStem, aHash, str_comp_nocase(pAbsolutePath + str_length(pAbsolutePath) - 4, ".otf") == 0 ? ".otf" : ".ttf");
	if(!LucentFonts::ManagedFilename(aName) || FilenameSize <= (size_t)str_length(aName))
		return false;
	pStorage->CreateFolder("lucent", IStorage::TYPE_SAVE);
	pStorage->CreateFolder(LucentFonts::Directory, IStorage::TYPE_SAVE);
	str_format(aPath, sizeof(aPath), "%s/%s", LucentFonts::Directory, aName);
	// Identical imports are reusable. Never overwrite an existing file.
	if(pStorage->FileExists(aPath, IStorage::TYPE_SAVE))
	{
		SHA256_DIGEST Existing;
		if(!pStorage->CalculateHashes(aPath, IStorage::TYPE_SAVE, &Existing) || Existing != Hash)
			return false;
	}
	else
	{
		IOHANDLE File = pStorage->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const bool Written = io_write(File, Data.data(), Data.size()) == Data.size();
		const bool Closed = io_close(File) == 0;
		if(!Written || !Closed)
		{
			pStorage->RemoveFile(aPath, IStorage::TYPE_SAVE);
			return false;
		}
	}
	str_copy(pFilename, aName, FilenameSize);
	return true;
}
#endif
