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
	int m_Focus = 0; // list, search, filters, details, players, address, connect
	void Tab(bool Reverse) { m_Focus = (m_Focus + (Reverse ? 6 : 1)) % 7; }
	float m_Open = 0;
	float m_Content = 1;
	void Toggle(EPanel Panel)
	{
		m_Panel = m_Panel == Panel ? EPanel::NONE : Panel;
		m_Content = 0;
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
		const float Target = m_Panel == EPanel::NONE ? 0.f : 1.f;
		const float Response = Animate ? 1.f - std::exp(-std::clamp(Dt, 0.f, .1f) * 15.f) : 1.f;
		m_Open += (Target - m_Open) * Response;
		if(std::abs(Target - m_Open) < .001f)
			m_Open = Target;
		m_Content += (1.f - m_Content) * Response;
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
