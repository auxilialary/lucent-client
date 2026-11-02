/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "scoreboard.h"

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
#include <game/client/components/statboard.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/hud/visuals.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui.h>
#include <game/localization.h>

// Horizontal spacing of the scoreboard contents, both to its edges and between columns
static constexpr float MARGIN = 10.0f;
static constexpr const char *SCOREBOARD_CURSOR_BIND_NAME = "toggle_scoreboard_cursor";
static constexpr float ROUNDING = 7.5f;
static const ColorRGBA BACKGROUND_COLOR = LucentStyle::Style::Surface.WithAlpha(.88f);

static constexpr float POPUP_WIDTH = 110.0f;

static constexpr ColorRGBA TEAM_RED_TITLE_COLOR(0.975f, 0.17f, 0.17f);
static constexpr ColorRGBA TEAM_BLUE_TITLE_COLOR(0.17f, 0.46f, 0.975f);

static constexpr const char *TIME_WIDTH_TEXT = "00:00:00";

CScoreboard::CScoreboard()
{
	CScoreboard::OnReset();
}

void CScoreboard::SetUiMousePos(vec2 Pos)
{
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pScreen = Ui()->Screen();

	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
	Pos = Pos / vec2(pScreen->w, pScreen->h) * WindowSize;
	Ui()->OnCursorMove(Pos.x - UpdatedMousePos.x, Pos.y - UpdatedMousePos.y);
}

void CScoreboard::LockMouse()
{
	Ui()->ClosePopupMenus();
	m_MouseUnlocked = false;
	SetUiMousePos(m_LastMousePos.value());
	m_LastMousePos = Ui()->MousePos();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = static_cast<CScoreboard *>(pUserData);

	pSelf->GameClient()->m_Spectator.OnRelease();
	pSelf->GameClient()->m_Emoticon.OnRelease();

	pSelf->m_Active = pResult->GetInteger(0) != 0;

	if(!pSelf->IsActive() && pSelf->m_MouseUnlocked)
	{
		pSelf->LockMouse();
	}
}

void CScoreboard::ConToggleScoreboardCursor(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = static_cast<CScoreboard *>(pUserData);

	if(!pSelf->IsActive() ||
		pSelf->GameClient()->m_Menus.IsActive() ||
		pSelf->GameClient()->m_Chat.IsActive() ||
		pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		return;
	}

	pSelf->m_MouseUnlocked = !pSelf->m_MouseUnlocked;

	if(!pSelf->m_MouseUnlocked)
	{
		pSelf->Ui()->ClosePopupMenus();
	}

	vec2 OldMousePos = pSelf->Ui()->MousePos();

	if(pSelf->m_LastMousePos == std::nullopt)
	{
		pSelf->SetUiMousePos(pSelf->Ui()->Screen()->Center());
	}
	else
	{
		pSelf->SetUiMousePos(pSelf->m_LastMousePos.value());
	}

	// save pos, so moving the mouse in esc menu doesn't change the position
	pSelf->m_LastMousePos = OldMousePos;
}

void CScoreboard::OnConsoleInit()
{
	Console()->Register("+scoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
	Console()->Register(SCOREBOARD_CURSOR_BIND_NAME, "", CFGFLAG_CLIENT, ConToggleScoreboardCursor, this, "Toggle scoreboard cursor");
}

void CScoreboard::OnInit()
{
	m_DeadTeeTexture = Graphics()->LoadTexture("deadtee.png", IStorage::TYPE_ALL);
}

void CScoreboard::OnReset()
{
	m_Active = false;
	m_PointsService.Reset();
	m_NextPointsRead = 0;
	m_Visibility = {};
	m_SpectatorVisibility = {};
	m_Planner = {};
	m_HeaderWidth = {};
	for(auto &Column : m_aColumnMotion)
		Column = {};
	m_Top = {};
	m_SpectatorExtent = {};
	for(auto &Player : m_aPlayers)
	{
		Player.m_PointsValue = -1;
		Player.m_aPointsName[0] = 0;
		Player.m_aMeasuredName[0] = 0;
		Player.m_aMeasuredClan[0] = 0;
	}
	m_WasVisible = false;
	m_MouseUnlocked = false;
	m_LastMousePos = std::nullopt;
}

void CScoreboard::ResetTexts()
{
	for(auto &Label : m_aDDTeamLabels)
		Label.m_aText[0] = 0;
	for(CPlayerElement &Player : m_aPlayers)
	{
		Player.m_aMeasuredName[0] = Player.m_aMeasuredClan[0] = 0;
		Player.m_Score.Reset(TextRender());
		Player.m_ScoreMillis.Reset(TextRender());
		Player.m_Name.Reset(TextRender());
		Player.m_ReadyMark.Reset(TextRender());
		Player.m_Clan.Reset(TextRender());
		Player.m_Ping.Reset(TextRender());
		Player.m_Points.Reset(TextRender());
		Player.m_DDTeam.Reset(TextRender());
	}
	m_HeaderTimer.Reset(TextRender());
	m_HeaderMapMeasure.Reset(TextRender());
	m_TitleScore.Reset(TextRender());
	m_TitleScoreMillis.Reset(TextRender());
	m_HeadlineScore.Reset(TextRender());
	m_HeadlineName.Reset(TextRender());
	m_HeadlineClan.Reset(TextRender());
	m_HeadlinePing.Reset(TextRender());
	m_HeadlinePoints.Reset(TextRender());
}

void CScoreboard::OnStateChange(int NewState, int OldState)
{
	m_PointsService.Reset();
	for(auto &Player : m_aPlayers)
	{
		Player.m_PointsValue = -1;
		Player.m_aPointsName[0] = 0;
	}
	m_NextPointsRead = 0;
}

void CScoreboard::OnShutdown()
{
	m_PointsService.Reset();
	ResetTexts();
}

void CScoreboard::OnWindowResize()
{
	ResetTexts();
}

void CScoreboard::OnRelease()
{
	m_Active = false;

	if(m_MouseUnlocked)
	{
		LockMouse();
	}
}

bool CScoreboard::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!IsActive() || !m_MouseUnlocked)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);

	return true;
}

bool CScoreboard::OnInput(const IInput::CEvent &Event)
{
	if(m_MouseUnlocked && Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		LockMouse();
		return true;
	}

	return IsActive() && m_MouseUnlocked;
}

void CScoreboard::RenderTitle(CUIRect TitleLabel, int Team, const char *pTitle, float TitleFontSize, bool MapTitle)
{
	const bool IsMapTitle = MapTitle;
	if(MapTitle && m_MouseUnlocked)
		Ui()->DoToolTip(&m_MapTitleButtonId, &TitleLabel, pTitle);
	if(IsMapTitle && m_MouseUnlocked && GameClient()->m_aMapDescription[0] != '\0')
	{
		const int ButtonResult = Ui()->DoButtonLogic(&m_MapTitleButtonId, 0, &TitleLabel, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
		if(ButtonResult != 0)
		{
			m_MapTitlePopupContext.m_pScoreboard = this;

			m_MapTitlePopupContext.m_FontSize = 12.0f;
			const float MaxWidth = 300.0f;
			const float Margin = 5.0f;
			const char *pDescription = GameClient()->m_aMapDescription;
			const float TextWidth = std::min(std::ceil(TextRender()->TextWidth(m_MapTitlePopupContext.m_FontSize, pDescription) + 0.5f), MaxWidth);
			float TextHeight = 0.0f;
			STextSizeProperties TextSizeProps{};
			TextSizeProps.m_pHeight = &TextHeight;
			TextRender()->TextWidth(m_MapTitlePopupContext.m_FontSize, pDescription, -1, TextWidth, 0, TextSizeProps);

			Ui()->DoPopupMenu(&m_MapTitlePopupContext, Ui()->MouseX(), Ui()->MouseY(), TextWidth + Margin * 2, TextHeight + Margin * 2, &m_MapTitlePopupContext, CMapTitlePopupContext::Render);
		}
		if(Ui()->HotItem() == &m_MapTitleButtonId)
		{
			TitleLabel.Draw(ColorRGBA(0.7f, 0.7f, 0.7f, 0.3f), IGraphics::CORNER_ALL, 5.0f);
		}
	}

	SLabelProperties Props;
	Props.m_MaxWidth = TitleLabel.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&TitleLabel, pTitle, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_ML : TEXTALIGN_MR, Props);
}

