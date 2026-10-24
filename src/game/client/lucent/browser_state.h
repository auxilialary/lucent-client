#ifndef GAME_CLIENT_LUCENT_BROWSER_STATE_H
#define GAME_CLIENT_LUCENT_BROWSER_STATE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

class CLucentBrowserState
{
public:
	enum class EPanel
	{
		NONE,
		FILTERS,
		DETAILS,
		PLAYERS,
		FRIENDS,
		COMMUNITIES
	};
	EPanel m_Panel = EPanel::NONE;
	EPanel m_VisiblePanel = EPanel::NONE;
	EPanel VisiblePanel() const { return m_VisiblePanel; }
	int m_Focus = 0; // list, search, filters, details, players, address, connect
	void Tab(bool Reverse) { m_Focus = (m_Focus + (Reverse ? 6 : 1)) % 7; }
	float m_Open = 0;
	float m_Content = 1;
	float m_OpenTime = 0;
	float m_ContentTime = .28f;
	void Toggle(EPanel Panel)
	{
		m_Panel = m_Panel == Panel ? EPanel::NONE : Panel;
		if(m_Panel != EPanel::NONE)
		{
			m_VisiblePanel = m_Panel;
			m_Content = 0;
			m_ContentTime = 0;
		}
	}
	bool Escape()
	{
		if(m_Panel == EPanel::NONE)
			return false;
		m_Panel = EPanel::NONE;
		return true;
	}
	void Update(float Dt, bool Animate)
	{
		const bool Open = m_Panel != EPanel::NONE;
		const float Step = std::clamp(Dt, 0.f, .1f);
		m_OpenTime = Animate ? std::clamp(m_OpenTime + (Open ? Step : -Step), 0.f, .28f) : (Open ? .28f : 0.f);
		m_ContentTime = Animate ? std::min(.28f, m_ContentTime + Step) : .28f;
		const auto Ease = [](float Time) {
			const float T = Time / .28f;
			return 1.f - (1.f - T) * (1.f - T) * (1.f - T);
		};
		m_Open = Ease(m_OpenTime);
		m_Content = Ease(m_ContentTime);
		if(m_Open == 0)
			m_VisiblePanel = EPanel::NONE;
	}
	float PanelWidth(float Width) const { return std::min(290.f, Width * .43f) * m_Open; }
	template<typename T>
	static int FindSelection(int Count, std::string_view Address, T GetAddress)
	{
		for(int i = 0; i < Count; ++i)
			if(Address == GetAddress(i))
				return i;
		return -1;
	}
	// Prefer first-party community metadata, then a canonical DDNet identity.
	// Unknown identities remain intact and are clipped by the presentation layer.
	static std::string_view Descriptor(std::string_view Name, std::string_view VerifiedType = {})
	{
		if(!VerifiedType.empty() && VerifiedType != "None")
			return VerifiedType;
		constexpr std::string_view Prefix = "DDNet ";
		if(Name.substr(0, Prefix.size()) != Prefix)
			return Name;
		const auto Separator = Name.find(" - ", Prefix.size());
		if(Separator == std::string_view::npos)
			return Name;
		const auto Region = Name.substr(Prefix.size(), Separator - Prefix.size());
		if(Region.size() < 2 || Region.size() > 3 || !std::all_of(Region.begin(), Region.end(), [](char C) { return C >= 'A' && C <= 'Z'; }))
			return Name;
		const auto Mode = Name.substr(Separator + 3);
		for(const auto Known : {"Novice", "Moderate", "Brutal", "Insane", "Dummy", "Solo", "Oldschool", "DDmaX", "Race", "Fun"})
			if(Mode == Known)
				return Mode;
		return Name;
	}
	static void Sort(int Column, int &Sort, int &Order, bool ThreeOrders)
	{
		Order = Sort == Column ? (Order + 1) % (ThreeOrders ? 3 : 2) : 0;
		Sort = Column;
	}
};

// Independent of HTTP and graphics: only the current generation may be published.
class CLucentPreviewSelection
{
	std::string m_Key;
	double m_Changed = 0;
	uint64_t m_Generation = 0;
	bool m_Started = false;

public:
	bool Select(std::string_view Key, double Now)
	{
		if(Key == m_Key)
			return false;
		m_Key.assign(Key);
		m_Changed = Now;
		m_Started = false;
		++m_Generation;
		return true;
	}
	bool Ready(double Now) const { return !m_Started && !m_Key.empty() && Now - m_Changed >= .22; }
	void Start() { m_Started = true; }
	uint64_t Generation() const { return m_Generation; }
	bool Accept(uint64_t Generation) const { return Generation == m_Generation; }
	const std::string &Key() const { return m_Key; }
};
#endif
