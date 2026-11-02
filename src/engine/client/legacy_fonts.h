#ifndef ENGINE_CLIENT_LEGACY_FONTS_H
#define ENGINE_CLIENT_LEGACY_FONTS_H

#include "custom_font.h"

#include <base/log.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace LucentFonts
{
	inline std::vector<std::string> OtherClientFontDirectories(IStorage *pStorage)
	{
		std::set<std::string> Roots, Directories;
		char aCwd[IO_MAX_PATH_LENGTH];
		if(!fs_getcwd(aCwd, sizeof(aCwd)))
			return {};
		const auto Absolute = [&](const char *pPath) {
			return fs_is_relative_path(pPath) ? std::string(aCwd) + "/" + pPath : std::string(pPath);
		};
		Roots.insert(aCwd);
		char aPath[IO_MAX_PATH_LENGTH];
		pStorage->GetBinaryPathAbsolute("", aPath, sizeof(aPath));
		Roots.insert(Absolute(aPath));
		for(int i = 0; i < pStorage->NumPaths(); ++i)
		{
			pStorage->GetCompletePath(i, "", aPath, sizeof(aPath));
			Roots.insert(Absolute(aPath));
		}
		for(const char *pClient : {"DDNet", "Teeworlds", "EClient", "BestClient", "Best Client"})
			if(fs_storage_path(pClient, aPath, sizeof(aPath)) == 0 && fs_is_dir(aPath))
				Roots.insert(aPath);
		// Bounded neighbour lookup, not a recursive scan of the user's disk.
		// Covers portable clients and sibling source trees (including CLion builds).
		const auto Anchors = Roots;
		for(const auto &Root : Anchors)
		{
			str_copy(aPath, Root.c_str());
			for(int Level = 0; Level < 4; ++Level)
			{
				for(const char *pClient : {"eclient", "EClient", "bestclient", "BestClient", "best-client", "Best Client"})
				{
					const std::string Candidate = std::string(aPath) + "/" + pClient;
					if(fs_is_dir(Candidate.c_str()))
						Roots.insert(Candidate);
				}
				if(fs_parent_dir(aPath) != 0)
					break;
			}
		}
		for(const auto &Root : Roots)
			for(const char *pSuffix : {"entity/fonts", "data/entity/fonts", "fonts", "data/fonts", "custom_fonts", "data/custom_fonts"})
			{
				const auto Directory = Root + "/" + pSuffix;
				if(fs_is_dir(Directory.c_str()))
					Directories.insert(Directory);
			}
		return {Directories.begin(), Directories.end()};
	}

	inline bool BundledFont(const char *pName)
	{
		for(const char *pBuiltin : {"DejaVuSans.ttf", "GlowSansJ-Compressed-Book.otf", "Font_Awesome_6_Free-Solid-900.otf", "NotoEmoji-Regular.ttf"})
			if(str_comp_nocase(pName, pBuiltin) == 0)
				return true;
		return false;
	}

	inline int ImportOtherClientFonts(IStorage *pStorage, FT_Library Library, const std::vector<std::string> &Directories)
	{
		std::set<std::string> KnownHashes;
		struct CFiles
		{
			std::vector<std::string> m_Names;
			static int Collect(const char *pName, int IsDir, int Type, void *pUser)
			{
				if(!IsDir && ManagedFilename(pName))
					static_cast<CFiles *>(pUser)->m_Names.emplace_back(pName);
				return 0;
			}
		};
		CFiles Existing;
		pStorage->ListDirectory(IStorage::TYPE_SAVE, Directory, CFiles::Collect, &Existing);
		for(const auto &Name : Existing.m_Names)
		{
			SHA256_DIGEST Digest;
			char aHash[SHA256_MAXSTRSIZE];
			if(pStorage->CalculateHashes((std::string(Directory) + "/" + Name).c_str(), IStorage::TYPE_SAVE, &Digest))
			{
				sha256_str(Digest, aHash, sizeof(aHash));
				KnownHashes.insert(aHash);
			}
		}
		int Imported = 0;
		for(const auto &Source : Directories)
		{
			CFiles Files;
			fs_listdir(Source.c_str(), CFiles::Collect, IStorage::TYPE_ABSOLUTE, &Files);
			std::sort(Files.m_Names.begin(), Files.m_Names.end());
			for(const auto &Name : Files.m_Names)
			{
				if(BundledFont(Name.c_str()))
					continue;
				const auto Path = Source + "/" + Name;
				CCustomFontFace Candidate;
				if(!Candidate.Load(pStorage, Library, Path.c_str(), IStorage::TYPE_ABSOLUTE))
					continue;
				char aHash[SHA256_MAXSTRSIZE], aFilename[256];
				sha256_str(sha256(Candidate.Data().data(), Candidate.Data().size()), aHash, sizeof(aHash));
				if(KnownHashes.count(aHash))
					continue;
				if(ImportCustomFontFile(pStorage, Library, Path.c_str(), aFilename, sizeof(aFilename)))
				{
					KnownHashes.insert(aHash);
					++Imported;
					log_info("fonts", "Imported '%s' as '%s'", Path.c_str(), aFilename);
				}
			}
		}
		return Imported;
	}
}
#endif
