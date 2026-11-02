/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "renderer.h"

#include "row_data.h"

#include <base/dbg.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/demo.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data7.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/statboard.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/hud/visuals.h>
#include <game/client/lucent/scoreboard/identity_text.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui.h>
#include <game/localization.h>

// Horizontal spacing of the scoreboard contents, both to its edges and between columns
static constexpr float MARGIN = 10.0f;
static constexpr float ROUNDING = 7.5f;
static const ColorRGBA BACKGROUND_COLOR = LucentStyle::Style::Surface.WithAlpha(.88f);

static constexpr float POPUP_WIDTH = 110.0f;

static constexpr ColorRGBA TEAM_RED_TITLE_COLOR(0.975f, 0.17f, 0.17f);
static constexpr ColorRGBA TEAM_BLUE_TITLE_COLOR(0.17f, 0.46f, 0.975f);

static constexpr const char *TIME_WIDTH_TEXT = "00:00:00";

namespace LucentScoreboard
{
	// Same coverage bands as Lucent's panel border, with the actual DDTeam tint.
	static void DrawTeamBorder(IGraphics *pGraphics, const LucentHud::CPanelGeometry &G, ColorRGBA Color)
	{
		if(!G.m_Count)
			return;
		Color.a *= G.m_BorderPeak;
		const auto Offsets = G.BorderOffsets();
		const std::array Contours = {G.Boundary(Offsets[0]), G.Boundary(Offsets[1]), G.Boundary(Offsets[2]), G.Boundary(Offsets[3])};
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(int Band = 0; Band < 3; ++Band)
		{
			if(Offsets[Band] == Offsets[Band + 1])
				continue;
			const auto Inner = Color.WithAlpha(Band == 0 ? 0.f : Color.a), Outer = Color.WithAlpha(Band == 2 ? 0.f : Color.a);
			for(int I = 0; I < G.m_Count; ++I)
			{
				const int J = (I + 1) % G.m_Count;
				pGraphics->SetColor4(Inner, Outer, Outer, Inner);
				const IGraphics::CFreeformItem Quad(Contours[Band][I], Contours[Band + 1][I], Contours[Band][J], Contours[Band + 1][J]);
				pGraphics->QuadsDrawFreeform(&Quad, 1);
			}
		}
		pGraphics->QuadsEnd();
	}

	void CRenderer::Init()
	{
		m_DeadTeeTexture = m_Component.Graphics()->LoadTexture("deadtee.png", IStorage::TYPE_ALL);
	}

	void CRenderer::Reset()
	{
		m_Visibility = {};
		m_SpectatorVisibility = {};
		m_Planner = {};
		m_HeaderWidth = {};
		for(auto &Column : m_aColumnMotion)
			Column = {};
		m_Top = {};
		m_SpectatorExtent = {};
		m_SpectatorWidth = {};
		for(auto &Player : m_aPlayers)
		{
			Player.m_PointsValue = -1;
			Player.m_aPointsName[0] = 0;
		}
		m_WasVisible = false;
	}

	void CRenderer::ResetPoints()
	{
		for(auto &Player : m_aPlayers)
		{
			Player.m_PointsValue = -1;
			Player.m_aPointsName[0] = 0;
		}
	}

	void CRenderer::ResetTexts()
	{
		for(auto &Label : m_aDDTeamLabels)
		{
			Label.m_Title.Reset(m_Component.TextRender());
			Label.m_Count.Reset(m_Component.TextRender());
		}
		for(CPlayerElement &Player : m_aPlayers)
		{
			Player.m_Score.Reset(m_Component.TextRender());
			Player.m_ScoreMillis.Reset(m_Component.TextRender());
			Player.m_Name.Reset(m_Component.TextRender());
			Player.m_ReadyMark.Reset(m_Component.TextRender());
			Player.m_Clan.Reset(m_Component.TextRender());
			Player.m_Ping.Reset(m_Component.TextRender());
			Player.m_Points.Reset(m_Component.TextRender());
		}
		m_HeaderTimer.Reset(m_Component.TextRender());
		m_HeaderMapMeasure.Reset(m_Component.TextRender());
		m_TitleScore.Reset(m_Component.TextRender());
		m_TitleScoreMillis.Reset(m_Component.TextRender());
		m_HeadlineScore.Reset(m_Component.TextRender());
		m_HeadlineName.Reset(m_Component.TextRender());
		m_HeadlinePing.Reset(m_Component.TextRender());
		m_HeadlinePoints.Reset(m_Component.TextRender());
	}

