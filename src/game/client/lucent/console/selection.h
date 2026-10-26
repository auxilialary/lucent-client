#ifndef GAME_CLIENT_LUCENT_CONSOLE_SELECTION_H
#define GAME_CLIENT_LUCENT_CONSOLE_SELECTION_H

#include <base/str.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

inline std::pair<float, float> ConsoleCopyPosition(float Width, float Height, float EndX, float EndY, float Size, float Gap)
{
	const float X = EndX + Gap + Size <= Width ? EndX + Gap : EndX - Size - Gap;
	return {std::clamp(X, 0.f, std::max(0.f, Width - Size)), std::clamp(EndY - Size, 0.f, std::max(0.f, Height - Size))};
}

// Entry identities are monotonic and never refer to ring-buffer addresses.
class CLucentConsoleSelection
{
public:
	struct CPosition
	{
		uint64_t m_Entry = 0;
		int m_Character = 0; // Decoded Unicode position, not a byte offset.
		bool operator<(const CPosition &Other) const
		{
			return m_Entry < Other.m_Entry || (m_Entry == Other.m_Entry && m_Character < Other.m_Character);
		}
	};
	CPosition m_Anchor, m_Focus;
	void Clear() { m_Anchor = m_Focus = {}; }
	void Begin(CPosition Position) { m_Anchor = m_Focus = Position; }
	bool HasSelection() const { return m_Anchor.m_Entry && (m_Anchor < m_Focus || m_Focus < m_Anchor); }
	void Evict(uint64_t Entry)
	{
		if(m_Anchor.m_Entry == Entry || m_Focus.m_Entry == Entry)
			Clear();
	}
	std::pair<int, int> Range(uint64_t Entry, int Characters) const
	{
		const auto First = std::min(m_Anchor, m_Focus), Last = std::max(m_Anchor, m_Focus);
		if(!HasSelection() || Entry < First.m_Entry || Entry > Last.m_Entry)
			return {0, 0};
		return {Entry == First.m_Entry ? std::clamp(First.m_Character, 0, Characters) : 0,
			Entry == Last.m_Entry ? std::clamp(Last.m_Character, 0, Characters) : Characters};
	}
	void Append(std::string &Text, bool &FirstEntry, uint64_t Entry, const char *pText, int Characters) const
	{
		const auto First = std::min(m_Anchor, m_Focus), Last = std::max(m_Anchor, m_Focus);
		if(!HasSelection() || Entry < First.m_Entry || Entry > Last.m_Entry)
			return;
		const auto [Start, End] = Range(Entry, Characters);
		if(!FirstEntry)
			Text += '\n';
		FirstEntry = false;
		const size_t BeginByte = str_utf8_offset_chars_to_bytes(pText, Start);
		const size_t EndByte = str_utf8_offset_chars_to_bytes(pText, End);
		Text.append(pText + BeginByte, EndByte - BeginByte);
	}
};
#endif
