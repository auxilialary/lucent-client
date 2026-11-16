#ifndef GAME_CLIENT_LUCENT_HUD_WATCHERS_H
#define GAME_CLIENT_LUCENT_HUD_WATCHERS_H

namespace LucentHud::Watchers
{
	// The other local tee may watch us. Only exclude the currently viewed tee.
	inline bool Eligible(int Id, int Self, bool Active, bool HasInfo)
	{
		return Id >= 0 && Id != Self && Active && HasInfo;
	}
	inline bool InferNames(bool Sixup, int Exact, int Count, int Candidates)
	{
		return !Sixup && Exact == 0 && Count > 0 && Candidates == Count;
	}
}
#endif