void CScoreboard::RenderTitleScore(CUIRect ScoreLabel, int Team, float TitleFontSize)
{
	// map best
	char aScore[128] = "";
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const bool TimeScore = GameClient()->m_GameInfo.m_TimeScore;
	const bool Race7 = Client()->IsSixup() && pGameInfoObj && pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE;
	if(GameClient()->m_ReceivedDDNetPlayerFinishTimes || TimeScore || Race7)
	{
		if(GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET)
		{
			Ui()->RenderTime(ScoreLabel,
				TitleFontSize,
				GameClient()->m_MapBestTimeSeconds,
				GameClient()->m_MapBestTimeSeconds == FinishTime::NOT_FINISHED_MILLIS,
				GameClient()->m_MapBestTimeMillis,
				GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis,
				m_TitleScore, m_TitleScoreMillis, TextRender()->DefaultTextColor());
			return;
		}
	}
	else if(GameClient()->IsTeamPlay()) // normal score
	{
		const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
		if(pGameDataObj)
		{
			str_format(aScore, sizeof(aScore), "%d", Team == TEAM_RED ? pGameDataObj->m_TeamscoreRed : pGameDataObj->m_TeamscoreBlue);
		}
	}
	else
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active &&
			GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW &&
			GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId])
		{
			str_format(aScore, sizeof(aScore), "%d", GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId]->m_Score);
		}
		else if(GameClient()->m_Snap.m_pLocalInfo)
		{
			str_format(aScore, sizeof(aScore), "%d", GameClient()->m_Snap.m_pLocalInfo->m_Score);
		}
	}

	const float ScoreTextWidth = aScore[0] != '\0' ? TextRender()->TextWidth(TitleFontSize, aScore, -1, -1.0f, 0) : 0.0f;
	if(ScoreTextWidth != 0.0f)
	{
		Ui()->DoLabel(&ScoreLabel, aScore, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_MR : TEXTALIGN_ML);
	}
}

void CScoreboard::RenderHeader(float CenterX, float Top, float ViewportWidth)
{
	const auto *pInfo = GameClient()->m_Snap.m_pGameInfoObj;
	char aTimer[64];
	const int Best = GameClient()->m_MapBestTimeSeconds;
	const bool Race = GameClient()->m_GameInfo.m_TimeScore || GameClient()->m_ReceivedDDNetPlayerFinishTimes ||
			  (Client()->IsSixup() && pInfo && (pInfo->m_GameFlags & protocol7::GAMEFLAG_RACE));
	if(Race && Best >= 0)
	{
		str_time(int64_t(Best) * 100, ETimeFormat::HOURS, aTimer, sizeof(aTimer));
		if(Best < 3600 && GameClient()->m_MapBestTimeMillis >= 0)
		{
			char aFraction[8];
			if(GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis)
				str_format(aFraction, sizeof(aFraction), ".%03d", GameClient()->m_MapBestTimeMillis % 1000);
			else
				str_format(aFraction, sizeof(aFraction), ".%02d", (int)std::round((GameClient()->m_MapBestTimeMillis % 1000) / 10.f));
			str_append(aTimer, aFraction);
		}
	}
	else
	{
		const int Seconds = pInfo ? LucentScoreboard::RoundTime(Client()->GameTick(g_Config.m_ClDummy), Client()->GameTickSpeed(),
						    pInfo->m_RoundStartTick, pInfo->m_TimeLimit, pInfo->m_WarmupTimer,
						    pInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME, pInfo->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) :
					    0;
		str_time(int64_t(std::max(0, Seconds)) * 100, ETimeFormat::HOURS, aTimer, sizeof(aTimer));
	}
	constexpr float MapFont = 16, TimerFont = 13;
	m_HeaderTimer.Update(TextRender(), aTimer, TimerFont);
	const char *pMap = GameClient()->Map()->BaseName();
	m_HeaderMapMeasure.Update(TextRender(), pMap, MapFont);
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
	m_HeaderTimer.Render(TextRender(), Pos, LucentStyle::Style::Accent);
}

void CScoreboard::RenderTitleBar(CUIRect TitleBar, int Team, const char *pTitle)
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

	const float TitleFontSize = 14.0f;
	const float ScoreTextWidth = TextRender()->TextWidth(TitleFontSize, TIME_WIDTH_TEXT);
	const float TitleTextWidth = TextRender()->TextWidth(TitleFontSize, pTitle);

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

void CScoreboard::RenderGoals(CUIRect Goals)
{
	Goals.Draw(BACKGROUND_COLOR, IGraphics::CORNER_ALL, ROUNDING);
	Goals.VMargin(5.0f, &Goals);

	const float FontSize = 10.0f;
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	char aBuf[64];

	if(pGameInfoObj->m_ScoreLimit)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), pGameInfoObj->m_ScoreLimit);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_ML);
	}

	if(pGameInfoObj->m_TimeLimit)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), pGameInfoObj->m_TimeLimit);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MC);
	}

	if(pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Round %d/%d"), pGameInfoObj->m_RoundCurrent, pGameInfoObj->m_RoundNum);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MR);
	}
}

void CScoreboard::DrawFloatingPanel(const CUIRect &Rect, int Index, float Opacity)
{
	const auto &Geometry = m_aPanels[Index].Get({Rect.x, Rect.y, Rect.w, Rect.h}, LucentHud::PixelGrid(Graphics()));
	LucentHud::DrawSoftFill(Graphics(), Geometry, LucentHud::PanelSurface(Opacity, true), LucentHud::PanelSurface(Opacity, false));
	LucentHud::DrawSoftBorder(Graphics(), Geometry, Opacity, false);
}

