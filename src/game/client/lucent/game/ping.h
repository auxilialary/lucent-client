#ifndef GAME_CLIENT_LUCENT_GAME_PING_H
#define GAME_CLIENT_LUCENT_GAME_PING_H
#include <base/vmath.h>

#include <algorithm>
#include <cstdint>
#include <vector>
namespace LucentPing
{
	struct CEvent
	{
		uint64_t m_Id;
		vec2 m_Position;
		double m_Created;
		bool m_Remote = false;
		int m_SenderClientId = -1;
		float Alpha(double Now) const { return std::clamp(float((4.0 - (Now - m_Created)) / .6), 0.f, 1.f); }
	};
	class CState
	{
		uint64_t m_NextId = 1;
		std::vector<CEvent> m_Events;

	public:
		void Clear() { m_Events.clear(); }
		void ClearRemote()
		{
			m_Events.erase(std::remove_if(m_Events.begin(), m_Events.end(), [](const CEvent &Event) { return Event.m_Remote; }), m_Events.end());
		}
		void Update(double Now)
		{
			m_Events.erase(std::remove_if(m_Events.begin(), m_Events.end(), [Now](const CEvent &Event) { return Now - Event.m_Created >= 4.0; }), m_Events.end());
		}
		CEvent Add(vec2 Position, double Now)
		{
			CEvent Event{m_NextId++, Position, Now};
			Insert(Event, Now);
			return Event;
		}
		void AddRemote(uint64_t Id, vec2 Position, double Received, int SenderClientId = -1)
		{
			Insert({Id, Position, Received, true, SenderClientId}, Received);
		}
		const std::vector<CEvent> &Events() const { return m_Events; }

	private:
		void Insert(const CEvent &Event, double Now)
		{
			Update(Now);
			if(m_Events.size() == 8)
				m_Events.erase(m_Events.begin());
			m_Events.push_back(Event);
		}
	};
}
#endif
