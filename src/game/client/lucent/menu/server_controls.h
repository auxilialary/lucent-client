#ifndef GAME_CLIENT_LUCENT_SERVER_CONTROLS_H
#define GAME_CLIENT_LUCENT_SERVER_CONTROLS_H
#include <game/client/lucent/motion/transition.h>

#include <algorithm>

// Fixed-slot branch: intent changes immediately, the reversible reveal takes 280 ms.
class CLucentServerControls
{
	bool m_Requested = false;
	float m_Time = 0;

public:
	void Reset() { *this = CLucentServerControls(); }
	void Open() { m_Requested = true; }
	void Close() { m_Requested = false; }
	bool IsOpen() const { return m_Requested; }
	void Update(float Dt, bool Animate, bool ParentVisible, bool Focused)
	{
		if(!ParentVisible || !Focused)
		{
			Reset();
			return;
		}
		m_Time = Animate ? std::clamp(m_Time + (m_Requested ? Dt : -Dt), 0.0f, LucentStyle::Motion::Branch) : (m_Requested ? LucentStyle::Motion::Branch : 0.0f);
	}
	float Progress() const
	{
		const float T = m_Time / LucentStyle::Motion::Branch;
		return LucentStyle::Motion::Smooth(T);
	}
	bool Interactive() const { return m_Requested ? m_Time == LucentStyle::Motion::Branch : m_Time == 0; }
	enum class EAction
	{
		NONE,
		START,
		CONNECT,
		STOP,
		BACK
	};
	static EAction Action(int Slot, bool Running, bool Starting)
	{
		if(Slot == 2)
			return EAction::BACK;
		if(Running)
			return Slot == 0 ? EAction::CONNECT : EAction::STOP;
		return Slot == 0 && !Starting ? EAction::START : EAction::NONE;
	}
	static bool CanDispatch(EAction Action, bool Running, bool Starting)
	{
		return Action == EAction::BACK ||
		       (Action == EAction::START && CanStart(Running, Starting)) ||
		       ((Action == EAction::CONNECT || Action == EAction::STOP) && Running);
	}
	static bool CanStart(bool Running, bool Starting) { return !Running && !Starting; }
};
#endif
