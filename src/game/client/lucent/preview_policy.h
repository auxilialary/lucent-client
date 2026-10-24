#ifndef GAME_CLIENT_LUCENT_PREVIEW_POLICY_H
#define GAME_CLIENT_LUCENT_PREVIEW_POLICY_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace LucentPreview
{
	inline bool PngDimensions(const unsigned char *pData, size_t Size, uint32_t &W, uint32_t &H)
	{
		const unsigned char aSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
		if(Size < 33 || Size > 4 * 1024 * 1024 || std::memcmp(pData, aSignature, 8) || std::memcmp(pData + 12, "IHDR", 4))
			return false;
		const auto Read = [](const unsigned char *p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; };
		if(Read(pData + 8) != 13)
			return false;
		W = Read(pData + 16);
		H = Read(pData + 20);
		return W >= 8 && H >= 8 && W <= 4096 && H <= 4096 && uint64_t(W) * H <= 8 * 1024 * 1024;
	}
	inline bool ThumbnailUrl(std::string_view Url)
	{
		return Url.size() < 256 && Url.starts_with("https://ddnet.org/ranks/maps/") && Url.ends_with(".png") && Url.find("..") == std::string_view::npos && Url.find_first_of("?#\\") == std::string_view::npos;
	}
	// Both displayed slots are pinned during a crossfade.
	template<typename T>
	int EvictionSlot(const T &Entries, int Current, int Previous)
	{
		int Slot = -1;
		for(int i = 0; i < int(Entries.size()); ++i)
			if(i != Current && i != Previous && (Slot < 0 || Entries[i].m_Used < Entries[Slot].m_Used))
				Slot = i;
		return Slot;
	}
	template<typename T>
	int Find(const T &Entries, std::string_view Key)
	{
		for(int i = 0; i < int(Entries.size()); ++i)
			if(Entries[i].m_Key == Key && Entries[i].m_Texture.IsValid())
				return i;
		return -1;
	}
	class CFailures
	{
		struct CFailure
		{
			std::string m_Key;
			double m_Until = 0;
		};
		std::array<CFailure, 16> m_aEntries;

	public:
		void Add(std::string_view Key, double Now)
		{
			auto &Failure = *std::min_element(m_aEntries.begin(), m_aEntries.end(), [](const auto &A, const auto &B) { return A.m_Until < B.m_Until; });
			Failure.m_Key.assign(Key);
			Failure.m_Until = Now + 60;
		}
		bool Contains(std::string_view Key, double Now) const
		{
			for(const auto &Failure : m_aEntries)
				if(Failure.m_Key == Key && Now < Failure.m_Until)
					return true;
			return false;
		}
	};
}
#endif
