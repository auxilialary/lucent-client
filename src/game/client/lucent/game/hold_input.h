#ifndef GAME_CLIENT_LUCENT_GAME_HOLD_INPUT_H
#define GAME_CLIENT_LUCENT_GAME_HOLD_INPUT_H

namespace LucentInput
{
	struct CRepeatedInput
	{
		int m_Fire, m_Jump;
	};

	// Per connection. Physical button state stays separate from the transmitted jump pulse.
	class CHoldInput
	{
		int m_LastTick = -1;
		bool m_JumpHeld = false;
		int m_Jump = 0;
		int m_LastJumpTick = -1;

	public:
		CRepeatedInput Update(int Tick, int Fire, int PreviousFire, bool JumpHeld, bool RepeatFire, bool RepeatJump, int CounterMask, int TickSpeed = 50)
		{
			const bool NewTick = Tick != m_LastTick;
			// A real press already supplies an edge; preserve odd/even press/release parity.
			if(RepeatFire && (Fire & 1) && Fire == PreviousFire && NewTick && m_LastTick >= 0)
				Fire = (Fire + 2) & CounterMask;
			if(!JumpHeld)
				m_Jump = 0;
			else if(!RepeatJump || !m_JumpHeld)
			{
				m_Jump = 1;
				m_LastJumpTick = Tick;
			}
			else if(NewTick)
			{
				// Round 50 ms up to whole ticks; leave a release tick between presses.
				const int Interval = (TickSpeed + 19) / 20;
				if(Tick < m_LastJumpTick)
					m_LastJumpTick = Tick;
				const bool Press = m_Jump == 0 && Tick - m_LastJumpTick >= Interval;
				m_Jump = Press ? 1 : 0;
				if(Press)
					m_LastJumpTick = Tick;
			}
			m_JumpHeld = JumpHeld;
			m_LastTick = Tick;
			return {Fire, m_Jump};
		}
	};
}
#endif
