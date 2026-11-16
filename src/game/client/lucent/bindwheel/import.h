#ifndef GAME_CLIENT_LUCENT_BINDWHEEL_IMPORT_H
#define GAME_CLIENT_LUCENT_BINDWHEEL_IMPORT_H

#include "model.h"

#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/storage.h>

#include <filesystem>
#include <set>

namespace LucentBindWheel
{
	struct CSource
	{
		std::string m_Path;
		CImport m_Data;
	};
	inline const char *SourceClient(const CSource &Source)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_copy(aPath, Source.m_Path.c_str());
		fs_normalize_path(aPath);
		if(str_comp_nocase(fs_filename(aPath), "entity_bindwheel.cfg") == 0)
			return "E Client";
		if(str_find_nocase(aPath, "/bestclient/") || str_find_nocase(aPath, "/best-client/") || str_find_nocase(aPath, "/best client/"))
			return "Best Client";
		if(str_find_nocase(aPath, "/tclient/") || str_find_nocase(aPath, "/taterclient/"))
			return "T Client";
		if(str_comp_nocase(fs_filename(aPath), "settings_tclient.cfg") == 0)
			return "T / Best Client";
		return "Other client";
	}
	inline const char *SourceLocation(const CSource &Source)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_copy(aPath, Source.m_Path.c_str());
		fs_normalize_path(aPath);
		if(str_find_nocase(aPath, "/AppData/Roaming/") || str_find(aPath, "/.local/share/"))
			return "Saved settings";
		if(str_find_nocase(aPath, "/AppData/Local/"))
			return "Local settings";
		return "Client folder";
	}
	inline int SuggestedSource(const std::vector<CSource> &Sources)
	{
		for(int i = 0; i < int(Sources.size()); ++i)
			if(!Sources[i].m_Data.m_Binds.empty() && Sources[i].m_Data.m_Errors == 0)
				return i;
		return Sources.empty() ? -1 : 0;
	}
	inline bool SameSourceFile(const std::string &A, const std::string &B)
	{
		std::error_code Error;
		return std::filesystem::equivalent(std::filesystem::u8path(A), std::filesystem::u8path(B), Error) && !Error;
	}
	inline bool ReadConfig(const std::string &Path, CImport &Data)
	{
		IOHANDLE File = io_open(Path.c_str(), IOFLAG_READ);
		if(!File)
			return false;
		const int64_t Length = io_length(File);
		if(Length < 0 || Length > 4 * 1024 * 1024)
		{
			io_close(File);
			return false;
		}
		std::string Text(size_t(Length), '\0');
		const bool Read = io_read(File, Text.data(), unsigned(Length)) == unsigned(Length);
		io_close(File);
		if(!Read || Text.find('\0') != std::string::npos)
			return false;
		Parse(Text, Data);
		return true;
	}
	inline bool LoadSource(const std::string &Path, CSource &Source)
	{
		Source = {Path, {}};
		if(!ReadConfig(Path, Source.m_Data))
			return false;
		// Bind keys and wheel preferences live in companion files for E Client.
		char aParent[IO_MAX_PATH_LENGTH];
		str_copy(aParent, Path.c_str());
		if(fs_parent_dir(aParent) == 0)
			for(const char *pName : {"settings_ddnet.cfg", "settings_entity.cfg", "settings_BestClient.cfg"})
			{
				const std::string Companion = std::string(aParent) + "/" + pName;
				if(Companion == Path)
					continue;
				CImport Extra;
				if(!ReadConfig(Companion, Extra))
					continue;
				Source.m_Data.m_Keys.insert(Source.m_Data.m_Keys.end(), Extra.m_Keys.begin(), Extra.m_Keys.end());
				if(Extra.m_ResetMouse >= 0)
					Source.m_Data.m_ResetMouse = Extra.m_ResetMouse;
				if(Extra.m_Animation >= 0)
					Source.m_Data.m_Animation = Extra.m_Animation;
				if(Extra.m_Scale >= 0)
					Source.m_Data.m_Scale = Extra.m_Scale;
			}
		return !Source.m_Data.m_Binds.empty();
	}
	inline std::vector<CSource> DiscoverInRoots(const std::set<std::string> &Roots)
	{
		std::set<std::string> Paths;
		for(const auto &Root : Roots)
			for(const char *pSuffix : {"", "/data", "/config", "/configs"})
				for(const char *pFile : {"entity_bindwheel.cfg", "settings_tclient.cfg"})
				{
					const auto Candidate = Root + pSuffix + "/" + pFile;
					char aNormalized[IO_MAX_PATH_LENGTH];
					str_copy(aNormalized, Candidate.c_str());
					fs_normalize_path(aNormalized);
					if(fs_is_file(aNormalized))
						Paths.insert(aNormalized);
				}
		std::vector<CSource> Sources;
		for(const auto &Path : Paths)
		{
			bool Duplicate = false;
			for(const auto &Existing : Sources)
			{
				if(SameSourceFile(Path, Existing.m_Path))
				{
					Duplicate = true;
					break;
				}
			}
			if(Duplicate)
				continue;
			CSource Source;
			if(LoadSource(Path, Source))
				Sources.push_back(std::move(Source));
		}
		return Sources;
	}
	inline std::vector<CSource> Discover(IStorage *pStorage)
	{
		std::set<std::string> Roots;
		char aCwd[IO_MAX_PATH_LENGTH], aPath[IO_MAX_PATH_LENGTH];
		if(!fs_getcwd(aCwd, sizeof(aCwd)))
			return {};
		const auto AddRoot = [&](const char *pPath) { Roots.insert(fs_is_relative_path(pPath) ? std::string(aCwd) + "/" + pPath : pPath); };
		AddRoot(aCwd);
		pStorage->GetBinaryPathAbsolute("", aPath, sizeof(aPath));
		AddRoot(aPath);
		for(int i = 0; i < pStorage->NumPaths(); ++i)
		{
			pStorage->GetCompletePath(i, "", aPath, sizeof(aPath));
			AddRoot(aPath);
		}
		for(const char *pClient : {"DDNet", "Teeworlds", "EClient", "EntityClient", "TClient", "TaterClient", "BestClient", "Best Client"})
			if(fs_storage_path(pClient, aPath, sizeof(aPath)) == 0 && fs_is_dir(aPath))
				AddRoot(aPath);
		const auto Anchors = Roots;
		for(const auto &Root : Anchors)
		{
			str_copy(aPath, Root.c_str());
			for(int Level = 0; Level < 4; ++Level)
			{
				for(const char *pClient : {"eclient", "EClient", "EntityClient", "tclient", "TClient", "TaterClient", "bestclient", "BestClient", "best-client", "Best Client"})
				{
					const auto Candidate = std::string(aPath) + "/" + pClient;
					if(fs_is_dir(Candidate.c_str()))
						Roots.insert(Candidate);
				}
				if(fs_parent_dir(aPath) != 0)
					break;
			}
		}
		return DiscoverInRoots(Roots);
	}
}
#endif
