#include <game/client/lucent/game/hold_input.h>

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#define CHECK(Expression) \
	do \
	{ \
		if(!(Expression)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #Expression); \
			std::abort(); \
		} \
	} while(false)

int main()
{
	using LucentInput::CHoldInput;
	constexpr int Mask = 63;
	CHoldInput Main;
	auto Out = Main.Update(100, 1, 0, true, true, true, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 1);
	// Subtick aiming and extra frames must not produce more pulses in the same tick.
	Out = Main.Update(100, 1, 1, true, true, true, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 1);
	Out = Main.Update(101, 1, 1, true, true, true, Mask);
	CHECK(Out.m_Fire == 3 && Out.m_Jump == 0);
	Out = Main.Update(102, 3, 3, true, true, true, Mask);
	CHECK(Out.m_Fire == 5 && Out.m_Jump == 0);
	// A physical release must remain released, without inventing a press.
	Out = Main.Update(103, 6, 5, false, true, true, Mask);
	CHECK(Out.m_Fire == 6 && Out.m_Jump == 0);
	Out = Main.Update(104, 6, 6, false, true, true, Mask);
	CHECK(Out.m_Fire == 6 && Out.m_Jump == 0);
	// A quick manual click supplies its own counter edge.
	Out = Main.Update(105, 9, 6, true, true, true, Mask);
	CHECK(Out.m_Fire == 9 && Out.m_Jump == 1);
	// Counter rollover preserves held state and one release/press pair.
	Out = Main.Update(106, 63, 63, true, true, true, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 0);
	// Disabling features restores normal held input; other weapons use this path too.
	Out = Main.Update(107, 1, 1, true, false, false, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 1);
	Out = Main.Update(108, 1, 1, true, false, false, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 1);
	CHoldInput Dummy;
	Out = Dummy.Update(108, 1, 0, true, true, true, Mask);
	CHECK(Out.m_Fire == 1 && Out.m_Jump == 1);
	Main = {};
	Out = Main.Update(200, 0, 0, false, true, true, Mask);
	CHECK(Out.m_Fire == 0 && Out.m_Jump == 0);
	// Held jump presses are at least 50 ms apart at different server tick rates.
	for(int Speed : {20, 50, 100, 128})
	{
		CHoldInput Jump;
		int LastPress = -1;
		int PreviousJump = 0;
		int Presses = 0;
		for(int Tick = 0; Tick < Speed * 2; ++Tick)
		{
			Out = Jump.Update(Tick, 0, 0, true, false, true, Mask, Speed);
			if(Out.m_Jump && !PreviousJump)
			{
				if(LastPress >= 0)
					CHECK((Tick - LastPress) * 1000 >= Speed * 50);
				LastPress = Tick;
				++Presses;
			}
			PreviousJump = Out.m_Jump;
		}
		CHECK(Presses >= 10);
	}
	// Simulate a server with a three-tick weapon cooldown. Repeated input does not bypass it.
	int Fire = 1, Previous = 0, Cooldown = 0, Shots = 0;
	for(int Tick = 0; Tick < 30; ++Tick)
	{
		Out = Main.Update(300 + Tick, Fire, Previous, false, true, true, Mask);
		if(Cooldown > 0)
			--Cooldown;
		if(Out.m_Fire != Previous && Cooldown == 0)
		{
			++Shots;
			Cooldown = 3;
		}
		Previous = Fire = Out.m_Fire;
	}
	CHECK(Shots == 10);
	std::puts("Hold input: tick pacing, manual edges, release, rollover, jump pulses, reset, independent connections and server cooldown passed");
}