void CScoreboard::RenderSpectators(CUIRect Spectators)
{
	DrawFloatingPanel(Spectators, 1, m_Opacity * m_SpectatorVisibility.m_Value);
	CUIRect Title = {Spectators.x + 10, Spectators.y + 5, Spectators.w - 20, 16};
	Ui()->DoLabel(&Title, Localize("Spectators"), 11, TEXTALIGN_ML);
	const float CellWidth = (Spectators.w - 20) / m_SpectatorColumns;
	int Entry = 0;
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;
		CUIRect Cell = {Spectators.x + 10 + (Entry % m_SpectatorColumns) * CellWidth,
			Spectators.y + 24 + (Entry / m_SpectatorColumns) * m_SpectatorRow, CellWidth - 6, m_SpectatorRow};
		++Entry;
		const auto &Data = GameClient()->m_aClients[pInfo->m_ClientId];
		auto &Player = m_aPlayers[pInfo->m_ClientId];
		char aEntry[128] = "";
		if(g_Config.m_ClShowIds)
		{
			char aId[16];
			GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::NO_INDENT);
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
			GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::NO_INDENT);
			str_copy(aName, aId);
		}
		str_append(aName, Data.m_aName);
		str_copy(aClan, Data.m_aClan);
		LucentScoreboard::SingleLine(aName);
		LucentScoreboard::SingleLine(aClan);
		Player.m_Name.Update(TextRender(), aName, Font, NameWidth, LucentScoreboard::SingleLineTextFlags);
		Player.m_Clan.Update(TextRender(), aClan, Font, ClanWidth, LucentScoreboard::SingleLineTextFlags);

		if(m_MouseUnlocked)
		{
			const int ButtonResult = Ui()->DoButtonLogic(&Player.m_PlayerButtonId, 0, &Cell, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
			if(ButtonResult != 0)
			{
				m_ScoreboardPopupContext.m_pScoreboard = this;
				m_ScoreboardPopupContext.m_ClientId = pInfo->m_ClientId;
				m_ScoreboardPopupContext.m_IsLocal = GameClient()->m_aLocalIds[0] == pInfo->m_ClientId ||
								     (Client()->DummyConnected() && GameClient()->m_aLocalIds[1] == pInfo->m_ClientId);
				m_ScoreboardPopupContext.m_IsSpectating = true;

				Ui()->DoPopupMenu(&m_ScoreboardPopupContext, Ui()->MouseX(), Ui()->MouseY(), POPUP_WIDTH,
					m_ScoreboardPopupContext.m_IsLocal ? 30.0f : 60.0f, &m_ScoreboardPopupContext, CScoreboardPopupContext::Render);
			}

			if(Ui()->HotItem() == &m_aPlayers[pInfo->m_ClientId].m_PlayerButtonId ||
				(Ui()->IsPopupOpen(&m_ScoreboardPopupContext) && m_ScoreboardPopupContext.m_ClientId == pInfo->m_ClientId))
			{
				Cell.Draw(LucentStyle::Style::Accent.WithAlpha(LucentStyle::Style::Selected), IGraphics::CORNER_ALL, 2.f);
			}
		}

		if(m_MouseUnlocked)
		{
			str_copy(Player.m_aSpectatorTooltip, aEntry);
			Ui()->DoToolTip(&Player.m_Name, &Cell, Player.m_aSpectatorTooltip);
		}
		const float TextY = Cell.y + (Cell.h - Font) / 2;
		const auto NameColor = Data.m_AuthLevel ? color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor)) : TextRender()->DefaultTextColor();
		const int Local = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
		const auto ClanColor = Local >= 0 && str_comp(Data.m_aClan, GameClient()->m_aClients[Local].m_aClan) == 0 ? color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor)) : LucentStyle::Style::SecondaryText;
		Player.m_Name.Render(TextRender(), {Cell.x, TextY}, NameColor);
		Player.m_Clan.Render(TextRender(), {Cell.x + NameWidth + 3, TextY}, ClanColor);
	}
}

