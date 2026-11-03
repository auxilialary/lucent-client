#ifndef GAME_CLIENT_LUCENT_LUCENT_TEE_ALIGN_H
#define GAME_CLIENT_LUCENT_LUCENT_TEE_ALIGN_H

#include <base/vmath.h>

struct STeeUnderneathParams
{
	bool m_CheckOtherTeam = true;
	bool m_CheckSolidObstacles = false;
	bool m_CheckKillTiles = false;
	bool m_CheckTeleTiles = false;
	bool m_CheckSolo = false;
};

struct STeeUnderneathResult
{
	bool m_Found = false;
	int m_ClientId = -1;
	vec2 m_LocalPos = vec2(0.0f, 0.0f);
	vec2 m_TargetPos = vec2(0.0f, 0.0f);
	vec2 m_RelativePos = vec2(0.0f, 0.0f);
	float m_Distance = 0.0f;
};

class CLucentTeeAlign
{
public:
	static STeeUnderneathResult FindTeeUnderneath(
		const class CGameClient *pGameClient,
		const STeeUnderneathParams &Params = STeeUnderneathParams());

	static bool IsAligned(const STeeUnderneathResult &Result);
};

#endif
