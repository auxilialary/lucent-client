/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/lucent/scoreboard/layout.h>
#include <game/client/lucent/scoreboard/panel.h>
#include <game/client/lucent/scoreboard/points.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

class CScoreboard : public CComponent
{
	void RenderTitleScore(CUIRect ScoreLabel, int Team, float TitleFontSize);
	void RenderTitle(CUIRect TitleLabel, int Team, const char *pTitle, float TitleFontSize, bool MapTitle = false);
	void RenderHeader(float CenterX, float Top, float ViewportWidth);
	void RenderTitleBar(CUIRect TitleBar, int Team, const char *pTitle);
	void RenderGoals(CUIRect Goals);
	void RenderSpectators(CUIRect Spectators);
	void RenderMouseHint(CUIRect MouseHint);
	void RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, float TeamWidth);
	void RenderRecordingNotification(float x);

	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleScoreboardCursor(IConsole::IResult *pResult, void *pUserData);

	const char *GetTeamName(int Team) const;

	bool m_Active;
	LucentStyle::Motion::CTransition m_Visibility, m_SpectatorVisibility;
	LucentScoreboard::CPlanner m_Planner;
	LucentScoreboard::CPointsService m_PointsService;
	double m_NextPointsRead = 0;
	float m_FontSize = 12, m_ScoreWidth = 80, m_PointsWidth = 44, m_PingWidth = 26, m_DDTeamWidth = 0;
	struct CDDTeamLabel
	{
		char m_aText[64] = {}, m_aTooltip[96] = {};
		float m_Width = 0;
	};
	std::array<CDDTeamLabel, MAX_CLIENTS + 1> m_aDDTeamLabels;

	LucentScoreboard::CPanelCache m_aPanels[2 + LucentScoreboard::MaxColumns];
	LucentScoreboard::CPanelMotion m_aColumnMotion[LucentScoreboard::MaxColumns];
	void DrawFloatingPanel(const CUIRect &Rect, int Index, float Opacity);
	LucentScoreboard::CExtent m_HeaderWidth, m_Top, m_SpectatorExtent;
	float m_RowHeight = 22.f;
	float m_NameWidth = 0, m_ClanWidth = 0;
	float m_SpectatorRow = 14.f;
	float m_Opacity = 1.f;
	int m_SpectatorColumns = 1;
	double m_RevealStart = 0;
	bool m_WasVisible = false;
	double m_Now = 0;

	IGraphics::CTextureHandle m_DeadTeeTexture;

	std::optional<vec2> m_LastMousePos;
	bool m_MouseUnlocked = false;

	void SetUiMousePos(vec2 Pos);
	void LockMouse();

	class CScoreboardPopupContext : public SPopupMenuId
	{
	public:
		CScoreboard *m_pScoreboard = nullptr;
		CButtonContainer m_FriendAction;
		CButtonContainer m_MuteAction;
		CButtonContainer m_EmoticonAction;

		CButtonContainer m_SpectateButton;

		int m_ClientId;
		bool m_IsLocal;
		bool m_IsSpectating;

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	} m_ScoreboardPopupContext;

	class CMapTitlePopupContext : public SPopupMenuId
	{
	public:
		CScoreboard *m_pScoreboard = nullptr;

		float m_FontSize;

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	} m_MapTitlePopupContext;
	char m_MapTitleButtonId;

	class CPlayerElement
	{
	public:
		char m_aMeasuredName[128] = {}, m_aMeasuredClan[MAX_CLAN_LENGTH] = {};
		float m_NameWidth = 0, m_ClanWidth = 0;
		int m_PointsValue = -1;
		char m_aPointsName[64] = {};
		char m_PlayerButtonId;
		char m_TimeTooltipId;
		char m_aTimeTooltip[32];
		char m_aSpectatorTooltip[128] = {};

		CCachedText m_Score;
		CCachedText m_ScoreMillis;
		CCachedText m_Name;
		CCachedText m_ReadyMark;
		CCachedText m_Clan;
		CCachedText m_Ping, m_Points, m_DDTeam;
	};
	CPlayerElement m_aPlayers[MAX_CLIENTS];

	CCachedText m_HeaderTimer, m_HeaderMapMeasure;
	CCachedText m_TitleScore;
	CCachedText m_TitleScoreMillis;
	CCachedText m_HeadlineScore;
	CCachedText m_HeadlineName;
	CCachedText m_HeadlineClan;
	CCachedText m_HeadlinePing, m_HeadlinePoints;

	void ResetTexts();

public:
	CScoreboard();
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnWindowResize() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const;
};

#endif