void CScoreboard::RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd, float TeamWidth)
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");
	if(CountEnd <= CountStart)
		return;

	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
	const bool TimeScore = GameClient()->m_GameInfo.m_TimeScore;
	const bool MillisecondScore = GameClient()->m_ReceivedDDNetPlayerFinishTimes;
	const bool TrueMilliseconds = GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis;

	bool Race7 = Client()->IsSixup() && pGameInfoObj && pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE;

	const bool UseTime = Race7 || TimeScore || MillisecondScore;

	const float LineHeight = m_RowHeight;
	const float FontSize = m_FontSize;
	const float TeeSizeMod = LineHeight / 60.f;
	const float Spacing = 0;
	const float RoundRadius = std::min(4.f, LineHeight / 4);
	const auto Fields = LucentScoreboard::CFields::Calculate(Scoreboard.w, FontSize, m_ScoreWidth, m_PointsWidth, m_PingWidth, m_NameWidth, m_ClanWidth, TeamWidth);
	const float BaseScoreOffset = Scoreboard.x + Fields.m_Score, ScoreLength = Fields.m_ScoreWidth;
	const float BaseTeeOffset = Scoreboard.x + Fields.m_Tee, TeeLength = Fields.m_TeeWidth;
	const float BaseNameOffset = Scoreboard.x + Fields.m_Name, NameLength = std::max(.1f, Fields.m_NameWidth - FontSize);
	const float BaseClanOffset = Scoreboard.x + Fields.m_Clan, ClanLength = std::max(.1f, Fields.m_ClanWidth);
	const float BaseCountryOffset = Scoreboard.x + Fields.m_Country, CountryLength = Fields.m_CountryWidth;
	const float CountryFlagSize = std::max(0.f, LineHeight - CountryLength / 2);
	const float BasePointsOffset = Scoreboard.x + Fields.m_Points, PointsLength = Fields.m_PointsWidth;
	const float BasePingOffset = Scoreboard.x + Fields.m_Ping, PingLength = Fields.m_PingWidth;

	// render headlines
	const float HeadlineFontsize = std::min(10.f, FontSize);
	CUIRect Headline;
	Scoreboard.HSplitTop(22.0f, &Headline, &Scoreboard);
	const float HeadlineY = Headline.y + Headline.h / 2.0f - HeadlineFontsize / 2.0f;
	const ColorRGBA HeadlineColor = TextRender()->DefaultTextColor();
	m_HeadlineScore.Update(TextRender(), UseTime ? Localize("Time") : Localize("Score"), HeadlineFontsize);
	m_HeadlineName.Update(TextRender(), Localize("Name"), HeadlineFontsize);
	m_HeadlineClan.Update(TextRender(), Localize("Clan"), HeadlineFontsize);
	m_HeadlinePing.Update(TextRender(), Localize("Ping"), HeadlineFontsize);
	m_HeadlinePoints.Update(TextRender(), "Pts", HeadlineFontsize);
	m_HeadlineScore.Render(TextRender(), vec2(BaseScoreOffset + ScoreLength - m_HeadlineScore.Width(), HeadlineY), HeadlineColor);
	m_HeadlineName.Render(TextRender(), vec2(BaseNameOffset, HeadlineY), HeadlineColor);
	m_HeadlineClan.Render(TextRender(), vec2(BaseClanOffset + (ClanLength - m_HeadlineClan.Width()) / 2.0f, HeadlineY), HeadlineColor);
	m_HeadlinePoints.Render(TextRender(), vec2(BasePointsOffset + PointsLength - m_HeadlinePoints.Width(), HeadlineY), HeadlineColor);
	m_HeadlinePing.Render(TextRender(), vec2(BasePingOffset + PingLength - m_HeadlinePing.Width(), HeadlineY), HeadlineColor);

	// render player entries
	int CountRendered = 0;
	int PrevDDTeam = -1;

	char aBuf[64];

	for(int RenderDead = 0; RenderDead < 2; RenderDead++)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			// make sure that we render the correct team
			const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apInfoByDDTeamScore[i];
			if(!pInfo || pInfo->m_Team != Team)
				continue;
			bool IsDead = Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_DEAD;
			if(!RenderDead && IsDead)
				continue;
			if(RenderDead && !IsDead)
				continue;
			if(CountRendered++ < CountStart)
				continue;

			int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
			int NextDDTeam = 0;

			ColorRGBA TextColor = TextRender()->DefaultTextColor();
			TextColor.a = RenderDead ? 0.5f : 1.0f;
			TextRender()->TextColor(TextColor);

			for(int j = i + 1; j < MAX_CLIENTS; j++)
			{
				const CNetObj_PlayerInfo *pInfoNext = GameClient()->m_Snap.m_apInfoByDDTeamScore[j];
				if(!pInfoNext || pInfoNext->m_Team != Team)
					continue;

				NextDDTeam = GameClient()->m_Teams.Team(pInfoNext->m_ClientId);
				break;
			}

			if(PrevDDTeam == -1)
			{
				for(int j = i - 1; j >= 0; j--)
				{
					const CNetObj_PlayerInfo *pInfoPrev = GameClient()->m_Snap.m_apInfoByDDTeamScore[j];
					if(!pInfoPrev || pInfoPrev->m_Team != Team)
						continue;

					PrevDDTeam = GameClient()->m_Teams.Team(pInfoPrev->m_ClientId);
					break;
				}
			}

			CUIRect RowAndSpacing, Row;
			Scoreboard.HSplitTop(LineHeight + Spacing, &RowAndSpacing, &Scoreboard);
			RowAndSpacing.HSplitTop(LineHeight, &Row, nullptr);

			const float DeltaX = 0;
			Row.x += DeltaX;
			Row.y += 2.f * (1.f - LucentStyle::Motion::CubicOut(float((m_Now - m_RevealStart) / .08)));
			RowAndSpacing.y = Row.y;
			const float ScoreOffset = BaseScoreOffset + DeltaX;
			const float TeeOffset = BaseTeeOffset + DeltaX;
			const float NameOffset = BaseNameOffset + DeltaX;
			const float PointsOffset = BasePointsOffset + DeltaX;
			const float PingOffset = BasePingOffset + DeltaX;
			const float CountryOffset = BaseCountryOffset + DeltaX;
			const float ClanOffset = BaseClanOffset + DeltaX;
			const float TextY = Row.y + (Row.h - FontSize) / 2.0f;

			// team background
			if(DDTeam != TEAM_FLOCK)
			{
				const ColorRGBA Color = GameClient()->GetDDTeamColor(DDTeam).WithAlpha(0.5f);
				int TeamRectCorners = 0;
				if(PrevDDTeam != DDTeam)
				{
					TeamRectCorners |= IGraphics::CORNER_T;
				}
				if(NextDDTeam != DDTeam)
					TeamRectCorners |= IGraphics::CORNER_B;
				RowAndSpacing.Draw(Color, TeamRectCorners, RoundRadius);
			}
			PrevDDTeam = DDTeam;

			// background so it's easy to find the local player or the followed one in spectator mode
			if((!GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_Local) ||
				(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW && pInfo->m_Local) ||
				(GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId))
			{
				LucentHud::SoftFill(Graphics(), {Row.x, Row.y, Row.w, Row.h}, LucentStyle::Gradient::Blend(LucentStyle::Style::ServerSelection, LucentStyle::Style::Accent, LucentStyle::Style::SceneBase).WithAlpha(.42f * m_Opacity), .6f);
			}

			const CGameClient::CClientData &ClientData = GameClient()->m_aClients[pInfo->m_ClientId];
			CPlayerElement &Player = m_aPlayers[pInfo->m_ClientId];

			if(DDTeam > TEAM_FLOCK && DDTeam < int(m_aDDTeamLabels.size()))
			{
				const auto &Label = m_aDDTeamLabels[DDTeam];
				const float TeamFont = FontSize * .75f;
				Player.m_DDTeam.Update(TextRender(), Label.m_aText, TeamFont, std::max(.1f, (TeamWidth - 4) * FontSize / 12), LucentScoreboard::SingleLineTextFlags);
				const float X = Row.x + 7 * FontSize / 12;
				Player.m_DDTeam.Render(TextRender(), {X, Row.y + (Row.h - TeamFont) / 2}, TextColor);
				if(m_MouseUnlocked)
				{
					CUIRect Cell = {X, Row.y, TeamWidth * FontSize / 12, Row.h};
					Ui()->DoToolTip(&Player.m_DDTeam, &Cell, Label.m_aTooltip);
				}
			}

			if(m_MouseUnlocked)
			{
				const int ButtonResult = Ui()->DoButtonLogic(&Player.m_PlayerButtonId, 0, &Row, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
				if(ButtonResult != 0)
				{
					m_ScoreboardPopupContext.m_pScoreboard = this;
					m_ScoreboardPopupContext.m_ClientId = pInfo->m_ClientId;
					m_ScoreboardPopupContext.m_IsLocal = GameClient()->m_aLocalIds[0] == pInfo->m_ClientId ||
									     (Client()->DummyConnected() && GameClient()->m_aLocalIds[1] == pInfo->m_ClientId);
					m_ScoreboardPopupContext.m_IsSpectating = false;

					Ui()->DoPopupMenu(&m_ScoreboardPopupContext, Ui()->MouseX(), Ui()->MouseY(), POPUP_WIDTH,
						m_ScoreboardPopupContext.m_IsLocal ? 58.5f : 87.5f, &m_ScoreboardPopupContext, CScoreboardPopupContext::Render);
				}

				if(Ui()->HotItem() == &Player.m_PlayerButtonId ||
					Ui()->HotItem() == &Player.m_TimeTooltipId ||
					(Ui()->IsPopupOpen(&m_ScoreboardPopupContext) && m_ScoreboardPopupContext.m_ClientId == pInfo->m_ClientId))
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
				if(m_MouseUnlocked && pRect->Inside(Ui()->MousePos()))
				{
					Ui()->SetHotItem(&Player.m_TimeTooltipId);
					str_time(TimeInMillis / 10, ETimeFormat::HOURS_CENTISECS, Player.m_aTimeTooltip, sizeof(Player.m_aTimeTooltip));
					Ui()->DoToolTip({&Player.m_TimeTooltipId,
						*pRect,
						Player.m_aTimeTooltip,
						WidthHint * 1.5f,
						FontSize,
						true});
				}
			};
			constexpr int SecondsPerHour = 60 * 60;

			if(Race7)
			{
				Ui()->RenderTime(ScorePosition, FontSize, pInfo->m_Score / 1000, pInfo->m_Score == protocol7::FinishTime::NOT_FINISHED, pInfo->m_Score % 1000, true,
					Player.m_Score, Player.m_ScoreMillis, TextColor);
				if(pInfo->m_Score / 1000 >= SecondsPerHour)
				{
					DoMillisecondTooltip(&ScorePosition, time_milliseconds_from_seconds(pInfo->m_Score / 1000.0f), Player.m_Score.Width());
				}
			}
			else if(MillisecondScore)
			{
				Ui()->RenderTime(ScorePosition, FontSize, ClientData.m_FinishTimeSeconds, ClientData.m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS, ClientData.m_FinishTimeMillis, TrueMilliseconds,
					Player.m_Score, Player.m_ScoreMillis, TextColor);
				if(ClientData.m_FinishTimeSeconds >= SecondsPerHour)
				{
					DoMillisecondTooltip(&ScorePosition, static_cast<int64_t>(ClientData.m_FinishTimeSeconds) * 1000 + ClientData.m_FinishTimeMillis, Player.m_Score.Width());
				}
			}
			else if(TimeScore)
			{
				Ui()->RenderTime(ScorePosition, FontSize, pInfo->m_Score, pInfo->m_Score == FinishTime::NOT_FINISHED_TIMESCORE, -1, false,
					Player.m_Score, Player.m_ScoreMillis, TextColor);
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "%d", std::clamp(pInfo->m_Score, -999, 99999));
				Player.m_Score.Update(TextRender(), aBuf, FontSize);
				Player.m_Score.Render(TextRender(), vec2(ScoreOffset + ScoreLength - Player.m_Score.Width(), TextY), TextColor);
			}

			// CTF flag
			if(pGameInfoObj && (pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) &&
				pGameDataObj && (pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientId || pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId))
			{
				Graphics()->TextureSet(pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId ? GameClient()->m_GameSkin.m_SpriteFlagBlue : GameClient()->m_GameSkin.m_SpriteFlagRed);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1, 1, 1, m_Opacity);
				Graphics()->QuadsSetSubset(1.0f, 0.0f, 0.0f, 1.0f);
				IGraphics::CQuadItem QuadItem(TeeOffset, Row.y - 2.5f - Spacing / 2.0f, Row.h / 2.0f, Row.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}

			// skin
			if(RenderDead)
			{
				Graphics()->TextureSet(m_DeadTeeTexture);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1, 1, 1, m_Opacity);
				if(GameClient()->IsTeamPlay())
				{
					Graphics()->SetColor(GameClient()->m_Skins7.GetTeamColor(true, 0, GameClient()->m_aClients[pInfo->m_ClientId].m_Team, protocol7::SKINPART_BODY).WithAlpha(m_Opacity));
				}
				CTeeRenderInfo TeeInfo = GameClient()->m_aClients[pInfo->m_ClientId].m_RenderInfo;
				TeeInfo.m_Size *= TeeSizeMod;

				IGraphics::CQuadItem QuadItem(TeeOffset, Row.y, TeeInfo.m_Size, TeeInfo.m_Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
			else
			{
				CTeeRenderInfo TeeInfo = ClientData.m_RenderInfo;
				TeeInfo.m_Size *= TeeSizeMod;

				RenderTools()->RenderIdleTeeAtCenter(&TeeInfo, vec2(TeeOffset + TeeLength / 2, Row.y + Row.h / 2.0f), EMOTE_NORMAL, vec2(1, 0), m_Opacity);

				if(m_MouseUnlocked)
				{
					const CUIRect SkinRect = {TeeOffset, Row.y, TeeLength, Row.h};
					Ui()->DoToolTip(&m_aPlayers[pInfo->m_ClientId].m_PlayerButtonId, &SkinRect, ClientData.m_aSkinName);
				}
			}

			// name
			{
				if(g_Config.m_ClShowIds)
				{
					char aClientId[16];
					GameClient()->FormatClientId(pInfo->m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
					str_copy(aBuf, aClientId);
					str_append(aBuf, ClientData.m_aName);
				}
				else
				{
					str_copy(aBuf, ClientData.m_aName);
				}
				LucentScoreboard::SingleLine(aBuf);
				Player.m_Name.Update(TextRender(), aBuf, FontSize, NameLength, LucentScoreboard::SingleLineTextFlags);
				if(m_MouseUnlocked)
				{
					CUIRect Cell = {NameOffset, Row.y, NameLength, Row.h};
					Ui()->DoToolTip(&Player.m_Name, &Cell, ClientData.m_aName);
				}

				ColorRGBA NameColor = TextColor;
				if(ClientData.m_AuthLevel)
				{
					NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
				}
				Player.m_Name.Render(TextRender(), vec2(NameOffset, TextY), NameColor);

				// ready / watching
				if(Client()->IsSixup() && Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_READY)
				{
					Player.m_ReadyMark.Update(TextRender(), "✓", FontSize);
					Player.m_ReadyMark.Render(TextRender(), vec2(NameOffset + Player.m_Name.Width(), TextY), ColorRGBA(0.1f, 1.0f, 0.1f, TextColor.a));
				}
			}

			// clan
			{
				ColorRGBA ClanColor = TextColor;
				if(GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0 && str_comp(ClientData.m_aClan, GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_aClan) == 0)
				{
					ClanColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor));
				}
				char aClan[64];
				str_copy(aClan, ClientData.m_aClan);
				LucentScoreboard::SingleLine(aClan);
				Player.m_Clan.Update(TextRender(), aClan, FontSize, ClanLength, LucentScoreboard::SingleLineTextFlags);
				if(m_MouseUnlocked)
				{
					CUIRect Cell = {ClanOffset, Row.y, ClanLength, Row.h};
					Ui()->DoToolTip(&Player.m_Clan, &Cell, ClientData.m_aClan);
				}
				Player.m_Clan.Render(TextRender(), vec2(ClanOffset + (ClanLength - std::min(Player.m_Clan.Width(), ClanLength)) / 2.0f, TextY), ClanColor);
			}

			// country flag
			GameClient()->m_CountryFlags.Render(ClientData.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * m_Opacity),
				CountryOffset, Row.y + (Spacing + CountryFlagSize) / 2.0f, CountryLength, Row.h - Spacing - CountryFlagSize);

			// Public DDNet profile points, independent of map score/time and slot identity.
			if(Player.m_PointsValue >= 0)
				str_format(aBuf, sizeof(aBuf), "%d", Player.m_PointsValue);
			else
				str_copy(aBuf, "—");
			Player.m_Points.Update(TextRender(), aBuf, FontSize);
			Player.m_Points.Render(TextRender(), vec2(PointsOffset + PointsLength - Player.m_Points.Width(), TextY), TextColor);
			if(m_MouseUnlocked)
			{
				CUIRect Cell = {PointsOffset, Row.y, PointsLength, Row.h};
				Ui()->DoToolTip(&Player.m_Points, &Cell, "DDNet profile points for this nickname; not verified identity");
			}
			// ping
			ColorRGBA PingColor = g_Config.m_ClEnablePingColor ? CRenderTools::GetPingTextColor(pInfo->m_Latency) : TextRender()->DefaultTextColor();
			str_format(aBuf, sizeof(aBuf), "%d", std::clamp(pInfo->m_Latency, 0, 999));
			Player.m_Ping.Update(TextRender(), aBuf, FontSize);
			Player.m_Ping.Render(TextRender(), vec2(PingOffset + PingLength - Player.m_Ping.Width(), TextY), PingColor);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			if(CountRendered == CountEnd)
				break;
		}
		if(CountRendered == CountEnd)
			break;
	}
}

