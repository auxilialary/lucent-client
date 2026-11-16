#ifndef ENGINE_SHARED_SOUND_PACK_H
#define ENGINE_SHARED_SOUND_PACK_H

#include <base/io.h>
#include <base/str.h>

#include <engine/storage.h>

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LucentSoundPacks
{
	inline constexpr const char *Directory = "lucent/sounds";
	inline constexpr size_t MaxArchive = 64 * 1024 * 1024, MaxFile = 16 * 1024 * 1024, MaxExpanded = 128 * 1024 * 1024;
	using CBytes = std::vector<unsigned char>;
	struct CEntry
	{
		std::string m_Path;
		CBytes m_Data;
	};
	inline bool Supported(const char *pName)
	{
		return str_endswith_nocase(pName, ".wv") || str_endswith_nocase(pName, ".opus");
	}
	inline bool SafeManagedPrefix(const std::string &Name)
	{
		for(unsigned char C : Name)
			if(!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C >= 128))
				return false;
		return !Name.empty() && Name.front() != '.' && str_utf8_check(Name.c_str());
	}
	inline std::string DisplayName(const std::string &Name)
	{
		return Name.empty() ? "Default" : Name.substr(0, Name.size() - 69);
	}
	inline bool Managed(const char *pName)
	{
		const std::string Name = pName;
		if(Name.size() < 70 || Name.size() > 120 || !SafeManagedPrefix(Name))
			return false;
		const size_t Hash = Name.size() - 68;
		if(Name[Hash - 1] != '-' || Name.substr(Name.size() - 4) != ".zip")
			return false;
		for(size_t i = Hash; i < Name.size() - 4; ++i)
			if(!((Name[i] >= '0' && Name[i] <= '9') || (Name[i] >= 'a' && Name[i] <= 'f')))
				return false;
		return true;
	}
	inline bool SafePath(const std::string &Path)
	{
		if(Path.empty() || Path.size() > 512 || Path.front() == '/')
			return false;
		for(unsigned char C : Path)
			if(C < 32 || C == 127 || C == '\\' || C == ':')
				return false;
		size_t Start = 0;
		while(Start < Path.size())
		{
			const auto End = Path.find('/', Start);
			const auto Part = Path.substr(Start, End == std::string::npos ? End : End - Start);
			if(Part.empty() || Part == "." || Part == ".." || Part.back() == '.' || Part.back() == ' ')
				return false;
			Start = End == std::string::npos ? Path.size() : End + 1;
		}
		return true;
	}
	inline bool Read(IStorage *pStorage, const char *pPath, int Type, CBytes &Bytes, size_t Limit = MaxArchive)
	{
		const auto File = pStorage->OpenFile(pPath, IOFLAG_READ, Type);
		if(!File)
			return false;
		const auto Size = io_length(File);
		bool Ok = Size > 0 && Size <= (int64_t)Limit;
		if(Ok)
		{
			Bytes.resize(Size);
			Ok = io_read(File, Bytes.data(), Bytes.size()) == Bytes.size();
		}
		io_close(File);
		return Ok;
	}
	inline bool ImportSupported(const char *pName)
	{
		return str_endswith_nocase(pName, ".zip") || str_endswith_nocase(pName, ".wv");
	}
	// Self-contained snapshots use standard ZIP Store, so presets never depend on source files.
	inline bool Encode(const std::vector<CEntry> &Entries, CBytes &Zip)
	{
		Zip.clear();
		if(Entries.empty() || Entries.size() > 2048)
			return false;
		CBytes Central;
		std::set<std::string> Paths;
		const auto Put = [](CBytes &Out, uint32_t Value, int Count) {
			for(int i = 0; i < Count; ++i)
				Out.push_back(Value >> (8 * i));
		};
		for(const auto &Entry : Entries)
		{
			const auto &Name = Entry.m_Path;
			const auto &Data = Entry.m_Data;
			if(!Supported(Name.c_str()) || !SafePath(Name) || !Paths.insert(Name).second || Data.empty() || Data.size() > MaxFile || Zip.size() + Data.size() + Central.size() + 2 * Name.size() + 98 > MaxArchive)
				return false;
			const auto Offset = Zip.size();
			const auto Crc = crc32(0, Data.data(), Data.size());
			Put(Zip, 0x04034b50, 4);
			Put(Zip, 20, 2);
			Put(Zip, 0x800, 2);
			Put(Zip, 0, 2);
			Put(Zip, 0, 4);
			Put(Zip, Crc, 4);
			Put(Zip, Data.size(), 4);
			Put(Zip, Data.size(), 4);
			Put(Zip, Name.size(), 2);
			Put(Zip, 0, 2);
			Zip.insert(Zip.end(), Name.begin(), Name.end());
			Zip.insert(Zip.end(), Data.begin(), Data.end());
			Put(Central, 0x02014b50, 4);
			Put(Central, 20, 2);
			Put(Central, 20, 2);
			Put(Central, 0x800, 2);
			Put(Central, 0, 2);
			Put(Central, 0, 4);
			Put(Central, Crc, 4);
			Put(Central, Data.size(), 4);
			Put(Central, Data.size(), 4);
			Put(Central, Name.size(), 2);
			Put(Central, 0, 4);
			Put(Central, 0, 4);
			Put(Central, 0, 4);
			Put(Central, Offset, 4);
			Central.insert(Central.end(), Name.begin(), Name.end());
		}
		const auto Offset = Zip.size();
		Zip.insert(Zip.end(), Central.begin(), Central.end());
		Put(Zip, 0x06054b50, 4);
		Put(Zip, 0, 4);
		Put(Zip, Entries.size(), 2);
		Put(Zip, Entries.size(), 2);
		Put(Zip, Central.size(), 4);
		Put(Zip, Offset, 4);
		Put(Zip, 0, 2);
		return true;
	}
	inline bool WrapWv(const std::string &Name, const CBytes &Data, CBytes &Zip)
	{
		if(!str_endswith_nocase(Name.c_str(), ".wv") || Name.find('/') != std::string::npos)
			return false;
		return Encode({{Name, Data}}, Zip);
	}
	// Incoming entries override the same standard filename while retaining other custom sounds.
	inline std::vector<CEntry> Merge(const std::vector<CEntry> &Current, const std::vector<CEntry> &Incoming)
	{
		auto Result = Current;
		for(const auto &Entry : Incoming)
		{
			const auto It = std::find_if(Result.begin(), Result.end(), [&](const CEntry &Old) {
				return str_comp_nocase(Old.m_Path.substr(0, Old.m_Path.rfind('.')).c_str(), Entry.m_Path.substr(0, Entry.m_Path.rfind('.')).c_str()) == 0;
			});
			if(It == Result.end())
				Result.push_back(Entry);
			else
				*It = Entry;
		}
		return Result;
	}
	inline std::string Slug(const char *pLabel)
	{
		std::string Name;
		for(unsigned char C : std::string(pLabel))
		{
			if(Name.size() == 40 || C == '.')
				break;
			Name += ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '-' || C >= 128) ? char(C) : '_';
		}
		while(!Name.empty() && !str_utf8_check(Name.c_str()))
			Name.pop_back();
		return Name.empty() ? "Pack" : Name;
	}
	enum ECategory
	{
		ALL,
		WEAPONS,
		HOOK,
		JUMPS,
		MOVEMENT,
		CHARACTER,
		PICKUPS,
		CHAT,
		CTF,
		MUSIC,
		CATEGORY_COUNT
	};
	inline constexpr const char *CategoryNames[] = {"All", "Weapons", "Hook", "Jumps", "Movement", "Character", "Pickups", "Chat", "CTF", "Music"};
	struct CSoundDescription
	{
		const char *m_pSet;
		ECategory m_Category;
		const char *m_pLabel;
	};
	inline constexpr CSoundDescription Descriptions[] = {
		{"gun_fire", WEAPONS, "Gun: shot"}, {"shotgun_fire", WEAPONS, "Shotgun: shot"}, {"grenade_fire", WEAPONS, "Grenade: launch"},
		{"hammer_fire", WEAPONS, "Hammer: swing"}, {"hammer_hit", WEAPONS, "Hammer: hit"}, {"ninja_fire", WEAPONS, "Ninja: attack"},
		{"grenade_explode", WEAPONS, "Grenade: explosion"}, {"ninja_hit", WEAPONS, "Ninja: hit"}, {"laser_fire", WEAPONS, "Laser: shot"},
		{"laser_bounce", WEAPONS, "Laser: bounce"}, {"weapon_switch", WEAPONS, "Weapon switch"}, {"weapon_noammo", WEAPONS, "No ammo"}, {"hit", WEAPONS, "Hit feedback"},
		{"hook_loop", HOOK, "Hook: flying"}, {"hook_attach_ground", HOOK, "Hook: attach to ground"}, {"hook_attach_player", HOOK, "Hook: attach to player"}, {"hook_noattach", HOOK, "Hook: failed attach"},
		{"player_jump", JUMPS, "Jump"}, {"player_airjump", JUMPS, "Air jump"}, {"body_land", MOVEMENT, "Landing"}, {"player_skid", MOVEMENT, "Skidding"},
		{"player_pain_short", CHARACTER, "Pain: short"}, {"player_pain_long", CHARACTER, "Pain: long"}, {"player_die", CHARACTER, "Death"}, {"player_spawn", CHARACTER, "Spawn"}, {"tee_cry", CHARACTER, "Cry"},
		{"pickup_health", PICKUPS, "Health pickup"}, {"pickup_armor", PICKUPS, "Armor pickup"}, {"pickup_grenade", PICKUPS, "Grenade pickup"}, {"pickup_shotgun", PICKUPS, "Shotgun pickup"}, {"pickup_ninja", PICKUPS, "Ninja pickup"}, {"weapon_spawn", PICKUPS, "Weapon spawn"},
		{"chat_server", CHAT, "Server message"}, {"chat_client", CHAT, "Chat message"}, {"chat_highlight", CHAT, "Chat highlight"},
		{"ctf_drop", CTF, "Flag dropped"}, {"ctf_return", CTF, "Flag returned"}, {"ctf_grab_pl", CTF, "Your flag taken"}, {"ctf_grab_en", CTF, "Enemy flag taken"}, {"ctf_capture", CTF, "Flag captured"}, {"menu", MUSIC, "Menu music"}};
	inline CSoundDescription Describe(const char *pSet)
	{
		for(const auto &Description : Descriptions)
			if(str_comp(Description.m_pSet, pSet) == 0)
				return Description;
		return {pSet, CHARACTER, pSet};
	}
	// The caller validates and decodes before publishing. Only generated filenames are accepted.
	inline bool Save(IStorage *pStorage, const char *pName, const CBytes &Zip)
	{
		if(!Managed(pName) || Zip.empty() || Zip.size() > MaxArchive)
			return false;
		pStorage->CreateFolder("lucent", IStorage::TYPE_SAVE);
		pStorage->CreateFolder(Directory, IStorage::TYPE_SAVE);
		const std::string Path = std::string(Directory) + "/" + pName, Temp = Path + ".tmp";
		const auto File = pStorage->OpenFile(Temp.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const bool Written = io_write(File, Zip.data(), Zip.size()) == Zip.size();
		const bool Closed = io_close(File) == 0;
		if(!Written || !Closed || !pStorage->RenameFile(Temp.c_str(), Path.c_str(), IStorage::TYPE_SAVE))
		{
			pStorage->RemoveFile(Temp.c_str(), IStorage::TYPE_SAVE);
			return false;
		}
		return true;
	}
	// Only ordinary single-disk ZIP Store/Deflate archives. No filesystem extraction.
	inline bool Parse(const CBytes &Zip, std::vector<CEntry> &Entries)
	{
		Entries.clear();
		if(Zip.size() < 22 || Zip.size() > MaxArchive)
			return false;
		const auto U16 = [&](size_t P) -> uint32_t { return Zip[P] | (uint32_t(Zip[P + 1]) << 8); };
		const auto U32 = [&](size_t P) -> uint32_t { return U16(P) | (U16(P + 2) << 16); };
		size_t End = Zip.size() - 22;
		while(U32(End) != 0x06054b50 || End + 22 + U16(End + 20) != Zip.size())
		{
			if(End == 0 || Zip.size() - End > 65557)
				return false;
			--End;
		}
		const size_t Count = U16(End + 10), CentralSize = U32(End + 12), Central = U32(End + 16);
		if(U16(End + 4) || U16(End + 6) || U16(End + 8) != Count || !Count || Count > 2048 || Central > End || CentralSize != End - Central)
			return false;
		size_t Pos = Central, Total = 0;
		std::set<std::string> Paths;
		std::vector<CEntry> Result;
		for(size_t i = 0; i < Count; ++i)
		{
			if(Pos + 46 > End || U32(Pos) != 0x02014b50)
				return false;
			const size_t Flags = U16(Pos + 8), Method = U16(Pos + 10), Compressed = U32(Pos + 20), Size = U32(Pos + 24);
			const size_t NameSize = U16(Pos + 28), Extra = U16(Pos + 30), Comment = U16(Pos + 32), Local = U32(Pos + 42);
			const auto Crc = U32(Pos + 16), Mode = U32(Pos + 38) >> 16;
			if(Pos + 46 + NameSize + Extra + Comment > End || U16(Pos + 34) || (Flags & ~0x808) || (Method != 0 && Method != 8) || Size > MaxFile || Compressed > MaxArchive || Total + Size > MaxExpanded || ((Mode & 0170000) != 0 && (Mode & 0170000) != 0100000 && (Mode & 0170000) != 0040000))
				return false;
			std::string Path(reinterpret_cast<const char *>(Zip.data() + Pos + 46), NameSize);
			if(!SafePath(Path) || !Paths.insert(Path).second || Local > Central || Central - Local < 30 || U32(Local) != 0x04034b50 || U16(Local + 6) != Flags || U16(Local + 8) != Method || U16(Local + 26) != NameSize)
				return false;
			const size_t Data = Local + 30 + NameSize + U16(Local + 28);
			if(Data > Central || Compressed > Central - Data || std::string(reinterpret_cast<const char *>(Zip.data() + Local + 30), NameSize) != Path)
				return false;
			if(!(Flags & 8) && (U32(Local + 14) != Crc || U32(Local + 18) != Compressed || U32(Local + 22) != Size))
				return false;
			CBytes Bytes(Size + 1); // Also gives empty deflate streams output space.
			if(Method == 0)
			{
				if(Compressed != Size)
					return false;
				std::copy(Zip.begin() + Data, Zip.begin() + Data + Size, Bytes.begin());
			}
			else
			{
				z_stream Stream{};
				Stream.next_in = const_cast<Bytef *>(Zip.data() + Data);
				Stream.avail_in = Compressed;
				Stream.next_out = Bytes.data();
				Stream.avail_out = Bytes.size();
				if(inflateInit2(&Stream, -MAX_WBITS) != Z_OK)
					return false;
				const int Status = inflate(&Stream, Z_FINISH);
				const bool Valid = Status == Z_STREAM_END && Stream.total_out == Size && Stream.total_in == Compressed;
				inflateEnd(&Stream);
				if(!Valid)
					return false;
			}
			Bytes.resize(Size);
			if(crc32(0, Bytes.data(), Bytes.size()) != Crc)
				return false;
			Total += Size;
			if(Supported(Path.c_str()))
			{
				if(!Size)
					return false;
				Result.push_back({Path, std::move(Bytes)});
			}
			Pos += 46 + NameSize + Extra + Comment;
		}
		if(Pos != End || Result.empty())
			return false;
		Entries = std::move(Result);
		return true;
	}
	inline std::string Stem(std::string Path)
	{
		Path = Path.substr(Path.find_last_of('/') == std::string::npos ? 0 : Path.find_last_of('/') + 1);
		Path.resize(Path.rfind('.'));
		for(char &C : Path)
			if(C >= 'A' && C <= 'Z')
				C += 'a' - 'A';
		return Path;
	}
	// Match generated DDNet filenames, accepting audio/, data/audio/ and pack wrappers.
	inline bool Match(const std::vector<CEntry> &Entries, const std::vector<std::string> &Catalog, std::map<size_t, const CEntry *> &Matches)
	{
		Matches.clear();
		for(const auto &Entry : Entries)
		{
			const auto Name = Stem(Entry.m_Path);
			for(size_t i = 0; i < Catalog.size(); ++i)
				if(Name == Stem(Catalog[i]))
				{
					if(!Matches.emplace(i, &Entry).second)
						return false; // Never depend on archive order for duplicate replacements.
				}
		}
		return !Matches.empty();
	}
	inline bool BuildSingleImport(const std::vector<CEntry> &Current, const std::string &Name, const CBytes &Data, const std::vector<std::string> &Catalog, CBytes &Zip)
	{
		if(!str_endswith_nocase(Name.c_str(), ".wv") || !SafePath(Name) || Name.find('/') != std::string::npos || Data.empty() || Data.size() > MaxFile)
			return false;
		std::vector<CEntry> Incoming = {{Name, Data}};
		std::map<size_t, const CEntry *> Matches;
		if(!Match(Incoming, Catalog, Matches) || Matches.size() != 1)
			return false;
		Incoming[0].m_Path = Catalog[Matches.begin()->first];
		return Encode(Merge(Current, Incoming), Zip);
	}
	// Owns only overrides. Defaults remain resident and are never unloaded here.
	class CSelection
	{
		std::map<size_t, int> m_Samples;

	public:
		const std::map<size_t, int> &Samples() const { return m_Samples; }
		template<typename TUnload, typename TCommit>
		bool Adopt(CSelection &Pending, TUnload Unload, TCommit Commit)
		{
			if(&Pending == this || !Commit(Pending.m_Samples))
				return false;
			for(const auto &[Index, Id] : m_Samples)
				Unload(Id);
			m_Samples = std::move(Pending.m_Samples);
			Pending.m_Samples.clear();
			return true;
		}
		template<typename TLoad, typename TUnload, typename TCommit>
		bool Replace(const std::map<size_t, const CEntry *> &Matches, TLoad Load, TUnload Unload, TCommit Commit)
		{
			std::map<size_t, int> Pending;
			for(const auto &[Index, pEntry] : Matches)
			{
				const int Id = Load(*pEntry);
				if(Id < 0)
				{
					for(const auto &[Unused, Sample] : Pending)
						Unload(Sample);
					return false;
				}
				Pending.emplace(Index, Id);
			}
			if(!Commit(Pending))
			{
				for(const auto &[Unused, Sample] : Pending)
					Unload(Sample);
				return false;
			}
			for(const auto &[Unused, Sample] : m_Samples)
				Unload(Sample);
			m_Samples = std::move(Pending);
			return true;
		}
	};
}
#endif
