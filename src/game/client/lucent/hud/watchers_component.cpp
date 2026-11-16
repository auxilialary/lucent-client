#include "watchers_component.h"
#include "watchers.h"
#include "visuals.h"

#include <base/time.h>
#include <engine/client.h>
#include <engine/shared/config.h>
#include <generated/protocol7.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

void CLucentWatchers::ResetPresentation()
{
	m_LucentWatcherVisibility = {};
	m_LucentWatcherHeight = {};
	m_LucentWatcherWidth = {};
	for(auto &Row : m_aLucentWatcherRows)
		Row = {};
}

void CLucentWatchers::OnReset()
{
	ResetPresentation();
	std::fill(std::begin(m_aSpectatorWatchers), std::end(m_aSpectatorWatchers), false);
	m_Count = 0;
}

void CLucentWatchers::OnNewSnapshot()
{
	std::fill(std::begin(m_aSpectatorWatchers), std::end(m_aSpectatorWatchers), false);
	m_Count = 0;
	bool HasCount = false;
	for(int i = 0; i < Client()->SnapNumItems(IClient::SNAP_CURRENT); ++i)
	{
		const auto Item = Client()->SnapGetItem(IClient::SNAP_CURRENT, i);
		if(Item.m_Type != NETOBJTYPE_SPECTATORCOUNT)
			continue;
		const auto *pCount = static_cast<const CNetObj_SpectatorCount *>(Item.m_pData);
		if(Item.m_Id == 0 || !HasCount)
		{
			m_Count = pCount->m_NumSpectators;
			HasCount = true;
		}
		// Servers may supply watcher IDs as additional count objects (ID + 1).
		if(Item.m_Id > 0 && Item.m_Id <= MAX_CLIENTS)
			m_aSpectatorWatchers[Item.m_Id - 1] = true;
	}
}

bool CLucentWatchers::IsWatchingLocal(int ClientId) const
{
	const auto *pGame = GameClient();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS ||
		!LucentHud::Watchers::Eligible(ClientId, pGame->m_Snap.m_LocalClientId, pGame->m_aClients[ClientId].m_Active, pGame->m_Snap.m_apPlayerInfos[ClientId] != nullptr))
		return false;
	return Client()->IsSixup() ?
		(Client()->m_TranslationContext.m_aClients[ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_WATCHING) != 0 :
		m_aSpectatorWatchers[ClientId];
}

