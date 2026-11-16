/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_H
#define GAME_CLIENT_COMPONENTS_HUD_H
#include <engine/client.h>
#include <game/client/lucent/motion/cursor_motion_blur.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/lucent/hud/finish_summary.h>
#include <game/client/lucent/hud/layout.h>
#include <game/client/lucent/hud/tee_alignment.h>
#include <game/client/lucent/hud/weapon_scale.h>
#include <game/client/lucent/motion/transition.h>
#include <game/client/render.h>

struct SScoreInfo
{
	SScoreInfo()
	{
		Reset();
	}

	void Reset()
	{
		m_TextRankContainerIndex.Reset();
		m_TextScoreContainerIndex.Reset();
		m_RoundRectQuadContainerIndex = -1;
		m_OptionalNameTextContainerIndex.Reset();
		m_aScoreText[0] = 0;
		m_aRankText[0] = 0;
		m_aPlayerNameText[0] = 0;
		m_ScoreTextWidth = 0.f;
		m_Initialized = false;
	}

	STextContainerIndex m_TextRankContainerIndex;
	STextContainerIndex m_TextScoreContainerIndex;
	float m_ScoreTextWidth;
	char m_aScoreText[16];
	char m_aRankText[16];
	char m_aPlayerNameText[MAX_NAME_LENGTH];
	int m_RoundRectQuadContainerIndex;
	STextContainerIndex m_OptionalNameTextContainerIndex;

	bool m_Initialized;
};

class CHud : public CComponent
{
	float m_Width, m_Height;
	LucentStyle::Motion::CCursorMotionBlur m_CursorMotionBlur;

	int m_HudQuadContainerIndex;
	SScoreInfo m_aScoreInfo[2];
	STextContainerIndex m_FPSTextContainerIndex;
	STextContainerIndex m_DDRaceEffectsTextContainerIndex;
	STextContainerIndex m_PlayerAngleTextContainerIndex;
	float m_PlayerPrevAngle;
	STextContainerIndex m_aPlayerSpeedTextContainers[2];
	float m_aPlayerPrevSpeed[2];
	int m_aPlayerSpeed[2];
	enum class ESpeedChange
	{
		NONE,
		INCREASE,
		DECREASE
	};
	ESpeedChange m_aLastPlayerSpeedChange[2];
	STextContainerIndex m_aPlayerPositionContainers[2];
	float m_aPlayerPrevPosition[2];

	enum
	{
		LUCENT_TIMER,
		LUCENT_CLOCK,
		LUCENT_JUMPS,
		LUCENT_SECONDARY,
		LUCENT_RECORDING,
		LUCENT_RECORDING_AUTO,
		LUCENT_RECORDING_RACE,
		LUCENT_RECORDING_REPLAY,
		LUCENT_RECORDING_TIME,
		LUCENT_RECORDING_AUTO_TIME,
		LUCENT_COORDINATES,
		LUCENT_SPEED_X,
		LUCENT_SPEED_Y,
		LUCENT_AMMO,
		LUCENT_ALIGNMENT_0,
		LUCENT_ALIGNMENT_1,
		LUCENT_ALIGNMENT_2,
		LUCENT_ALIGNMENT_3,
		LUCENT_FINISH_TIME,
		LUCENT_FINISH_PREVIOUS_TIME,
		LUCENT_FINISH_TIME_DELTA,
		LUCENT_FINISH_POINTS,
		LUCENT_FINISH_AWARD,
		LUCENT_FINISH_GLOBAL_LABEL,
		LUCENT_FINISH_GLOBAL_RANK,
		LUCENT_FINISH_GLOBAL_PREVIOUS,
		LUCENT_FINISH_GLOBAL_DELTA,
		LUCENT_FINISH_REGION_LABEL,
		LUCENT_FINISH_REGION_RANK,
		LUCENT_FINISH_REGION_PREVIOUS,
		LUCENT_FINISH_REGION_DELTA,
		LUCENT_TEXT_COUNT
	};
	struct CLucentText
	{
		STextContainerIndex m_Index;
		char m_aText[256] = {};
		float m_Size = -1;
		int m_PixelHeight = -1;
	};
	CLucentText m_aLucentText[LUCENT_TEXT_COUNT];
	LucentHud::CRect m_LucentTimerBounds = {};
	LucentHud::CRect m_LucentStatusBounds = {};
	LucentHud::CRect m_LucentCoordinatesBounds = {};
	LucentHud::CRect m_LucentHotbarBounds = {};
	LucentHud::Alignment::CState m_LucentAlignment;
	LucentStyle::Motion::CTransition m_aLucentWeaponSelection[6];
	LucentHud::CWeaponScale m_LucentWeaponScale;
	class CFinishLookup;
	struct CFinishSummary
	{
		char m_aName[MAX_NAME_LENGTH] = {};
		char m_aRegion[5] = {};
		std::shared_ptr<CFinishLookup> m_pProfile, m_pMap;
		LucentHud::Finish::CProfile m_Profile;
		LucentHud::Finish::CRank m_Rank, m_PreviousRank, m_CurrentRank;
		LucentStyle::Motion::CTransition m_Reveal;
		double m_FinishedAt = -100, m_VisibleUntil = 0, m_RankReceivedAt = 0;
		double m_ProfileDue = 0, m_MapDue = 0;
		double m_NextRankRequest = 0, m_LastRankRequest = -100;
		int m_RankRequests = 0;
		int m_TimeMs = 0, m_DiffMs = 0;
		int m_MapPoints = -1, m_PointsBeforeAward = -1, m_ConfirmedAward = 0;
		bool m_FirstFinish = false, m_RankHeader = false, m_RankReceived = false;
	};
	CFinishSummary m_aFinishSummaries[NUM_DUMMIES];
	CFinishSummary *LucentFinishPlayer();
	void OnLucentFinishMessage(int MsgType, const void *pRawMsg);
	struct CAlignmentVisual
	{
		int m_Id = -1;
		char m_aName[128] = {};
		CTeeRenderInfo m_Tee;
	};
	CAlignmentVisual m_aAlignmentVisual[4];
	float PrepareAlignmentText(int Slot, const char *pText, float Size);
	void DrawAlignmentText(int Slot, float X, float CenterY, float Alpha);
	void RenderLucentAlignment();
	float LucentText(int Slot, const char *pText, float X, float Y, float Size, ColorRGBA Color, bool Center = false);
	void RenderLucentTimer(int Time);
	bool RenderLucentFinishCard(const LucentHud::CLayout &Layout, LucentHud::CRect &Panel, float Reveal, float Alpha, double Now);
	void RenderLucentHotbar(int ClientId, const CNetObj_Character *pCharacter);
	void RenderLucentRecording();
	void RenderLucentCoordinates();
	void RenderLucentStatus(int ClientId, const CNetObj_Character *pCharacter, bool Extended);

