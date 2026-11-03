#include "lucent_tee_align.h"

#include <base/vmath.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

static bool IsPlayerSolo(const CGameClient *pGameClient, int ClientId)
{
	if(!pGameClient || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;

	if(pGameClient->m_Teams.GetSolo(ClientId))
		return true;

	if(pGameClient->m_aClients[ClientId].m_Solo)
		return true;

	if(pGameClient->m_Snap.m_aCharacters[ClientId].m_HasExtendedData &&
		(pGameClient->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_SOLO))
	{
		return true;
	}

	return false;
}

STeeUnderneathResult CLucentTeeAlign::FindTeeUnderneath(
	const CGameClient *pGameClient,
	const STeeUnderneathParams &Params)
{
	STeeUnderneathResult Result;
	if(!pGameClient)
		return Result;

	int LocalClientId = pGameClient->m_Snap.m_LocalClientId;
	if(pGameClient->m_Snap.m_SpecInfo.m_Active)
		LocalClientId = pGameClient->m_Snap.m_SpecInfo.m_SpectatorId;

	if(LocalClientId < 0 || LocalClientId >= MAX_CLIENTS)
		return Result;

	if(!pGameClient->m_Snap.m_aCharacters[LocalClientId].m_Active)
		return Result;

	const vec2 LocalPos = pGameClient->m_PredictedChar.m_Pos;
	const CCollision *pCollision = pGameClient->Collision();

	int BestId = -1;
	vec2 BestTargetPos = vec2(0.0f, 0.0f);
	float BestDeltaY = 1e9f;
	bool FoundExactX = false;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalClientId)
			continue;

		if(!pGameClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		if(Params.m_CheckOtherTeam && pGameClient->IsOtherTeam(i))
			continue;

		if(Params.m_CheckSolo && (IsPlayerSolo(pGameClient, LocalClientId) || IsPlayerSolo(pGameClient, i)))
			continue;

		vec2 TargetPos;
		const bool IsDummy = pGameClient->PredictDummy() && i == pGameClient->m_aLocalIds[!g_Config.m_ClDummy];
		if(i == pGameClient->m_Snap.m_LocalClientId || IsDummy)
			TargetPos = pGameClient->m_aClients[i].m_Predicted.m_Pos;
		else
			TargetPos = pGameClient->m_aClients[i].m_RenderPos;

		float DeltaY = TargetPos.y - LocalPos.y;
		if(DeltaY <= 0.0f)
			continue;

		if(pCollision)
		{
			if(Params.m_CheckSolidObstacles)
			{
				bool BlockedBySolid = false;
				const float aWidthOffsets[] = {-13.5f, -9.0f, -4.5f, 0.0f, 4.5f, 9.0f, 13.5f};
				for(float OffsetX : aWidthOffsets)
				{
					if(pCollision->CheckPoint(LocalPos.x + OffsetX, LocalPos.y + 14.0f + 2.0f))
					{
						BlockedBySolid = true;
						break;
					}

					if(pCollision->IntersectLine(
						vec2(LocalPos.x + OffsetX, LocalPos.y),
						vec2(TargetPos.x + OffsetX, TargetPos.y),
						nullptr, nullptr))
					{
						BlockedBySolid = true;
						break;
					}
				}
				if(BlockedBySolid)
					continue;
			}

			if(Params.m_CheckKillTiles || Params.m_CheckTeleTiles)
			{
				bool Blocked = false;
				const int Steps = 3;
				const float aTileOffsets[] = {-13.5f, 0.0f, 13.5f};
				for(int Step = 1; Step <= Steps && !Blocked; Step++)
				{
					vec2 SamplePos = mix(LocalPos, TargetPos, (float)Step / (float)(Steps + 1));
					for(float OffsetX : aTileOffsets)
					{
						int MapIndex = pCollision->GetPureMapIndex(SamplePos.x + OffsetX, SamplePos.y);
						if(MapIndex < 0)
							continue;

						int TileIndex = pCollision->GetTileIndex(MapIndex);
						int FrontTileIndex = pCollision->GetFrontTileIndex(MapIndex);

						if(Params.m_CheckKillTiles && (TileIndex == TILE_DEATH || FrontTileIndex == TILE_DEATH))
						{
							Blocked = true;
							break;
						}
						if(Params.m_CheckTeleTiles && (pCollision->IsTeleport(MapIndex) || pCollision->IsTeleCheckpoint(MapIndex) || pCollision->IsEvilTeleport(MapIndex)))
						{
							Blocked = true;
							break;
						}
					}
				}
				if(Blocked)
					continue;
			}
		}

		const bool MatchesX = (LocalPos.x == TargetPos.x);

		if(MatchesX)
		{
			if(!FoundExactX || DeltaY < BestDeltaY)
			{
				FoundExactX = true;
				BestDeltaY = DeltaY;
				BestId = i;
				BestTargetPos = TargetPos;
			}
		}
		else if(!FoundExactX && DeltaY < BestDeltaY)
		{
			BestDeltaY = DeltaY;
			BestId = i;
			BestTargetPos = TargetPos;
		}
	}

	if(BestId != -1)
	{
		Result.m_Found = true;
		Result.m_ClientId = BestId;
		Result.m_LocalPos = LocalPos;
		Result.m_TargetPos = BestTargetPos;
		Result.m_RelativePos = LocalPos - BestTargetPos;
		Result.m_Distance = distance(LocalPos, BestTargetPos);
	}

	return Result;
}

bool CLucentTeeAlign::IsAligned(const STeeUnderneathResult &Result)
{
	return Result.m_Found && (Result.m_LocalPos.x == Result.m_TargetPos.x);
}