void CLucentWatchers::Render(float ScreenWidth, float ScreenHeight, const LucentHud::CRect &TimerBounds)
{
	const auto &Snap = GameClient()->m_Snap;
	if(!g_Config.m_LucentHud || !Snap.m_pLocalCharacter || Snap.m_SpecInfo.m_Active || !Snap.m_pGameInfoObj ||
		(Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		ResetPresentation();
		return;
	}
	const double Now = time() / (double)time_freq();
	auto *pGame = GameClient();
	bool aWatchers[MAX_CLIENTS] = {};
	bool aCandidates[MAX_CLIENTS] = {};
	int Named = 0, Candidates = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		aWatchers[i] = IsWatchingLocal(i);
		Named += aWatchers[i];
		const auto *pInfo = pGame->m_Snap.m_apPlayerInfos[i];
		const auto &Data = pGame->m_aClients[i];
		aCandidates[i] = LucentHud::Watchers::Eligible(i, pGame->m_Snap.m_LocalClientId, Data.m_Active, pInfo != nullptr) &&
			(pInfo->m_Team == TEAM_SPECTATORS || Data.m_Spec || Data.m_Paused);
		Candidates += aCandidates[i];
	}
	const int Count = std::max(Named, m_Count);
	// Best Client's count-matching fallback is an inference, not a server-confirmed identity.
	const bool Inferred = LucentHud::Watchers::InferNames(Client()->IsSixup(), Named, Count, Candidates);
	if(Inferred)
		std::copy(std::begin(aCandidates), std::end(aCandidates), std::begin(aWatchers));
	const bool Visible = g_Config.m_ClShowhudSpectatorCount && Count > 0;
	const float Reveal = m_LucentWatcherVisibility.Update(Visible, Now, true, LucentStyle::Motion::Selection);
	if(Reveal <= 0)
		return;
	const LucentHud::CLayout Layout(ScreenWidth, ScreenHeight, g_Config.m_LucentHudScale);
	const float Scale = Layout.m_Scale;
	const float Size = LucentHud::TargetFontSize(6 * Scale, Graphics()->ScreenHeight(), ScreenHeight);
	const float CaptionSize = LucentHud::TargetFontSize(5 * Scale, Graphics()->ScreenHeight(), ScreenHeight);
	const float Alpha = Reveal * LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	const float RowHeight = 8 * Scale;
	const int Capacity = std::max(1, (int)((ScreenHeight - 32 * Scale) / RowHeight));
	const int MaxRows = Capacity - (Count > Capacity ? 1 : 0);
	char aTitle[128];
	str_format(aTitle, sizeof(aTitle), "%s | %d", Localize("Spectators"), Count);
	float ContentWidth = TextRender()->TextWidth(CaptionSize, aTitle);
	float Rows = 0;
	int Shown = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const bool Show = Visible && aWatchers[i] && Shown < MaxRows;
		if(Show)
		{
			++Shown;
			str_format(m_aaLucentWatcherNames[i], sizeof(m_aaLucentWatcherNames[i]), "%s%s", Inferred ? "~ " : "", pGame->m_aClients[i].m_aName);
		}
		const float Row = m_aLucentWatcherRows[i].Update(Show, Now, true, LucentStyle::Motion::Selection);
		Rows += Row;
		if(Row > 0 || Show)
			ContentWidth = std::max(ContentWidth, TextRender()->TextWidth(Size, m_aaLucentWatcherNames[i]));
	}
	// With no known names the header already conveys the entire available information.
	const int Remaining = Shown > 0 ? std::max(0, Count - Shown) : 0;
	char aRemaining[32] = "";
	if(Remaining > 0)
	{
		str_format(aRemaining, sizeof(aRemaining), "+%d", Remaining);
		ContentWidth = std::max(ContentWidth, TextRender()->TextWidth(Size, aRemaining));
	}
	const float Dt = Client()->RenderFrameTime();
	m_LucentWatcherHeight.Update((12 + 8 * (Rows + (Remaining > 0))) * Scale, Dt);
	const float Left = TimerBounds.w > 0 ? TimerBounds.x + TimerBounds.w + 6 * Scale : ScreenWidth / 2 + 6 * Scale;
	const float MaxWidth = std::max(1.f, std::min(160 * Scale, ScreenWidth - Left - 6 * Scale));
	m_LucentWatcherWidth.Update(std::min(ContentWidth + 8 * Scale, MaxWidth), Dt);
	const float Width = std::min(m_LucentWatcherWidth.m_Y, MaxWidth);
	const float X = std::clamp(Left, 6 * Scale, std::max(6 * Scale, ScreenWidth - Width - 6 * Scale));
	const LucentHud::CRect Panel = {X, 6 * Scale - (1 - Reveal) * 4 * Scale, Width, m_LucentWatcherHeight.m_Y};
	LucentHud::Panel(Graphics(), Panel, Alpha, Scale);
	char aCaption[128];
	LucentHud::Alignment::FitLabel(aCaption, aTitle, std::max(0.f, Width - 8 * Scale),
		[&](const char *pText) { return TextRender()->TextWidth(CaptionSize, pText); });
	TextRender()->TextColor(LucentHud::Secondary(Alpha));
	TextRender()->Text(Panel.x + 4 * Scale, Panel.y + 3 * Scale, CaptionSize, aCaption);
	float Y = Panel.y + 11 * Scale;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const float Row = m_aLucentWatcherRows[i].m_Value;
		if(Row <= 0)
			continue;
		if(Y + Size > Panel.y + Panel.h - Scale)
			break;
		char aName[128];
		const float Slide = (1 - Row) * 3 * Scale;
		LucentHud::Alignment::FitLabel(aName, m_aaLucentWatcherNames[i], std::max(0.f, Width - 8 * Scale - Slide),
			[&](const char *pText) { return TextRender()->TextWidth(Size, pText); });
		TextRender()->TextColor(LucentHud::Text(Alpha * Row));
		TextRender()->Text(Panel.x + 4 * Scale + Slide, Y, Size, aName);
		Y += RowHeight * Row;
	}
	if(Remaining > 0 && Y + Size <= Panel.y + Panel.h - Scale)
	{
		TextRender()->TextColor(LucentHud::Secondary(Alpha));
		TextRender()->Text(Panel.x + 4 * Scale, Y, Size, aRemaining);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