void CScoreboard::RenderMouseHint(CUIRect MouseHint)
{
	constexpr float FontSize = 11.0f;
	constexpr float PillPaddingX = 8.0f;
	constexpr float PillPaddingY = 5.0f;
	constexpr float TextKeycapGap = 5.0f;
	constexpr float KeycapPaddingX = 4.0f;
	constexpr float KeycapPaddingY = 2.0f;
	constexpr float KeycapRadius = 3.5f;

	char aKey[64];
	GameClient()->m_Binds.GetKey(SCOREBOARD_CURSOR_BIND_NAME, aKey, sizeof(aKey));

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
	const char *pChipText = HasKeycap ? aKey : SCOREBOARD_CURSOR_BIND_NAME;
	const float TextWidth = TextRender()->TextWidth(FontSize, aHint, -1);
	const float ChipWidth = TextRender()->TextWidth(FontSize, pChipText, -1) + 2 * KeycapPaddingX;
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

	Ui()->DoLabel(&Label, aHint, FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&Chip, pChipText, FontSize, TEXTALIGN_MC);
}

void CScoreboard::RenderRecordingNotification(float x)
{
	char aBuf[512] = "";

	const auto &&AppendRecorderInfo = [&](int Recorder, const char *pName) {
		if(GameClient()->DemoRecorder(Recorder)->IsRecording())
		{
			char aTime[32];
			str_time((int64_t)GameClient()->DemoRecorder(Recorder)->Length() * 100, ETimeFormat::HOURS, aTime, sizeof(aTime));
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

	CUIRect Rect = {x, 0.0f, TextRender()->TextWidth(FontSize, aBuf) + 30.0f, 25.0f};
	Rect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, ROUNDING);
	Rect.VSplitLeft(CircleSize, nullptr, &Rect);
	Rect.VSplitRight(Gap, &Rect, nullptr);

	CUIRect Circle;
	Rect.VSplitLeft(CircleSize, &Circle, &Rect);
	Circle.HMargin((Circle.h - Circle.w) / 2.0f, &Circle);
	Circle.Draw(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), IGraphics::CORNER_ALL, Circle.h / 2.0f);

	Rect.VSplitLeft(Gap, nullptr, &Rect);
	Ui()->DoLabel(&Rect, aBuf, FontSize, TEXTALIGN_ML);
}