	void CRenderer::RenderTitle(CUIRect TitleLabel, int Team, const char *pTitle, float TitleFontSize, bool MapTitle)
	{
		const bool IsMapTitle = MapTitle;
		if(MapTitle && m_Component.m_MouseUnlocked)
			m_Component.Ui()->DoToolTip(&m_Component.m_MapTitleButtonId, &TitleLabel, pTitle);
		if(IsMapTitle && m_Component.m_MouseUnlocked && m_Component.GameClient()->m_aMapDescription[0] != '\0')
		{
			const int ButtonResult = m_Component.Ui()->DoButtonLogic(&m_Component.m_MapTitleButtonId, 0, &TitleLabel, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
			if(ButtonResult != 0)
			{
				m_Component.m_MapTitlePopupContext.m_pScoreboard = &m_Component;

				m_Component.m_MapTitlePopupContext.m_FontSize = 12.0f;
				const float MaxWidth = 300.0f;
				const float Margin = 5.0f;
				const char *pDescription = m_Component.GameClient()->m_aMapDescription;
				const float TextWidth = std::min(std::ceil(m_Component.TextRender()->TextWidth(m_Component.m_MapTitlePopupContext.m_FontSize, pDescription) + 0.5f), MaxWidth);
				float TextHeight = 0.0f;
				STextSizeProperties TextSizeProps{};
				TextSizeProps.m_pHeight = &TextHeight;
				m_Component.TextRender()->TextWidth(m_Component.m_MapTitlePopupContext.m_FontSize, pDescription, -1, TextWidth, 0, TextSizeProps);

				m_Component.Ui()->DoPopupMenu(&m_Component.m_MapTitlePopupContext, m_Component.Ui()->MouseX(), m_Component.Ui()->MouseY(), TextWidth + Margin * 2, TextHeight + Margin * 2, &m_Component.m_MapTitlePopupContext, CScoreboard::CMapTitlePopupContext::Render);
			}
			if(m_Component.Ui()->HotItem() == &m_Component.m_MapTitleButtonId)
			{
				TitleLabel.Draw(ColorRGBA(0.7f, 0.7f, 0.7f, 0.3f), IGraphics::CORNER_ALL, 5.0f);
			}
		}

		SLabelProperties Props;
		Props.m_MaxWidth = TitleLabel.w;
		Props.m_EllipsisAtEnd = true;
		m_Component.Ui()->DoLabel(&TitleLabel, pTitle, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_ML : TEXTALIGN_MR, Props);
	}

	void CRenderer::RenderTitleScore(CUIRect ScoreLabel, int Team, float TitleFontSize)
	{
		// map best
		char aScore[128] = "";
		const CNetObj_GameInfo *pGameInfoObj = m_Component.GameClient()->m_Snap.m_pGameInfoObj;
		const bool TimeScore = m_Component.GameClient()->m_GameInfo.m_TimeScore;
		const bool Race7 = m_Component.Client()->IsSixup() && pGameInfoObj && pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE;
		if(m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimes || TimeScore || Race7)
		{
			if(m_Component.GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET)
			{
				m_Component.Ui()->RenderTime(ScoreLabel,
					TitleFontSize,
					m_Component.GameClient()->m_MapBestTimeSeconds,
					m_Component.GameClient()->m_MapBestTimeSeconds == FinishTime::NOT_FINISHED_MILLIS,
					m_Component.GameClient()->m_MapBestTimeMillis,
					m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis,
					m_TitleScore, m_TitleScoreMillis, m_Component.TextRender()->DefaultTextColor());
				return;
			}
		}
		else if(m_Component.GameClient()->IsTeamPlay()) // normal score
		{
			const CNetObj_GameData *pGameDataObj = m_Component.GameClient()->m_Snap.m_pGameDataObj;
			if(pGameDataObj)
			{
				str_format(aScore, sizeof(aScore), "%d", Team == TEAM_RED ? pGameDataObj->m_TeamscoreRed : pGameDataObj->m_TeamscoreBlue);
			}
		}
		else
		{
			if(m_Component.GameClient()->m_Snap.m_SpecInfo.m_Active &&
				m_Component.GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW &&
				m_Component.GameClient()->m_Snap.m_apPlayerInfos[m_Component.GameClient()->m_Snap.m_SpecInfo.m_SpectatorId])
			{
				str_format(aScore, sizeof(aScore), "%d", m_Component.GameClient()->m_Snap.m_apPlayerInfos[m_Component.GameClient()->m_Snap.m_SpecInfo.m_SpectatorId]->m_Score);
			}
			else if(m_Component.GameClient()->m_Snap.m_pLocalInfo)
			{
				str_format(aScore, sizeof(aScore), "%d", m_Component.GameClient()->m_Snap.m_pLocalInfo->m_Score);
			}
		}

		const float ScoreTextWidth = aScore[0] != '\0' ? m_Component.TextRender()->TextWidth(TitleFontSize, aScore, -1, -1.0f, 0) : 0.0f;
		if(ScoreTextWidth != 0.0f)
		{
			m_Component.Ui()->DoLabel(&ScoreLabel, aScore, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_MR : TEXTALIGN_ML);
		}
	}

	void CRenderer::RenderHeader(float CenterX, float Top, float ViewportWidth)
	{
		const auto *pInfo = m_Component.GameClient()->m_Snap.m_pGameInfoObj;
		char aTimer[64];
		const int Best = m_Component.GameClient()->m_MapBestTimeSeconds;
		const bool Race = m_Component.GameClient()->m_GameInfo.m_TimeScore || m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimes ||
				  (m_Component.Client()->IsSixup() && pInfo && (pInfo->m_GameFlags & protocol7::GAMEFLAG_RACE));
		if(Race && Best >= 0)
		{
			str_time(int64_t(Best) * 100, ETimeFormat::HOURS, aTimer, sizeof(aTimer));
			if(Best < 3600 && m_Component.GameClient()->m_MapBestTimeMillis >= 0)
			{
				char aFraction[8];
				if(m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis)
					str_format(aFraction, sizeof(aFraction), ".%03d", m_Component.GameClient()->m_MapBestTimeMillis % 1000);
				else
					str_format(aFraction, sizeof(aFraction), ".%02d", (int)std::round((m_Component.GameClient()->m_MapBestTimeMillis % 1000) / 10.f));
				str_append(aTimer, aFraction);
			}
		}
		else
		{
			const int Seconds = pInfo ? LucentScoreboard::RoundTime(m_Component.Client()->GameTick(g_Config.m_ClDummy), m_Component.Client()->GameTickSpeed(),
							    pInfo->m_RoundStartTick, pInfo->m_TimeLimit, pInfo->m_WarmupTimer,
							    pInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME, pInfo->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) :
						    0;
			str_time(int64_t(std::max(0, Seconds)) * 100, ETimeFormat::HOURS, aTimer, sizeof(aTimer));
		}
		constexpr float MapFont = 16, TimerFont = 13;
		m_HeaderTimer.Update(m_Component.TextRender(), aTimer, TimerFont);
		const char *pMap = m_Component.GameClient()->Map()->BaseName();
		m_HeaderMapMeasure.Update(m_Component.TextRender(), pMap, MapFont);
		const auto Target = LucentScoreboard::HeaderBounds(CenterX, Top, ViewportWidth, m_HeaderMapMeasure.Width(), m_HeaderTimer.Width());
		const float Minimum = std::min(Target.w, 2 * LucentScoreboard::HeaderPadding + LucentScoreboard::HeaderContentGap + m_HeaderTimer.Width() + 16.f);
		const float Width = std::max(Minimum, m_HeaderWidth.Update(Target.w, m_Now));
		CUIRect Header = {CenterX - Width / 2, Top, Width, Target.h};
		DrawFloatingPanel(Header, 0, m_Opacity);
		Header.VMargin(LucentScoreboard::HeaderPadding, &Header);
		CUIRect Timer, Title;
		Header.VSplitRight(m_HeaderTimer.Width(), &Title, &Timer);
		Title.VSplitRight(LucentScoreboard::HeaderContentGap, &Title, nullptr);
		RenderTitle(Title, TEAM_RED, pMap, MapFont, true);
		const vec2 Pos(Timer.x, Timer.y + (Timer.h - TimerFont) / 2);
		m_HeaderTimer.Render(m_Component.TextRender(), Pos, LucentStyle::Style::Accent);
	}

	void CRenderer::RenderTitleBar(CUIRect TitleBar, int Team, const char *pTitle)
	{
		dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

		const float TitleFontSize = 14.0f;
		const float ScoreTextWidth = m_Component.TextRender()->TextWidth(TitleFontSize, TIME_WIDTH_TEXT);
		const float TitleTextWidth = m_Component.TextRender()->TextWidth(TitleFontSize, pTitle);

		TitleBar.VMargin(MARGIN, &TitleBar);
		CUIRect TitleLabel, ScoreLabel;
		if(Team == TEAM_RED)
		{
			TitleBar.VSplitRight(ScoreTextWidth, &TitleLabel, &ScoreLabel);
			TitleLabel.VSplitRight(5.0f, &TitleLabel, nullptr);
			TitleLabel.VSplitLeft(std::min(TitleTextWidth + 2.0f, TitleLabel.w), &TitleLabel, nullptr);
		}
		else
		{
			TitleBar.VSplitLeft(ScoreTextWidth, &ScoreLabel, &TitleLabel);
			TitleLabel.VSplitLeft(5.0f, nullptr, &TitleLabel);
			TitleLabel.VSplitRight(std::min(TitleTextWidth + 2.0f, TitleLabel.w), nullptr, &TitleLabel);
		}

		RenderTitle(TitleLabel, Team, pTitle, TitleFontSize);
		RenderTitleScore(ScoreLabel, Team, TitleFontSize);
	}

	void CRenderer::RenderGoals(CUIRect Goals)
	{
		Goals.Draw(BACKGROUND_COLOR, IGraphics::CORNER_ALL, ROUNDING);
		Goals.VMargin(5.0f, &Goals);

		const float FontSize = 10.0f;
		const CNetObj_GameInfo *pGameInfoObj = m_Component.GameClient()->m_Snap.m_pGameInfoObj;
		char aBuf[64];

		if(pGameInfoObj->m_ScoreLimit)
		{
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), pGameInfoObj->m_ScoreLimit);
			m_Component.Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_ML);
		}

		if(pGameInfoObj->m_TimeLimit)
		{
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), pGameInfoObj->m_TimeLimit);
			m_Component.Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MC);
		}

		if(pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)
		{
			str_format(aBuf, sizeof(aBuf), Localize("Round %d/%d"), pGameInfoObj->m_RoundCurrent, pGameInfoObj->m_RoundNum);
			m_Component.Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MR);
		}
	}

	void CRenderer::DrawFloatingPanel(const CUIRect &Rect, int Index, float Opacity)
	{
		const auto &Geometry = m_aPanels[Index].Get({Rect.x, Rect.y, Rect.w, Rect.h}, LucentHud::PixelGrid(m_Component.Graphics()));
		LucentHud::DrawSoftFill(m_Component.Graphics(), Geometry, LucentStyle::Scoreboard::Surface.WithMultipliedAlpha(Opacity), LucentStyle::Scoreboard::Surface.WithMultipliedAlpha(Opacity));
		LucentHud::DrawSoftBorder(m_Component.Graphics(), Geometry, Opacity, false);
	}

	void CRenderer::RenderSpectators(CUIRect Spectators)
	{
		DrawFloatingPanel(Spectators, 1, m_Opacity * m_SpectatorVisibility.m_Value);
		CUIRect Title = {Spectators.x + 10, Spectators.y + 5, Spectators.w - 20, 16};
		m_Component.Ui()->DoLabel(&Title, Localize("Spectators"), 11, TEXTALIGN_ML);
		const float CellWidth = (Spectators.w - 20) / m_SpectatorColumns;
		int Entry = 0;
		for(const CNetObj_PlayerInfo *pInfo : m_Component.GameClient()->m_Snap.m_apInfoByName)
		{
			if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
				continue;
			CUIRect Cell = {Spectators.x + 10 + (Entry % m_SpectatorColumns) * CellWidth,
				Spectators.y + 24 + (Entry / m_SpectatorColumns) * m_SpectatorRow, CellWidth - 6, m_SpectatorRow};
			++Entry;
			const auto &Data = m_Component.GameClient()->m_aClients[pInfo->m_ClientId];
			auto &Player = m_aPlayers[pInfo->m_ClientId];
			char aEntry[128] = "";
			if(g_Config.m_ClShowIds)
			{
				char aId[16];
				m_Component.GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::NO_INDENT);
				str_copy(aEntry, aId);
			}
			if(Data.m_aClan[0])
			{
				str_append(aEntry, Data.m_aClan);
				str_append(aEntry, " ");
			}
			str_append(aEntry, Data.m_aName);
			LucentScoreboard::SingleLine(aEntry);
			const float Font = std::min(10.f, m_SpectatorRow * .72f);
			const float NameWidth = Cell.w * .68f, ClanWidth = std::max(.1f, Cell.w - NameWidth - 3);
			char aName[128] = "", aClan[64];
			if(g_Config.m_ClShowIds)
			{
				char aId[16];
				m_Component.GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::NO_INDENT);
				str_copy(aName, aId);
			}
			str_append(aName, Data.m_aName);
			str_copy(aClan, Data.m_aClan);
			LucentScoreboard::SingleLine(aName);
			LucentScoreboard::SingleLine(aClan);
			Player.m_Name.Update(m_Component.TextRender(), aName, Font, NameWidth, LucentScoreboard::SingleLineTextFlags);
			Player.m_Clan.Update(m_Component.TextRender(), aClan, Font, ClanWidth, LucentScoreboard::SingleLineTextFlags);

			if(m_Component.m_MouseUnlocked)
			{
				const int ButtonResult = m_Component.Ui()->DoButtonLogic(&Player.m_PlayerButtonId, 0, &Cell, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
				if(ButtonResult != 0)
				{
					m_Component.m_ScoreboardPopupContext.m_pScoreboard = &m_Component;
					m_Component.m_ScoreboardPopupContext.m_ClientId = pInfo->m_ClientId;
					m_Component.m_ScoreboardPopupContext.m_IsLocal = m_Component.GameClient()->m_aLocalIds[0] == pInfo->m_ClientId ||
											 (m_Component.Client()->DummyConnected() && m_Component.GameClient()->m_aLocalIds[1] == pInfo->m_ClientId);
					m_Component.m_ScoreboardPopupContext.m_IsSpectating = true;

					m_Component.Ui()->DoPopupMenu(&m_Component.m_ScoreboardPopupContext, m_Component.Ui()->MouseX(), m_Component.Ui()->MouseY(), POPUP_WIDTH,
						m_Component.m_ScoreboardPopupContext.m_IsLocal ? 30.0f : 60.0f, &m_Component.m_ScoreboardPopupContext, CScoreboard::CScoreboardPopupContext::Render);
				}

				if(m_Component.Ui()->HotItem() == &m_aPlayers[pInfo->m_ClientId].m_PlayerButtonId ||
					(m_Component.Ui()->IsPopupOpen(&m_Component.m_ScoreboardPopupContext) && m_Component.m_ScoreboardPopupContext.m_ClientId == pInfo->m_ClientId))
				{
					Cell.Draw(LucentStyle::Style::Accent.WithAlpha(LucentStyle::Style::Selected), IGraphics::CORNER_ALL, 2.f);
				}
			}

			if(m_Component.m_MouseUnlocked)
			{
				str_copy(Player.m_aSpectatorTooltip, aEntry);
				m_Component.Ui()->DoToolTip(&Player.m_Name, &Cell, Player.m_aSpectatorTooltip);
			}
			const float TextY = Cell.y + (Cell.h - Font) / 2;
			const auto NameColor = Data.m_AuthLevel ? color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor)) : m_Component.TextRender()->DefaultTextColor();
			const int Local = m_Component.GameClient()->m_aLocalIds[g_Config.m_ClDummy];
			const auto ClanColor = Local >= 0 && str_comp(Data.m_aClan, m_Component.GameClient()->m_aClients[Local].m_aClan) == 0 ? color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor)) : LucentStyle::Style::SecondaryText;
			Player.m_Name.Render(m_Component.TextRender(), {Cell.x, TextY}, NameColor);
			Player.m_Clan.Render(m_Component.TextRender(), {Cell.x + NameWidth + 3, TextY}, ClanColor);
		}
	}

	void CRenderer::RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, float ScoreWidth)
	{
		dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");
		if(CountEnd <= CountStart)
			return;

		const CNetObj_GameInfo *pGameInfoObj = m_Component.GameClient()->m_Snap.m_pGameInfoObj;
		const CNetObj_GameData *pGameDataObj = m_Component.GameClient()->m_Snap.m_pGameDataObj;
		const bool TimeScore = m_Component.GameClient()->m_GameInfo.m_TimeScore;
		const bool MillisecondScore = m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimes;
		const bool TrueMilliseconds = m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis;

		bool Race7 = m_Component.Client()->IsSixup() && pGameInfoObj && pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE;

		const bool UseTime = Race7 || TimeScore || MillisecondScore;

		const float LineHeight = m_RowHeight;
		const float FontSize = m_FontSize;
		const float TeeSizeMod = LineHeight / 60.f;
		const float Spacing = 0;
		const float RoundRadius = std::min(4.f, LineHeight / 4);
		const auto Fields = LucentScoreboard::CFields::Calculate(Scoreboard.w, FontSize, ScoreWidth, m_PointsWidth, m_PingWidth, 0, 0);
		const float BaseScoreOffset = Scoreboard.x + Fields.m_Score, ScoreLength = Fields.m_ScoreWidth;
		const float BaseTeeOffset = Scoreboard.x + Fields.m_Tee, TeeLength = Fields.m_TeeWidth;
		const float BaseNameOffset = Scoreboard.x + Fields.m_Name, IdentityWidth = Fields.m_IdentityWidth;
		const float BaseCountryOffset = Scoreboard.x + Fields.m_Country, CountryLength = Fields.m_CountryWidth;
		const float CountryFlagSize = std::max(0.f, LineHeight - CountryLength / 2);
		const float BasePointsOffset = Scoreboard.x + Fields.m_Points, PointsLength = Fields.m_PointsWidth;
		const float BasePingOffset = Scoreboard.x + Fields.m_Ping, PingLength = Fields.m_PingWidth;

		// render headlines
		const float HeadlineFontsize = std::min(10.f, FontSize);
		CUIRect Headline;
		Scoreboard.HSplitTop(22.0f, &Headline, &Scoreboard);
		const float HeadlineY = Headline.y + Headline.h / 2.0f - HeadlineFontsize / 2.0f;
		const ColorRGBA HeadlineColor = m_Component.TextRender()->DefaultTextColor();
		m_HeadlineScore.Update(m_Component.TextRender(), UseTime ? Localize("Time") : Localize("Score"), HeadlineFontsize);
		m_HeadlineName.Update(m_Component.TextRender(), Localize("Player"), HeadlineFontsize);
		m_HeadlinePing.Update(m_Component.TextRender(), Localize("Ping"), HeadlineFontsize);
		m_HeadlinePoints.Update(m_Component.TextRender(), "Pts", HeadlineFontsize);
		if(ScoreWidth > 0)
			m_HeadlineScore.Render(m_Component.TextRender(), vec2(BaseScoreOffset + ScoreLength - m_HeadlineScore.Width(), HeadlineY), HeadlineColor);
		m_HeadlineName.Render(m_Component.TextRender(), vec2(BaseNameOffset, HeadlineY), HeadlineColor);
		m_HeadlinePoints.Render(m_Component.TextRender(), vec2(BasePointsOffset + PointsLength - m_HeadlinePoints.Width(), HeadlineY), HeadlineColor);
		m_HeadlinePing.Render(m_Component.TextRender(), vec2(BasePingOffset + PingLength - m_HeadlinePing.Width(), HeadlineY), HeadlineColor);

		const auto &Content = m_Planner.m_Input.m_Content;
		const auto GroupRows = CGroupRows::Calculate(Content.m_Team[Team], {CountStart, CountEnd});
		for(int Index = 0; Index < GroupRows.m_Count; ++Index)
		{
			const auto &Group = GroupRows.m_Groups[Index];
			if(Group.m_Team >= int(m_aDDTeamLabels.size()))
				continue;
			const CUIRect Rect = {Scoreboard.x + 2, Scoreboard.y + Group.m_Top * LineHeight, std::max(0.f, Scoreboard.w - 4), (Group.m_Bottom - Group.m_Top) * LineHeight};
			const auto &Geometry = m_aGroupPanels[Group.m_Start].Get({Rect.x, Rect.y, Rect.w, Rect.h}, LucentHud::PixelGrid(m_Component.Graphics()));
			const auto TeamColor = m_Component.GameClient()->GetDDTeamColor(Group.m_Team);
			const auto Fill = LucentStyle::Gradient::Blend(LucentStyle::Scoreboard::Surface, TeamColor, LucentStyle::Scoreboard::GroupTint).WithAlpha(LucentStyle::Scoreboard::GroupFillAlpha * m_Opacity);
			const auto Border = LucentStyle::Gradient::Blend(TeamColor, LucentStyle::Style::Text, LucentStyle::Scoreboard::GroupBorderLighten).WithAlpha(LucentStyle::Scoreboard::GroupBorderAlpha * m_Opacity);
			LucentHud::DrawSoftFill(m_Component.Graphics(), Geometry, Fill, Fill);
			DrawTeamBorder(m_Component.Graphics(), Geometry, Border);
			auto &Label = m_aDDTeamLabels[Group.m_Team];
			const float TitleFont = std::min(10.f, LineHeight * .45f), CountFont = std::min(8.f, LineHeight * .36f);
			const float Left = Scoreboard.x + 7 * FontSize / 12;
			const float Top = Rect.y + GroupPaddingRows * LineHeight;
			const float Limit = std::max(.01f, Scoreboard.w - 14 * FontSize / 12);
			Label.m_Title.Update(m_Component.TextRender(), Label.m_aText, TitleFont, Limit, SingleLineTextFlags);
			Label.m_Count.Update(m_Component.TextRender(), Label.m_aCount, CountFont, Limit, SingleLineTextFlags);
			Label.m_Title.Render(m_Component.TextRender(), {Left, Top}, LucentStyle::Scoreboard::Nickname);
			Label.m_Count.Render(m_Component.TextRender(), {Left, Top + GroupHeaderRows * LineHeight * .55f}, LucentStyle::Style::DetailText);
			if(m_Component.m_MouseUnlocked)
				m_Component.Ui()->DoToolTip(&Label.m_Title, &Rect, Label.m_aText);
		}

		// render player entries
		int CountRendered = 0;

		char aBuf[64];

		for(int RenderDead = 0; RenderDead < 2; RenderDead++)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				// make sure that we render the correct team
				const CNetObj_PlayerInfo *pInfo = m_Component.GameClient()->m_Snap.m_apInfoByDDTeamScore[i];
				if(!pInfo || pInfo->m_Team != Team)
					continue;
				bool IsDead = m_Component.Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_DEAD;
				if(!RenderDead && IsDead)
					continue;
				if(RenderDead && !IsDead)
					continue;
				if(CountRendered++ < CountStart)
					continue;

				ColorRGBA TextColor = m_Component.TextRender()->DefaultTextColor();
				TextColor.a = RenderDead ? 0.5f : 1.0f;
				m_Component.TextRender()->TextColor(TextColor);
				CUIRect Row = {Scoreboard.x, Scoreboard.y + GroupRows.m_RowY[CountRendered - 1] * LineHeight, Scoreboard.w, LineHeight};
				const float DeltaX = 0;
				Row.x += DeltaX;
				Row.y += 2.f * (1.f - LucentStyle::Motion::CubicOut(float((m_Now - m_RevealStart) / .08)));
				const float ScoreOffset = BaseScoreOffset + DeltaX;
				const float TeeOffset = BaseTeeOffset + DeltaX;
				const float NameOffset = BaseNameOffset + DeltaX;
				const float PointsOffset = BasePointsOffset + DeltaX;
				const float PingOffset = BasePingOffset + DeltaX;
				const float CountryOffset = BaseCountryOffset + DeltaX;
				const float TextY = Row.y + (Row.h - FontSize) / 2.0f;

				// background so it's easy to find the local player or the followed one in spectator mode
				if((!m_Component.GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_Local) ||
					(m_Component.GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW && pInfo->m_Local) ||
					(m_Component.GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientId == m_Component.GameClient()->m_Snap.m_SpecInfo.m_SpectatorId))
				{
					LucentHud::SoftFill(m_Component.Graphics(), {Row.x, Row.y, Row.w, Row.h}, LucentStyle::Gradient::Blend(LucentStyle::Style::ServerSelection, LucentStyle::Style::Accent, LucentStyle::Style::SceneBase).WithAlpha(.42f * m_Opacity), .6f);
				}

				const CGameClient::CClientData &ClientData = m_Component.GameClient()->m_aClients[pInfo->m_ClientId];
				CPlayerElement &Player = m_aPlayers[pInfo->m_ClientId];

				if(m_Component.m_MouseUnlocked)
				{
					const int ButtonResult = m_Component.Ui()->DoButtonLogic(&Player.m_PlayerButtonId, 0, &Row, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
					if(ButtonResult != 0)
					{
						m_Component.m_ScoreboardPopupContext.m_pScoreboard = &m_Component;
						m_Component.m_ScoreboardPopupContext.m_ClientId = pInfo->m_ClientId;
						m_Component.m_ScoreboardPopupContext.m_IsLocal = m_Component.GameClient()->m_aLocalIds[0] == pInfo->m_ClientId ||
												 (m_Component.Client()->DummyConnected() && m_Component.GameClient()->m_aLocalIds[1] == pInfo->m_ClientId);
						m_Component.m_ScoreboardPopupContext.m_IsSpectating = false;

						m_Component.Ui()->DoPopupMenu(&m_Component.m_ScoreboardPopupContext, m_Component.Ui()->MouseX(), m_Component.Ui()->MouseY(), POPUP_WIDTH,
							m_Component.m_ScoreboardPopupContext.m_IsLocal ? 58.5f : 87.5f, &m_Component.m_ScoreboardPopupContext, CScoreboard::CScoreboardPopupContext::Render);
					}

					if(m_Component.Ui()->HotItem() == &Player.m_PlayerButtonId ||
						m_Component.Ui()->HotItem() == &Player.m_TimeTooltipId ||
						(m_Component.Ui()->IsPopupOpen(&m_Component.m_ScoreboardPopupContext) && m_Component.m_ScoreboardPopupContext.m_ClientId == pInfo->m_ClientId))
					{
						Row.Draw(LucentStyle::Gradient::Blend(LucentStyle::Style::ServerSelection, LucentStyle::Style::Accent, LucentStyle::Style::SceneBase).WithAlpha(.25f), IGraphics::CORNER_ALL, RoundRadius);
					}
				}

				// score
				CUIRect ScorePosition;
				ScorePosition.x = ScoreOffset;
				ScorePosition.w = ScoreLength;
				ScorePosition.y = Row.y;
				ScorePosition.h = Row.h;

				auto DoMillisecondTooltip = [&](const CUIRect *pRect, int64_t TimeInMillis, float WidthHint) {
					if(m_Component.m_MouseUnlocked && pRect->Inside(m_Component.Ui()->MousePos()))
					{
						m_Component.Ui()->SetHotItem(&Player.m_TimeTooltipId);
						str_time(TimeInMillis / 10, ETimeFormat::HOURS_CENTISECS, Player.m_aTimeTooltip, sizeof(Player.m_aTimeTooltip));
						m_Component.Ui()->DoToolTip({&Player.m_TimeTooltipId,
							*pRect,
							Player.m_aTimeTooltip,
							WidthHint * 1.5f,
							FontSize,
							true});
					}
				};
				constexpr int SecondsPerHour = 60 * 60;

				const auto Mode = Race7 ? EScoreMode::RACE7 : MillisecondScore ? EScoreMode::FINISH :
								      TimeScore                ? EScoreMode::TIMESCORE :
												 EScoreMode::POINTS;
				const bool KnownScore = CRowScore::Read(Mode, pInfo->m_Score, ClientData.m_FinishTimeSeconds, ClientData.m_FinishTimeMillis).m_Known;
				if(KnownScore)
				{
					if(Race7)
					{
						m_Component.Ui()->RenderTime(ScorePosition, FontSize, pInfo->m_Score / 1000, pInfo->m_Score == protocol7::FinishTime::NOT_FINISHED, pInfo->m_Score % 1000, true,
							Player.m_Score, Player.m_ScoreMillis, TextColor);
						if(pInfo->m_Score / 1000 >= SecondsPerHour)
						{
							DoMillisecondTooltip(&ScorePosition, time_milliseconds_from_seconds(pInfo->m_Score / 1000.0f), Player.m_Score.Width());
						}
					}
					else if(MillisecondScore)
					{
						m_Component.Ui()->RenderTime(ScorePosition, FontSize, ClientData.m_FinishTimeSeconds, ClientData.m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS, ClientData.m_FinishTimeMillis, TrueMilliseconds,
							Player.m_Score, Player.m_ScoreMillis, TextColor);
						if(ClientData.m_FinishTimeSeconds >= SecondsPerHour)
						{
							DoMillisecondTooltip(&ScorePosition, static_cast<int64_t>(ClientData.m_FinishTimeSeconds) * 1000 + ClientData.m_FinishTimeMillis, Player.m_Score.Width());
						}
					}
					else if(TimeScore)
					{
						m_Component.Ui()->RenderTime(ScorePosition, FontSize, pInfo->m_Score, pInfo->m_Score == FinishTime::NOT_FINISHED_TIMESCORE, -1, false,
							Player.m_Score, Player.m_ScoreMillis, TextColor);
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "%d", std::clamp(pInfo->m_Score, -999, 99999));
						Player.m_Score.Update(m_Component.TextRender(), aBuf, FontSize);
						Player.m_Score.Render(m_Component.TextRender(), vec2(ScoreOffset + ScoreLength - Player.m_Score.Width(), TextY), TextColor);
					}
				}

				// CTF flag
				if(pGameInfoObj && (pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) &&
					pGameDataObj && (pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientId || pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId))
				{
					m_Component.Graphics()->TextureSet(pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId ? m_Component.GameClient()->m_GameSkin.m_SpriteFlagBlue : m_Component.GameClient()->m_GameSkin.m_SpriteFlagRed);
					m_Component.Graphics()->QuadsBegin();
					m_Component.Graphics()->SetColor(1, 1, 1, m_Opacity);
					m_Component.Graphics()->QuadsSetSubset(1.0f, 0.0f, 0.0f, 1.0f);
					IGraphics::CQuadItem QuadItem(TeeOffset, Row.y - 2.5f - Spacing / 2.0f, Row.h / 2.0f, Row.h);
					m_Component.Graphics()->QuadsDrawTL(&QuadItem, 1);
					m_Component.Graphics()->QuadsEnd();
				}

				// skin
				if(RenderDead)
				{
					m_Component.Graphics()->TextureSet(m_DeadTeeTexture);
					m_Component.Graphics()->QuadsBegin();
					m_Component.Graphics()->SetColor(1, 1, 1, m_Opacity);
					if(m_Component.GameClient()->IsTeamPlay())
					{
						m_Component.Graphics()->SetColor(m_Component.GameClient()->m_Skins7.GetTeamColor(true, 0, m_Component.GameClient()->m_aClients[pInfo->m_ClientId].m_Team, protocol7::SKINPART_BODY).WithAlpha(m_Opacity));
					}
					CTeeRenderInfo TeeInfo = m_Component.GameClient()->m_aClients[pInfo->m_ClientId].m_RenderInfo;
					TeeInfo.m_Size *= TeeSizeMod;

					IGraphics::CQuadItem QuadItem(TeeOffset, Row.y, TeeInfo.m_Size, TeeInfo.m_Size);
					m_Component.Graphics()->QuadsDrawTL(&QuadItem, 1);
					m_Component.Graphics()->QuadsEnd();
				}
				else
				{
					CTeeRenderInfo TeeInfo = ClientData.m_RenderInfo;
					TeeInfo.m_Size *= TeeSizeMod;

					m_Component.RenderTools()->RenderIdleTeeAtCenter(&TeeInfo, vec2(TeeOffset + TeeLength / 2, Row.y + Row.h / 2.0f), EMOTE_NORMAL, vec2(1, 0), m_Opacity);

					if(m_Component.m_MouseUnlocked)
					{
						const CUIRect SkinRect = {TeeOffset, Row.y, TeeLength, Row.h};
						m_Component.Ui()->DoToolTip(&m_aPlayers[pInfo->m_ClientId].m_PlayerButtonId, &SkinRect, ClientData.m_aSkinName);
					}
				}

				const bool Ready = m_Component.Client()->IsSixup() && (m_Component.Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_READY);
				if(Ready)
					Player.m_ReadyMark.Update(m_Component.TextRender(), "✓", FontSize);
				if(g_Config.m_ClShowIds)
				{
					char aClientId[16];
					m_Component.GameClient()->FormatClientId(pInfo->m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
					str_copy(aBuf, aClientId);
					str_append(aBuf, ClientData.m_aName);
				}
				else
				{
					str_copy(aBuf, ClientData.m_aName);
				}
				LucentScoreboard::SingleLine(aBuf);
				char aClan[64];
				str_copy(aClan, ClientData.m_aClan);
				LucentScoreboard::SingleLine(aClan);
				const vec2 Metrics = m_Component.Graphics()->ScreenSize() / m_Component.Graphics()->GetScreen().Size();
				const auto Identity = LucentScoreboard::PrepareIdentityText(m_Component.TextRender(), Player.m_Name, Player.m_Clan,
					aBuf, aClan, IdentityWidth, FontSize, Row.h, Ready ? Player.m_ReadyMark.Width() : 0, Metrics);
				const float NameLength = std::max(.01f, IdentityWidth - Identity.m_Gap - Identity.m_ReadyWidth - Identity.m_ClanWidth);
				// name
				{
					if(m_Component.m_MouseUnlocked)
					{
						CUIRect Cell = {NameOffset, Row.y, NameLength, Row.h};
						m_Component.Ui()->DoToolTip(&Player.m_Name, &Cell, ClientData.m_aName);
					}

					ColorRGBA NameColor = LucentStyle::Scoreboard::Nickname.WithAlpha(TextColor.a);
					if(ClientData.m_AuthLevel)
					{
						NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
					}
					Player.m_Name.Render(m_Component.TextRender(), vec2(NameOffset, TextY), NameColor);

					// ready / watching
					if(Ready)
					{
						Player.m_ReadyMark.Render(m_Component.TextRender(), vec2(NameOffset + std::min(Player.m_Name.Width(), Identity.m_NameWidth), TextY), ColorRGBA(0.1f, 1.0f, 0.1f, TextColor.a));
					}
				}

				// Separate Clan data, presented as a compact tag immediately after the nickname.
				if(ClientData.m_aClan[0])
				{
					ColorRGBA ClanColor = LucentStyle::Scoreboard::ClanText.WithAlpha(TextColor.a);
					if(m_Component.GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0 && str_comp(ClientData.m_aClan, m_Component.GameClient()->m_aClients[m_Component.GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_aClan) == 0)
					{
						ClanColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor));
					}
					const auto Placement = CClanPillPlacement::Calculate(Identity, Player.m_Clan.Ink(), NameOffset, Player.m_Name.Width(), Row.y, Row.h, LucentHud::PixelGrid(m_Component.Graphics()));
					const CUIRect Pill = {Placement.m_Rect.x, Placement.m_Rect.y, Placement.m_Rect.w, Placement.m_Rect.h};
					const auto &Geometry = Player.m_ClanPill.Get({Pill.x, Pill.y, Pill.w, Pill.h}, LucentHud::PixelGrid(m_Component.Graphics()), true);
					const auto Fill = LucentStyle::Scoreboard::ClanSurface.WithMultipliedAlpha(m_Opacity);
					LucentHud::DrawSoftFill(m_Component.Graphics(), Geometry, Fill, Fill);
					if(m_Component.m_MouseUnlocked)
						m_Component.Ui()->DoToolTip(&Player.m_Clan, &Pill, ClientData.m_aClan);
					Player.m_Clan.Render(m_Component.TextRender(), Placement.m_Text, ClanColor);
				}

				// country flag
				m_Component.GameClient()->m_CountryFlags.Render(ClientData.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * m_Opacity),
					CountryOffset, Row.y + (Spacing + CountryFlagSize) / 2.0f, CountryLength, Row.h - Spacing - CountryFlagSize);

				// Public DDNet profile points, independent of map score/time and slot identity.
				if(Player.m_PointsValue >= 0)
					str_format(aBuf, sizeof(aBuf), "%d", Player.m_PointsValue);
				else
					str_copy(aBuf, "—");
				Player.m_Points.Update(m_Component.TextRender(), aBuf, FontSize);
				Player.m_Points.Render(m_Component.TextRender(), vec2(PointsOffset + PointsLength - Player.m_Points.Width(), TextY), TextColor);
				if(m_Component.m_MouseUnlocked)
				{
					CUIRect Cell = {PointsOffset, Row.y, PointsLength, Row.h};
					m_Component.Ui()->DoToolTip(&Player.m_Points, &Cell, "DDNet profile points for this nickname; not verified identity");
				}
				// ping
				ColorRGBA PingColor = g_Config.m_ClEnablePingColor ? CRenderTools::GetPingTextColor(pInfo->m_Latency) : m_Component.TextRender()->DefaultTextColor();
				str_format(aBuf, sizeof(aBuf), "%d", std::clamp(pInfo->m_Latency, 0, 999));
				Player.m_Ping.Update(m_Component.TextRender(), aBuf, FontSize);
				Player.m_Ping.Render(m_Component.TextRender(), vec2(PingOffset + PingLength - Player.m_Ping.Width(), TextY), PingColor);
				m_Component.TextRender()->TextColor(m_Component.TextRender()->DefaultTextColor());

				if(CountRendered == CountEnd)
					break;
			}
			if(CountRendered == CountEnd)
				break;
		}
	}

	void CRenderer::RenderMouseHint(CUIRect MouseHint)
	{
		constexpr float FontSize = 11.0f;
		constexpr float PillPaddingX = 8.0f;
		constexpr float PillPaddingY = 5.0f;
		constexpr float TextKeycapGap = 5.0f;
		constexpr float KeycapPaddingX = 4.0f;
		constexpr float KeycapPaddingY = 2.0f;
		constexpr float KeycapRadius = 3.5f;

		char aKey[64];
		m_Component.GameClient()->m_Binds.GetKey(CScoreboard::CURSOR_BIND_NAME, aKey, sizeof(aKey));

		char aHint[128];
		if(!aKey[0])
		{
			str_copy(aHint, Localize("not bound"));
		}
		else
		{
			str_copy(aHint, Localize("Show cursor"));
		}

		const bool HasKeycap = aKey[0] != '\0';
		const char *pChipText = HasKeycap ? aKey : CScoreboard::CURSOR_BIND_NAME;
		const float TextWidth = m_Component.TextRender()->TextWidth(FontSize, aHint, -1);
		const float ChipWidth = m_Component.TextRender()->TextWidth(FontSize, pChipText, -1) + 2 * KeycapPaddingX;
		const float ContentWidth = ChipWidth + TextKeycapGap + TextWidth;
		const float PillHeight = FontSize + 2 * PillPaddingY;
		const float PillWidth = std::min(ContentWidth + 2 * PillPaddingX, MouseHint.w);

		// content-sized pill, top center in the available space
		CUIRect Pill = {MouseHint.x + (MouseHint.w - PillWidth) / 2.0f, MouseHint.y, PillWidth, PillHeight};
		Pill.Draw(BACKGROUND_COLOR, IGraphics::CORNER_ALL, ROUNDING);

		CUIRect Content = Pill;
		Content.x += (Pill.w - ContentWidth) / 2.0f;
		Content.w = ContentWidth;

		CUIRect Label, Chip;
		if(HasKeycap)
		{
			// key behind the text: "Show cursor [key]"
			Content.VSplitLeft(TextWidth, &Label, &Content);
			Content.VSplitLeft(TextKeycapGap, nullptr, &Content);
			Content.VSplitLeft(ChipWidth, &Chip, nullptr);
		}
		else
		{
			// bind name in front of the text: "[bind name] not bound"
			Content.VSplitLeft(ChipWidth, &Chip, &Content);
			Content.VSplitLeft(TextKeycapGap, nullptr, &Content);
			Label = Content;
		}

		// floating chip, vertically centered in the pill
		Chip.HMargin((Chip.h - (FontSize + 2 * KeycapPaddingY)) / 2.0f, &Chip);
		Chip.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f), IGraphics::CORNER_ALL, KeycapRadius);

		m_Component.Ui()->DoLabel(&Label, aHint, FontSize, TEXTALIGN_ML);
		m_Component.Ui()->DoLabel(&Chip, pChipText, FontSize, TEXTALIGN_MC);
	}

	void CRenderer::RenderRecordingNotification(float x)
	{
		char aBuf[512] = "";

		const auto &&AppendRecorderInfo = [&](int Recorder, const char *pName) {
			if(m_Component.GameClient()->DemoRecorder(Recorder)->IsRecording())
			{
				char aTime[32];
				str_time((int64_t)m_Component.GameClient()->DemoRecorder(Recorder)->Length() * 100, ETimeFormat::HOURS, aTime, sizeof(aTime));
				str_append(aBuf, pName);
				str_append(aBuf, " ");
				str_append(aBuf, aTime);
				str_append(aBuf, "  ");
			}
		};

		AppendRecorderInfo(RECORDER_MANUAL, Localize("Manual"));
		AppendRecorderInfo(RECORDER_RACE, Localize("Race"));
		AppendRecorderInfo(RECORDER_AUTO, Localize("Auto"));
		AppendRecorderInfo(RECORDER_REPLAYS, Localize("Replay"));

		if(aBuf[0] == '\0')
			return;

		constexpr float FontSize = 10.0f;
		constexpr float CircleSize = 10.0f;
		constexpr float Gap = 5.0f;

		CUIRect Rect = {x, 0.0f, m_Component.TextRender()->TextWidth(FontSize, aBuf) + 30.0f, 25.0f};
		Rect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, ROUNDING);
		Rect.VSplitLeft(CircleSize, nullptr, &Rect);
		Rect.VSplitRight(Gap, &Rect, nullptr);

		CUIRect Circle;
		Rect.VSplitLeft(CircleSize, &Circle, &Rect);
		Circle.HMargin((Circle.h - Circle.w) / 2.0f, &Circle);
		Circle.Draw(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), IGraphics::CORNER_ALL, Circle.h / 2.0f);

		Rect.VSplitLeft(Gap, nullptr, &Rect);
		m_Component.Ui()->DoLabel(&Rect, aBuf, FontSize, TEXTALIGN_ML);
	}

	void CRenderer::Render(bool Visible)
	{
		const double Now = time_get() / double(time_freq());
		m_Now = Now;
		m_Opacity = m_Visibility.Update(Visible, Now, true, Visible ? .17f : .11f);
		if(Visible && !m_WasVisible)
			m_RevealStart = Now;
		m_WasVisible = Visible;
		if(m_Opacity <= 0.f)
			return;
		LucentUi::CScope StyleScope(m_Component.Ui());
		LucentUi::COpacityScope OpacityScope(m_Component.TextRender(), m_Opacity);
		LucentUi::CInteractionScope InteractionScope(m_Component.Ui(), Visible);

		if(!m_Component.GameClient()->m_Menus.IsActive() && !m_Component.GameClient()->m_Chat.IsActive())
		{
			m_Component.Ui()->StartCheck();
			m_Component.Ui()->Update();
		}

		if(m_Component.GameClient()->m_Motd.IsActive())
			m_Component.GameClient()->m_Motd.Clear();
		const CUIRect Screen = *m_Component.Ui()->Screen();
		m_Component.Ui()->MapScreen();
		const CNetObj_GameInfo *pGameInfoObj = m_Component.GameClient()->m_Snap.m_pGameInfoObj;
		const bool Teams = m_Component.GameClient()->IsTeamPlay();
		const auto &aTeamSize = m_Component.GameClient()->m_Snap.m_aTeamSize;
		std::array<const char *, 128> aPointsNames{};
		for(const auto *pInfo : m_Component.GameClient()->m_Snap.m_apInfoByName)
			if(pInfo && pInfo->m_Team != TEAM_SPECTATORS)
				aPointsNames[pInfo->m_ClientId] = m_Component.GameClient()->m_aClients[pInfo->m_ClientId].m_aName;
		if(Visible && m_Component.Client()->State() == IClient::STATE_ONLINE)
			m_Component.m_PointsService.Update(aPointsNames, Now, m_Component.Http(), m_Component.Engine());
		const bool ReadPoints = Now >= m_Component.m_NextPointsRead;
		if(ReadPoints)
			m_Component.m_NextPointsRead = Now + .25;
		const bool UsesTime = m_Component.GameClient()->m_GameInfo.m_TimeScore || m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimes ||
				      (m_Component.Client()->IsSixup() && pGameInfoObj && (pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE));
		m_PointsWidth = std::max(m_Component.TextRender()->TextWidth(12, "999999"), m_Component.TextRender()->TextWidth(12, "Pts")) + 4;
		m_PingWidth = std::max(m_Component.TextRender()->TextWidth(12, "999"), m_Component.TextRender()->TextWidth(12, Localize("Ping"))) + 2;
		int SpectatorCount = 0;
		std::array<int, MAX_CLIENTS + 1> aDDTeamCounts{};
		std::array<float, MAX_CLIENTS> aIdentityDemand{};
		for(const auto *pInfo : m_Component.GameClient()->m_Snap.m_apInfoByName)
		{
			if(!pInfo)
				continue;
			if(pInfo->m_Team == TEAM_SPECTATORS)
			{
				++SpectatorCount;
				continue;
			}
			const auto &Data = m_Component.GameClient()->m_aClients[pInfo->m_ClientId];
			auto &Player = m_aPlayers[pInfo->m_ClientId];
			const int DDTeam = m_Component.GameClient()->m_Teams.Team(pInfo->m_ClientId);
			if(DDTeam > TEAM_FLOCK && DDTeam < int(aDDTeamCounts.size()))
				++aDDTeamCounts[DDTeam];
			if(ReadPoints || str_comp(Player.m_aPointsName, Data.m_aName))
			{
				str_copy(Player.m_aPointsName, Data.m_aName);
				Player.m_PointsValue = m_Component.m_PointsService.Lookup(Data.m_aName, Now);
			}
			if(Player.m_PointsValue >= 1000000)
			{
				char aPts[16];
				str_format(aPts, sizeof(aPts), "%d", Player.m_PointsValue);
				m_PointsWidth = std::max(m_PointsWidth, m_Component.TextRender()->TextWidth(12, aPts) + 4);
			}
			char aName[128];
			if(g_Config.m_ClShowIds)
			{
				char aId[16];
				m_Component.GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::INDENT_AUTO);
				str_format(aName, sizeof(aName), "%s%s", aId, Data.m_aName);
			}
			else
				str_copy(aName, Data.m_aName);
			LucentScoreboard::SingleLine(aName);
			char aClan[MAX_CLAN_LENGTH];
			str_copy(aClan, Data.m_aClan);
			LucentScoreboard::SingleLine(aClan);
			const float NameWidth = IdentityTextWidth(m_Component.TextRender(), 12, aName);
			const float ClanWidth = aClan[0] ? IdentityTextWidth(m_Component.TextRender(), 12 * LucentStyle::Scoreboard::ClanFontScale, aClan) : 0;
			const auto Grid = LucentHud::PixelGrid(m_Component.Graphics());
			const float ClanDemand = aClan[0] ? std::ceil(ClanWidth / Grid.m_Step.x) * Grid.m_Step.x + 2 * LucentStyle::Scoreboard::ClanPadding + LucentStyle::Scoreboard::ClanGap : 0;
			const bool Ready = m_Component.Client()->IsSixup() && (m_Component.Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_READY);
			aIdentityDemand[pInfo->m_ClientId] = NameWidth + ClanDemand + (Ready ? IdentityTextWidth(m_Component.TextRender(), 12, "✓") : 0);
		}
		for(int Team = 1; Team < int(aDDTeamCounts.size()); ++Team)
		{
			const int Count = aDDTeamCounts[Team];
			if(!Count)
				continue;
			auto &Label = m_aDDTeamLabels[Team];
			if(Team == m_Component.GameClient()->m_Teams.TeamSuper())
				str_copy(Label.m_aText, Localize("Super"));
			else
				str_format(Label.m_aText, sizeof(Label.m_aText), Localize("Team %d"), Team);
			const int Maximum = Team == m_Component.GameClient()->m_Teams.TeamSuper() ? 0 : m_Component.GameClient()->m_GameInfo.m_MaxTeamSize;
			if(Maximum > 0)
				str_format(Label.m_aCount, sizeof(Label.m_aCount), "%d/%d", Count, Maximum);
			else
				str_format(Label.m_aCount, sizeof(Label.m_aCount), "%d", Count);
			Label.m_Width = std::max(IdentityTextWidth(m_Component.TextRender(), 10, Label.m_aText), IdentityTextWidth(m_Component.TextRender(), 8, Label.m_aCount));
		}
		// Match RenderScoreboard's sorted alive-then-dead traversal, including game-team filtering.
		CContentWidths Content;
		Content.m_Chrome = CFields::ChromeWidth(0, m_PointsWidth, m_PingWidth);
		Content.m_Independent = true;
		Content.m_Points = m_PointsWidth;
		Content.m_Ping = m_PingWidth;
		std::array<int, 2> aOrderedCounts{};
		for(int Dead = 0; Dead < 2; ++Dead)
			for(const auto *pInfo : m_Component.GameClient()->m_Snap.m_apInfoByDDTeamScore)
			{
				if(!pInfo || pInfo->m_Team < TEAM_RED || pInfo->m_Team > TEAM_BLUE)
					continue;
				const bool IsDead = m_Component.Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_DEAD;
				if(IsDead != bool(Dead))
					continue;
				const int DDTeam = m_Component.GameClient()->m_Teams.Team(pInfo->m_ClientId);
				const int Index = aOrderedCounts[pInfo->m_Team]++;
				Content.m_Team[pInfo->m_Team][Index] = DDTeam;
				Content.m_GroupTitle[pInfo->m_Team][Index] = DDTeam > TEAM_FLOCK && DDTeam < int(m_aDDTeamLabels.size()) ? m_aDDTeamLabels[DDTeam].m_Width : 0;
				Content.m_Identity[pInfo->m_Team][Index] = aIdentityDemand[pInfo->m_ClientId];
				const auto &Data = m_Component.GameClient()->m_aClients[pInfo->m_ClientId];
				const bool Race7 = m_Component.Client()->IsSixup() && pGameInfoObj && (pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE);
				const auto Mode = Race7 ? EScoreMode::RACE7 : m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimes ? EScoreMode::FINISH :
								      m_Component.GameClient()->m_GameInfo.m_TimeScore                   ? EScoreMode::TIMESCORE :
																	   EScoreMode::POINTS;
				const auto Value = CRowScore::Read(Mode, pInfo->m_Score, Data.m_FinishTimeSeconds, Data.m_FinishTimeMillis);
				if(Value.m_Known)
				{
					char aTime[64];
					if(Mode == EScoreMode::POINTS)
						str_format(aTime, sizeof(aTime), "%d", std::clamp(pInfo->m_Score, -999, 99999));
					else
						str_time(absolute(int64_t(Value.m_Seconds)) * 100, ETimeFormat::HOURS, aTime, sizeof(aTime));
					float Width = IdentityTextWidth(m_Component.TextRender(), 12, aTime);
					if(Mode != EScoreMode::POINTS && Value.m_Millis >= 0 && Value.m_Seconds < 3600)
					{
						char aFraction[16];
						const bool TrueMillis = Race7 || m_Component.GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis;
						if(TrueMillis)
							str_format(aFraction, sizeof(aFraction), ".%03d", Value.m_Millis % 1000);
						else
							str_format(aFraction, sizeof(aFraction), ".%02d", int(std::round((Value.m_Millis % 1000) / 10.f)));
						Width += IdentityTextWidth(m_Component.TextRender(), 12 * .7f, aFraction);
					}
					Content.m_Score[pInfo->m_Team][Index] = std::max(Width, IdentityTextWidth(m_Component.TextRender(), 10, UsesTime ? Localize("Time") : Localize("Score"))) + 2;
				}
			}
		CInput Input{Screen.w, Screen.h, aTeamSize[TEAM_RED], aTeamSize[TEAM_BLUE], Teams, SpectatorCount, 0,
			CFields::MinimumWidth(0, m_PointsWidth, m_PingWidth)};
		Input.m_Content = Content;
		const auto &Layout = m_Planner.Update(Input);
		const float SpectatorHeight = SpectatorCount > 0 ? m_SpectatorExtent.Update(Layout.m_SpectatorHeight, Now) : 0.f;
		if(SpectatorCount == 0)
			m_SpectatorExtent = {};
		const float SpectatorReveal = m_SpectatorVisibility.Update(SpectatorCount > 0, Now, true, .16f);
		// Content always uses the complete target grid. Animated surfaces never clip new rows.
		m_RowHeight = Layout.m_Row;
		m_SpectatorColumns = Layout.m_SpectatorColumns;
		m_SpectatorRow = Layout.m_SpectatorRow;
		const float Scale = m_Component.m_MouseUnlocked ? 1.f : LucentStyle::Motion::Lerp(.98f, 1.f, m_Opacity);
		// Map around the UI center, leaving mouse coordinates in the same space.
		const float ExtraX = Screen.w * (1 / Scale - 1) / 2;
		const float ExtraY = Screen.h * (1 / Scale - 1) / 2;
		m_Component.Graphics()->MapScreen({{-ExtraX, -ExtraY}, {Screen.w + ExtraX, Screen.h + ExtraY}});
		const float AnimatedTop = m_Top.Update(std::max(56.f, (Screen.h - Layout.m_Height - Layout.m_SpectatorHeight - 42) / 2), Now);
		// Keep newly arrived content inside the viewport even while a compact layout expands.
		const float Top = std::min(AnimatedTop, std::max(14.f, Screen.h - Layout.m_Height - Layout.m_SpectatorHeight - 64.f));
		CUIRect Scoreboard = {(Screen.w - Layout.m_Width) / 2, Top, Layout.m_Width, Layout.m_Height};
		RenderHeader(Scoreboard.x + Scoreboard.w / 2, Top, Screen.w);
		const char *pRedTeamName = m_Component.GetTeamName(TEAM_RED);
		const char *pBlueTeamName = m_Component.GetTeamName(TEAM_BLUE);
		if(Teams && pGameInfoObj)
		{
			// Game over title
			const CNetObj_GameData *pGameDataObj = m_Component.GameClient()->m_Snap.m_pGameDataObj;
			if((pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && pGameDataObj)
			{
				char aTitle[256];
				if(pGameDataObj->m_TeamscoreRed > pGameDataObj->m_TeamscoreBlue)
				{
					m_Component.TextRender()->TextColor(TEAM_RED_TITLE_COLOR);
					if(pRedTeamName == nullptr)
					{
						str_copy(aTitle, Localize("Red team wins!"));
					}
					else
					{
						str_format(aTitle, sizeof(aTitle), Localize("%s wins!"), pRedTeamName);
					}
				}
				else if(pGameDataObj->m_TeamscoreBlue > pGameDataObj->m_TeamscoreRed)
				{
					m_Component.TextRender()->TextColor(TEAM_BLUE_TITLE_COLOR);
					if(pBlueTeamName == nullptr)
					{
						str_copy(aTitle, Localize("Blue team wins!"));
					}
					else
					{
						str_format(aTitle, sizeof(aTitle), Localize("%s wins!"), pBlueTeamName);
					}
				}
				else
				{
					m_Component.TextRender()->TextColor(ColorRGBA(0.91f, 0.78f, 0.33f, 1.0f));
					str_copy(aTitle, Localize("Draw!"));
				}

				const float TitleFontSize = 36.0f;
				CUIRect GameOverTitle = {Scoreboard.x, Scoreboard.y - TitleFontSize - 6.0f, Scoreboard.w, TitleFontSize};
				m_Component.Ui()->DoLabel(&GameOverTitle, aTitle, TitleFontSize, TEXTALIGN_MC);
				m_Component.TextRender()->TextColor(m_Component.TextRender()->DefaultTextColor());
			}
		}
		std::array<LucentHud::CRect, LucentScoreboard::MaxColumns> aTargets{};
		std::array<bool, LucentScoreboard::MaxColumns> aVisible{};
		float Bottom = Scoreboard.y + Layout.m_Height + LucentScoreboard::ColumnGap;
		for(int Index = 0; Index < Layout.m_Columns; ++Index)
		{
			const int Group = Teams && Index >= Layout.m_RedColumns ? 1 : 0;
			const int Columns = Group ? Layout.m_BlueColumns : Layout.m_RedColumns;
			const int LocalColumn = Index - (Group ? Layout.m_RedColumns : 0);
			const int Count = aTeamSize[Group == 0 ? TEAM_RED : TEAM_BLUE];
			const auto Range = Layout.m_Ranges[Index];
			aTargets[Index] = LucentScoreboard::ColumnBounds(Layout, Scoreboard.x, Scoreboard.y, Index, Range.m_End - Range.m_Start);
			aVisible[Index] = Range.m_End > Range.m_Start;
		}
		// Each column owns a coverage mesh and retargetable bounds; there is no grid-wide fill.
		std::array<LucentHud::CRect, LucentScoreboard::MaxColumns> aBounds{};
		for(int Index = 0; Index < LucentScoreboard::MaxColumns; ++Index)
		{
			auto &Motion = m_aColumnMotion[Index];
			aBounds[Index] = Motion.Update(aTargets[Index], Now, aVisible[Index]);
			// Keep the capsule-to-columns gap exact while the shared anchor moves.
			aBounds[Index].y = Scoreboard.y + LucentScoreboard::HeaderHeight + LucentScoreboard::HeaderGap;
			const auto &Bounds = aBounds[Index];
			if(Motion.m_Visibility.m_Value > 0)
			{
				DrawFloatingPanel({Bounds.x, Bounds.y, Bounds.w, Bounds.h}, 2 + Index, m_Opacity * Motion.m_Visibility.m_Value);
			}
		}
		m_FontSize = std::min(12.f, m_RowHeight * .65f);
		for(int Index = 0; Index < Layout.m_Columns; ++Index)
			if(aVisible[Index])
				m_FontSize = std::min(m_FontSize, CommonFont(m_RowHeight, aBounds[Index].w,
									  CFields::ChromeWidth(Layout.m_Scores[Index], m_PointsWidth, m_PingWidth) + 72));

		for(int Group = 0; Group < (Teams ? 2 : 1); ++Group)
		{
			const int Team = Group == 0 ? TEAM_RED : TEAM_BLUE;
			const int Count = aTeamSize[Team];
			const int Columns = Group ? Layout.m_BlueColumns : Layout.m_RedColumns;
			for(int Column = 0; Column < Columns; ++Column)
			{
				const int Index = (Group ? Layout.m_RedColumns : 0) + Column;
				if(!aVisible[Index])
					continue;
				const auto &Bounds = aBounds[Index];
				CUIRect Body = {Bounds.x, Bounds.y, Bounds.w, Bounds.h};
				if(Teams)
				{
					CUIRect Title;
					Body.HSplitTop(LucentScoreboard::TeamTitleHeight, &Title, &Body);
					CUIRect Accent = {Title.x + 10, Title.y + Title.h - 1, Title.w - 20, .7f};
					Accent.Draw((Team == TEAM_RED ? TEAM_RED_TITLE_COLOR : TEAM_BLUE_TITLE_COLOR).WithAlpha(.55f), 0, 0);
					const char *pTitle = Team == TEAM_RED ? (pRedTeamName ? pRedTeamName : Localize("Red team")) : (pBlueTeamName ? pBlueTeamName : Localize("Blue team"));
					RenderTitleBar(Title, Team, pTitle);
				}
				const auto Range = Layout.m_Ranges[Index];
				RenderScoreboard(Body, Team, Range.m_Start, Range.m_End, Layout.m_Scores[Index]);
			}
		}
		if(SpectatorCount > 0)
		{
			const float Width = m_SpectatorWidth.Update(Layout.m_SpectatorWidth, Now);
			CUIRect Spectators = {(Screen.w - Width) / 2, Bottom, Width, SpectatorHeight};
			LucentUi::COpacityScope SpectatorOpacity(m_Component.TextRender(), SpectatorReveal);
			RenderSpectators(Spectators);
			Bottom += LucentScoreboard::SpectatorSpace(Layout, SpectatorCount);
		}

		if(pGameInfoObj && (pGameInfoObj->m_ScoreLimit || pGameInfoObj->m_TimeLimit || (pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)))
		{
			RenderGoals({Scoreboard.x, Bottom, Scoreboard.w, 20});
			Bottom += 23;
		}
		if(!m_Component.m_MouseUnlocked)
			RenderMouseHint({Scoreboard.x, Bottom, Scoreboard.w, 26});
		m_Component.Ui()->MapScreen();

		RenderRecordingNotification((Screen.w / 7) * 4 + 10);

		if(!m_Component.GameClient()->m_Menus.IsActive() && !m_Component.GameClient()->m_Chat.IsActive())
		{
			m_Component.Ui()->RenderPopupMenus();

			if(m_Component.m_MouseUnlocked)
				m_Component.RenderTools()->RenderCursor(m_Component.Ui()->MousePos(), 24.0f);

			m_Component.Ui()->FinishCheck();
		}
	}

}