	void RenderCursor();

	void RenderTextInfo();
	void RenderConnectionWarning();
	void RenderTeambalanceWarning();

	void PrepareAmmoHealthAndArmorQuads();
	void RenderAmmoHealthAndArmor(const CNetObj_Character *pCharacter);

	void PreparePlayerStateQuads();
	void RenderPlayerState(int ClientId);

	int m_LastSpectatorCountTick;
	void RenderSpectatorCount();
	void RenderDummyActions();
	void RenderMovementInformation();

	void UpdateMovementInformationTextContainer(STextContainerIndex &TextContainer, float FontSize, float Value, float &PrevValue);
	void RenderMovementInformationTextContainer(STextContainerIndex &TextContainer, const ColorRGBA &Color, float X, float Y);

	class CMovementInformation
	{
	public:
		vec2 m_Pos;
		vec2 m_Speed;
		float m_Angle = 0.0f;
	};
	class CMovementInformation GetMovementInformation(int ClientId, int Conn) const;

	void RenderGameTimer();
	void RenderPauseNotification();
	void RenderSuddenDeath();

	void RenderScoreHud();
	int m_LastLocalClientId = -1;

	void RenderSpectatorHud();
	void RenderWarmupTimer();
	void RenderLocalTime(float x);

	static constexpr float MOVEMENT_INFORMATION_LINE_HEIGHT = 8.0f;

public:
	CHud();
	int Sizeof() const override { return sizeof(*this); }

	void ResetHudContainers();
	void OnWindowResize() override;
	void OnReset() override;
	void OnRender() override;
	void OnUpdate() override;
	void OnInit() override;
	void OnShutdown() override;
	void OnNewSnapshot() override;

	// DDRace

	void OnMessage(int MsgType, void *pRawMsg) override;
	void RenderNinjaBarPos(float x, float y, float Width, float Height, float Progress, float Alpha = 1.0f);

private:
	void RenderRecord();
	void RenderDDRaceEffects();
	float m_TimeCpDiff;
	float m_aPlayerRecord[NUM_DUMMIES];
	float m_FinishTimeDiff;
	int m_DDRaceTime;
	int m_FinishTimeLastReceivedTick;
	int m_TimeCpLastReceivedTick;
	bool m_ShowFinishTime;

	inline float GetMovementInformationBoxHeight();
	inline int GetDigitsIndex(int Value, int Max);

	// Quad Offsets
	int m_aAmmoOffset[NUM_WEAPONS];
	int m_HealthOffset;
	int m_EmptyHealthOffset;
	int m_ArmorOffset;
	int m_EmptyArmorOffset;
	int m_aCursorOffset[NUM_WEAPONS];
	int m_FlagOffset;
	int m_AirjumpOffset;
	int m_AirjumpEmptyOffset;
	int m_aWeaponOffset[NUM_WEAPONS];
	int m_EndlessJumpOffset;
	int m_EndlessHookOffset;
	int m_JetpackOffset;
	int m_TeleportGrenadeOffset;
	int m_TeleportGunOffset;
	int m_TeleportLaserOffset;
	int m_SoloOffset;
	int m_CollisionDisabledOffset;
	int m_HookHitDisabledOffset;
	int m_HammerHitDisabledOffset;
	int m_GunHitDisabledOffset;
	int m_ShotgunHitDisabledOffset;
	int m_GrenadeHitDisabledOffset;
	int m_LaserHitDisabledOffset;
	int m_DeepFrozenOffset;
	int m_LiveFrozenOffset;
	int m_DummyHammerOffset;
	int m_DummyCopyOffset;
	int m_PracticeModeOffset;
	int m_Team0ModeOffset;
	int m_LockModeOffset;
};

#endif