void CScoreboard::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const bool Visible = IsActive();
	const double Now = time_get() / double(time_freq());
	m_Now = Now;
	m_Opacity = m_Visibility.Update(Visible, Now, true, Visible ? .17f : .11f);
	if(Visible && !m_WasVisible)
		m_RevealStart = Now;
	m_WasVisible = Visible;
	if(!Visible && m_MouseUnlocked)
		LockMouse();
	if(m_Opacity <= 0.f)
		return;
	LucentUi::CScope StyleScope(Ui());
	LucentUi::COpacityScope OpacityScope(TextRender(), m_Opacity);
	LucentUi::CInteractionScope InteractionScope(Ui(), Visible);

	if(!GameClient()->m_Menus.IsActive() && !GameClient()->m_Chat.IsActive())
	{
		Ui()->StartCheck();
		Ui()->Update();
	}

	if(GameClient()->m_Motd.IsActive())
		GameClient()->m_Motd.Clear();
	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const bool Teams = GameClient()->IsTeamPlay();
	const auto &aTeamSize = GameClient()->m_Snap.m_aTeamSize;
	std::array<const char *, 128> aPointsNames{};
	for(const auto *pInfo : GameClient()->m_Snap.m_apInfoByName)
		if(pInfo && pInfo->m_Team != TEAM_SPECTATORS)
			aPointsNames[pInfo->m_ClientId] = GameClient()->m_aClients[pInfo->m_ClientId].m_aName;
	if(Visible && Client()->State() == IClient::STATE_ONLINE)
		m_PointsService.Update(aPointsNames, Now, Http(), Engine());
	const bool ReadPoints = Now >= m_NextPointsRead;
	if(ReadPoints)
		m_NextPointsRead = Now + .25;
	const bool UsesTime = GameClient()->m_GameInfo.m_TimeScore || GameClient()->m_ReceivedDDNetPlayerFinishTimes ||
			      (Client()->IsSixup() && pGameInfoObj && (pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE));
	m_ScoreWidth = TextRender()->TextWidth(12, UsesTime ? "000000:00:00" : "-99999") + 4;
	m_PointsWidth = std::max(TextRender()->TextWidth(12, "999999"), TextRender()->TextWidth(12, "Pts")) + 4;
	m_PingWidth = std::max(TextRender()->TextWidth(12, "999"), TextRender()->TextWidth(12, Localize("Ping"))) + 2;
	int SpectatorCount = 0;
	std::array<int, MAX_CLIENTS + 1> aDDTeamCounts{};
	float ContentWidth = 400;
	m_NameWidth = m_ClanWidth = 0;
	for(const auto *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo)
			continue;
		if(pInfo->m_Team == TEAM_SPECTATORS)
		{
			++SpectatorCount;
			continue;
		}
		const auto &Data = GameClient()->m_aClients[pInfo->m_ClientId];
		auto &Player = m_aPlayers[pInfo->m_ClientId];
		const int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
		if(DDTeam > TEAM_FLOCK && DDTeam < int(aDDTeamCounts.size()))
			++aDDTeamCounts[DDTeam];
		if(ReadPoints || str_comp(Player.m_aPointsName, Data.m_aName))
		{
			str_copy(Player.m_aPointsName, Data.m_aName);
			Player.m_PointsValue = m_PointsService.Lookup(Data.m_aName, Now);
		}
		if(Player.m_PointsValue >= 1000000)
		{
			char aPts[16];
			str_format(aPts, sizeof(aPts), "%d", Player.m_PointsValue);
			m_PointsWidth = std::max(m_PointsWidth, TextRender()->TextWidth(12, aPts) + 4);
		}
		char aName[128];
		if(g_Config.m_ClShowIds)
		{
			char aId[16];
			GameClient()->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::INDENT_AUTO);
			str_format(aName, sizeof(aName), "%s%s", aId, Data.m_aName);
		}
		else
			str_copy(aName, Data.m_aName);
		if(str_comp(Player.m_aMeasuredName, aName) != 0)
		{
			str_copy(Player.m_aMeasuredName, aName);
			Player.m_NameWidth = TextRender()->TextWidth(12, aName);
		}
		if(str_comp(Player.m_aMeasuredClan, Data.m_aClan) != 0)
		{
			str_copy(Player.m_aMeasuredClan, Data.m_aClan);
			Player.m_ClanWidth = TextRender()->TextWidth(12, Data.m_aClan);
		}
		m_NameWidth = std::max(m_NameWidth, Player.m_NameWidth);
		m_ClanWidth = std::max(m_ClanWidth, Player.m_ClanWidth);
	}
	m_DDTeamWidth = 0;
	for(int Team = 1; Team < int(aDDTeamCounts.size()); ++Team)
	{
		const int Count = aDDTeamCounts[Team];
		if(!Count)
			continue;
		auto &Label = m_aDDTeamLabels[Team];
		char aText[64];
		if(Team == GameClient()->m_Teams.TeamSuper())
		{
			str_copy(aText, Localize("Super"));
			str_copy(Label.m_aTooltip, Localize("Super"));
		}
		else
		{
			if(Count > 1)
				str_format(aText, sizeof(aText), "%d %d/%d", Team, Count, GameClient()->MaxTeamSize());
			else
				str_format(aText, sizeof(aText), "%d", Team);
			str_format(Label.m_aTooltip, sizeof(Label.m_aTooltip), Localize("Team %d (%d/%d)"), Team, Count, GameClient()->MaxTeamSize());
		}
		if(str_comp(Label.m_aText, aText))
		{
			str_copy(Label.m_aText, aText);
			Label.m_Width = TextRender()->TextWidth(9, aText) + 4;
		}
		m_DDTeamWidth = std::max(m_DDTeamWidth, Label.m_Width);
	}
	ContentWidth = std::max(ContentWidth, LucentScoreboard::CFields::PreferredWidth(m_ScoreWidth, m_PointsWidth, m_PingWidth, m_NameWidth, m_ClanWidth, m_DDTeamWidth));
	const auto &Layout = m_Planner.Update({Screen.w, Screen.h, aTeamSize[TEAM_RED], aTeamSize[TEAM_BLUE], Teams, SpectatorCount, ContentWidth, LucentScoreboard::CFields::MinimumWidth(m_ScoreWidth, m_PointsWidth, m_PingWidth, m_DDTeamWidth)});
	const float SpectatorHeight = SpectatorCount > 0 ? m_SpectatorExtent.Update(Layout.m_SpectatorHeight, Now) : 0.f;
	if(SpectatorCount == 0)
		m_SpectatorExtent = {};
	const float SpectatorReveal = m_SpectatorVisibility.Update(SpectatorCount > 0, Now, true, .16f);
	// Content always uses the complete target grid. Animated surfaces never clip new rows.
	m_RowHeight = Layout.m_Row;
	m_SpectatorColumns = Layout.m_SpectatorColumns;
	m_SpectatorRow = Layout.m_SpectatorRow;
	const float Scale = m_MouseUnlocked ? 1.f : LucentStyle::Motion::Lerp(.98f, 1.f, m_Opacity);
	// Map around the UI center, leaving mouse coordinates in the same space.
	const float ExtraX = Screen.w * (1 / Scale - 1) / 2;
	const float ExtraY = Screen.h * (1 / Scale - 1) / 2;
	Graphics()->MapScreen({{-ExtraX, -ExtraY}, {Screen.w + ExtraX, Screen.h + ExtraY}});
	const float AnimatedTop = m_Top.Update(std::max(56.f, (Screen.h - Layout.m_Height - Layout.m_SpectatorHeight - 42) / 2), Now);
	// Keep newly arrived content inside the viewport even while a compact layout expands.
	const float Top = std::min(AnimatedTop, std::max(14.f, Screen.h - Layout.m_Height - Layout.m_SpectatorHeight - 64.f));
	CUIRect Scoreboard = {(Screen.w - Layout.m_Width) / 2, Top, Layout.m_Width, Layout.m_Height};
	RenderHeader(Scoreboard.x + Scoreboard.w / 2, Top, Screen.w);
	const char *pRedTeamName = GetTeamName(TEAM_RED);
	const char *pBlueTeamName = GetTeamName(TEAM_BLUE);
	if(Teams && pGameInfoObj)
	{
		// Game over title
		const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
		if((pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && pGameDataObj)
		{
			char aTitle[256];
			if(pGameDataObj->m_TeamscoreRed > pGameDataObj->m_TeamscoreBlue)
			{
				TextRender()->TextColor(TEAM_RED_TITLE_COLOR);
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
				TextRender()->TextColor(TEAM_BLUE_TITLE_COLOR);
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
				TextRender()->TextColor(ColorRGBA(0.91f, 0.78f, 0.33f, 1.0f));
				str_copy(aTitle, Localize("Draw!"));
			}

			const float TitleFontSize = 36.0f;
			CUIRect GameOverTitle = {Scoreboard.x, Scoreboard.y - TitleFontSize - 6.0f, Scoreboard.w, TitleFontSize};
			Ui()->DoLabel(&GameOverTitle, aTitle, TitleFontSize, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
	// Match RenderScoreboard's sorted alive-then-dead traversal, including game-team filtering.
	std::array<std::array<float, MAX_CLIENTS>, 2> aOrderedTeamWidths{};
	std::array<int, 2> aOrderedCounts{};
	for(int Dead = 0; Dead < 2; ++Dead)
		for(const auto *pInfo : GameClient()->m_Snap.m_apInfoByDDTeamScore)
		{
			if(!pInfo || pInfo->m_Team < TEAM_RED || pInfo->m_Team > TEAM_BLUE)
				continue;
			const bool IsDead = Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_DEAD;
			if(IsDead != bool(Dead))
				continue;
			const int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
			const float Width = DDTeam > TEAM_FLOCK && DDTeam < int(m_aDDTeamLabels.size()) ? m_aDDTeamLabels[DDTeam].m_Width : 0;
			aOrderedTeamWidths[pInfo->m_Team][aOrderedCounts[pInfo->m_Team]++] = Width;
		}
	std::array<float, LucentScoreboard::MaxColumns> aGutters{};
	std::array<LucentHud::CRect, LucentScoreboard::MaxColumns> aTargets{};
	std::array<bool, LucentScoreboard::MaxColumns> aVisible{};
	float Bottom = Scoreboard.y + Layout.m_Height + LucentScoreboard::ColumnGap;
	for(int Index = 0; Index < Layout.m_Columns; ++Index)
	{
		const int Group = Teams && Index >= Layout.m_RedColumns ? 1 : 0;
		const int Columns = Group ? Layout.m_BlueColumns : Layout.m_RedColumns;
		const int LocalColumn = Index - (Group ? Layout.m_RedColumns : 0);
		const int Count = aTeamSize[Group == 0 ? TEAM_RED : TEAM_BLUE];
		const auto Range = LucentScoreboard::ColumnRange(Count, LocalColumn, Columns);
		aGutters[Index] = LucentScoreboard::ColumnGutter(aOrderedTeamWidths[Group], Range);
		aTargets[Index] = LucentScoreboard::ColumnBounds(Layout, Scoreboard.x, Scoreboard.y, Index, Range.m_End - Range.m_Start);
		aVisible[Index] = Range.m_End > Range.m_Start || (Teams && LocalColumn == 0);
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
	m_FontSize = LucentScoreboard::CommonFont(m_RowHeight, Layout.ColumnWidth(), LucentScoreboard::CFields::MinimumWidth(m_ScoreWidth, m_PointsWidth, m_PingWidth));
	for(int Index = 0; Index < Layout.m_Columns; ++Index)
		if(aVisible[Index])
			m_FontSize = std::min(m_FontSize, LucentScoreboard::CommonFont(m_RowHeight, aBounds[Index].w,
								  LucentScoreboard::CFields::MinimumWidth(m_ScoreWidth, m_PointsWidth, m_PingWidth, aGutters[Index])));

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
			const auto Range = LucentScoreboard::ColumnRange(Count, Column, Columns);
			RenderScoreboard(Body, Team, Range.m_Start, Range.m_End, aGutters[Index]);
		}
	}
	if(SpectatorCount > 0)
	{
		CUIRect Spectators = {Scoreboard.x, Bottom, Scoreboard.w, SpectatorHeight};
		LucentUi::COpacityScope SpectatorOpacity(TextRender(), SpectatorReveal);
		RenderSpectators(Spectators);
		Bottom += LucentScoreboard::SpectatorSpace(Layout, SpectatorCount);
	}

	if(pGameInfoObj && (pGameInfoObj->m_ScoreLimit || pGameInfoObj->m_TimeLimit || (pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)))
	{
		RenderGoals({Scoreboard.x, Bottom, Scoreboard.w, 20});
		Bottom += 23;
	}
	if(!m_MouseUnlocked)
		RenderMouseHint({Scoreboard.x, Bottom, Scoreboard.w, 26});
	Ui()->MapScreen();

	RenderRecordingNotification((Screen.w / 7) * 4 + 10);

	if(!GameClient()->m_Menus.IsActive() && !GameClient()->m_Chat.IsActive())
	{
		Ui()->RenderPopupMenus();

		if(m_MouseUnlocked)
			RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);

		Ui()->FinishCheck();
	}
}

bool CScoreboard::IsActive() const
{
	// if statboard is active don't show scoreboard
	if(GameClient()->m_Statboard.IsActive())
		return false;

	if(m_Active)
		return true;

	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	if(GameClient()->m_Snap.m_pLocalInfo && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		// we are not a spectator, check if we are dead and the game isn't paused
		if(!GameClient()->m_Snap.m_pLocalCharacter && g_Config.m_ClScoreboardOnDeath &&
			!(pGameInfoObj && pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
			return true;
	}

	// if the game is over
	if(pGameInfoObj && pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetTeamName(int Team) const
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

	int ClanPlayers = 0;
	const char *pClanName = nullptr;
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByScore)
	{
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(!pClanName)
		{
			pClanName = GameClient()->m_aClients[pInfo->m_ClientId].m_aClan;
			ClanPlayers++;
		}
		else
		{
			if(str_comp(GameClient()->m_aClients[pInfo->m_ClientId].m_aClan, pClanName) == 0)
				ClanPlayers++;
			else
				return nullptr;
		}
	}

	if(ClanPlayers > 1 && pClanName[0] != '\0')
		return pClanName;
	else
		return nullptr;
}

CUi::EPopupMenuFunctionResult CScoreboard::CScoreboardPopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CScoreboardPopupContext *pPopupContext = static_cast<CScoreboardPopupContext *>(pContext);
	CScoreboard *pScoreboard = pPopupContext->m_pScoreboard;
	CUi *pUi = pPopupContext->m_pScoreboard->Ui();

	CGameClient::CClientData &Client = pScoreboard->GameClient()->m_aClients[pPopupContext->m_ClientId];

	if(!Client.m_Active)
		return CUi::POPUP_CLOSE_CURRENT;

	constexpr float Margin = 5.0f;
	constexpr float ItemSpacing = 2.0f;
	constexpr float FontSize = 12.0f;
	constexpr float ButtonSize = 17.5f;

	View.Margin(Margin, &View);

	CUIRect Label, Container, Action;

	View.HSplitTop(FontSize, &Label, &View);
	pUi->DoLabel(&Label, Client.m_aName, FontSize, TEXTALIGN_ML);

	if(!pPopupContext->m_IsLocal)
	{
		constexpr int ActionsNum = 3;
		constexpr float ActionSize = 25.0f;
		const float ActionSpacing = (View.w - (ActionsNum * ActionSize)) / 2;
		int ActionCorners = IGraphics::CORNER_ALL;

		View.HSplitTop(ItemSpacing * 2, nullptr, &View);
		View.HSplitTop(ActionSize, &Container, &View);

		Container.VSplitLeft(ActionSize, &Action, &Container);

		ColorRGBA FriendActionColor = Client.m_Friend ? ColorRGBA(0.95f, 0.3f, 0.3f, 0.85f * pUi->ButtonColorMul(&pPopupContext->m_FriendAction)) :
								ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * pUi->ButtonColorMul(&pPopupContext->m_FriendAction));
		const char *pFriendActionIcon = pUi->HotItem() == &pPopupContext->m_FriendAction && Client.m_Friend ? FontIcon::HEART_CRACK : FontIcon::HEART;
		if(pUi->DoButton_FontIcon(&pPopupContext->m_FriendAction, pFriendActionIcon, Client.m_Friend, &Action, BUTTONFLAG_LEFT, ActionCorners, true, FriendActionColor))
		{
			if(Client.m_Friend)
			{
				pScoreboard->GameClient()->Friends()->RemoveFriend(Client.m_aName, Client.m_aClan);
			}
			else
			{
				pScoreboard->GameClient()->Friends()->AddFriend(Client.m_aName, Client.m_aClan);
			}
		}

		pScoreboard->Ui()->DoToolTip(&pPopupContext->m_FriendAction, &Action, Client.m_Friend ? Localize("Remove friend") : Localize("Add friend"));

		Container.VSplitLeft(ActionSpacing, nullptr, &Container);
		Container.VSplitLeft(ActionSize, &Action, &Container);

		if(pUi->DoButton_FontIcon(&pPopupContext->m_MuteAction, FontIcon::BAN, Client.m_ChatIgnore, &Action, BUTTONFLAG_LEFT, ActionCorners))
		{
			Client.m_ChatIgnore ^= 1;
		}
		pScoreboard->Ui()->DoToolTip(&pPopupContext->m_MuteAction, &Action, Client.m_ChatIgnore ? Localize("Unmute") : Localize("Mute"));

		Container.VSplitLeft(ActionSpacing, nullptr, &Container);
		Container.VSplitLeft(ActionSize, &Action, &Container);

		const char *EmoticonActionIcon = Client.m_EmoticonIgnore ? FontIcon::COMMENT_SLASH : FontIcon::COMMENT;
		if(pUi->DoButton_FontIcon(&pPopupContext->m_EmoticonAction, EmoticonActionIcon, Client.m_EmoticonIgnore, &Action, BUTTONFLAG_LEFT, ActionCorners))
		{
			Client.m_EmoticonIgnore ^= 1;
		}
		pScoreboard->Ui()->DoToolTip(&pPopupContext->m_EmoticonAction, &Action, Client.m_EmoticonIgnore ? Localize("Unmute emoticons") : Localize("Mute emoticons"));
	}

	View.HSplitTop(ItemSpacing * 2, nullptr, &View);
	View.HSplitTop(ButtonSize, &Container, &View);

	bool IsSpectating = pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_Active && pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == pPopupContext->m_ClientId;
	ColorRGBA SpectateButtonColor = ColorRGBA(1.0f, 1.0f, 1.0f, (IsSpectating ? 0.25f : 0.5f) * pUi->ButtonColorMul(&pPopupContext->m_SpectateButton));
	if(!pPopupContext->m_IsSpectating)
	{
		if(pUi->DoButton_PopupMenu(&pPopupContext->m_SpectateButton, Localize("Spectate"), &Container, FontSize, TEXTALIGN_MC, 0.0f, false, true, SpectateButtonColor))
		{
			if(IsSpectating)
			{
				pScoreboard->GameClient()->m_Spectator.Spectate(SPEC_FREEVIEW);
				pScoreboard->Console()->ExecuteLine("say /spec", IConsole::CLIENT_ID_UNSPECIFIED);
			}
			else
			{
				if(pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_Active)
				{
					pScoreboard->GameClient()->m_Spectator.Spectate(pPopupContext->m_ClientId);
				}
				else
				{
					// escape the name
					char aEscapedCommand[2 * MAX_NAME_LENGTH + 32];
					str_copy(aEscapedCommand, "say /spec \"");
					char *pDst = aEscapedCommand + str_length(aEscapedCommand);
					str_escape(&pDst, Client.m_aName, aEscapedCommand + sizeof(aEscapedCommand));
					str_append(aEscapedCommand, "\"");

					pScoreboard->Console()->ExecuteLine(aEscapedCommand, IConsole::CLIENT_ID_UNSPECIFIED);
				}
			}
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CScoreboard::CMapTitlePopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CMapTitlePopupContext *pPopupContext = static_cast<CMapTitlePopupContext *>(pContext);
	CScoreboard *pScoreboard = pPopupContext->m_pScoreboard;

	pScoreboard->TextRender()->Text(View.x, View.y, pPopupContext->m_FontSize, pScoreboard->GameClient()->m_aMapDescription, View.w);

	return CUi::POPUP_KEEP_OPEN;
}
