#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_RENDERER_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_RENDERER_H
#include "layout.h"
#include "panel.h"

#include <generated/protocol.h>

#include <game/client/ui.h>
class CScoreboard;
namespace LucentScoreboard
{
	// Owns presentation state; the DDNet component supplies protocol and action integration.
	class CRenderer
	{
		CScoreboard &m_Component;
		void RenderTitleScore(CUIRect ScoreLabel, int Team, float TitleFontSize);
		void RenderTitle(CUIRect TitleLabel, int Team, const char *pTitle, float TitleFontSize, bool MapTitle = false);
		void RenderHeader(float CenterX, float Top, float ViewportWidth);
		void RenderTitleBar(CUIRect TitleBar, int Team, const char *pTitle);
		void RenderGoals(CUIRect Goals);
		void RenderSpectators(CUIRect Spectators);
		void RenderMouseHint(CUIRect MouseHint);
		void RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, float ScoreWidth);
		void RenderRecordingNotification(float x);

		void DrawFloatingPanel(const CUIRect &Rect, int Index, float Opacity);
		LucentStyle::Motion::CTransition m_Visibility, m_SpectatorVisibility;
		LucentScoreboard::CPlanner m_Planner;
		float m_FontSize = 12, m_PointsWidth = 44, m_PingWidth = 26;
		struct CDDTeamLabel
		{
			char m_aText[64] = {}, m_aCount[32] = {};
			float m_Width = 0;
			CCachedText m_Title, m_Count;
		};
		std::array<CDDTeamLabel, MAX_CLIENTS + 1> m_aDDTeamLabels;

		LucentScoreboard::CPanelCache m_aPanels[2 + LucentScoreboard::MaxColumns];
		LucentScoreboard::CPanelMotion m_aColumnMotion[LucentScoreboard::MaxColumns];
		LucentScoreboard::CExtent m_HeaderWidth, m_Top, m_SpectatorExtent, m_SpectatorWidth;
		float m_RowHeight = 22.f;
		float m_SpectatorRow = 14.f;
		float m_Opacity = 1.f;
		int m_SpectatorColumns = 1;
		double m_RevealStart = 0;
		bool m_WasVisible = false;
		double m_Now = 0;

		IGraphics::CTextureHandle m_DeadTeeTexture;
		class CPlayerElement
		{
		public:
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
			LucentScoreboard::CPanelCache m_ClanPill;
			CCachedText m_Ping, m_Points;
		};
		CPlayerElement m_aPlayers[MAX_CLIENTS];
		std::array<CPanelCache, MAX_CLIENTS> m_aGroupPanels;

		CCachedText m_HeaderTimer, m_HeaderMapMeasure;
		CCachedText m_TitleScore;
		CCachedText m_TitleScoreMillis;
		CCachedText m_HeadlineScore;
		CCachedText m_HeadlineName;
		CCachedText m_HeadlinePing, m_HeadlinePoints;

	public:
		explicit CRenderer(CScoreboard &Component) : m_Component(Component) {}
		void Init();
		void Reset();
		void ResetTexts();
		void ResetPoints();
		void Render(bool Visible);
	};
}
#endif
