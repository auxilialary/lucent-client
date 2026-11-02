#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_ROW_DATA_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_ROW_DATA_H
#include <engine/shared/protocol.h>
#include <engine/shared/protocol7.h>
namespace LucentScoreboard
{
	enum class EScoreMode
	{
		POINTS,
		RACE7,
		FINISH,
		TIMESCORE
	};
	struct CRowScore
	{
		bool m_Known;
		int m_Seconds, m_Millis;
		static CRowScore Read(EScoreMode Mode, int Score, int Seconds, int Millis)
		{
			if(Mode == EScoreMode::RACE7)
				return {Score >= 0 && Score != protocol7::FinishTime::NOT_FINISHED, Score / 1000, Score % 1000};
			if(Mode == EScoreMode::FINISH)
				return {Seconds >= 0, Seconds, Millis};
			if(Mode == EScoreMode::TIMESCORE)
				return {Score != FinishTime::NOT_FINISHED_TIMESCORE, Score, -1};
			return {true, Score, -1};
		}
	};
}
#endif
