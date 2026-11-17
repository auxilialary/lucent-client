#include "chat_component.h"

#include "chat_policy.h"

#include <base/time.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/lucent/hud/layout.h>
#include <game/client/lucent/hud/visuals.h>
#include <game/client/lucent/localization.h>

bool CLucentChat::Allow(int ClientId, const char *pText) const
{
	using C = LucentChat::ECategory;
	switch(LucentChat::Classify(ClientId, pText))
	{
	case C::SYSTEM: return g_Config.m_ClLucentChatSystem;
	case C::WELCOME: return g_Config.m_ClLucentChatWelcome;
	case C::RACE: return g_Config.m_ClLucentChatRace;
	case C::CONNECTION: return g_Config.m_ClLucentChatConnections;
	default: return true;
	}
}

void CLucentChat::Mention(int ClientId, const char *pText, bool Highlighted, const std::shared_ptr<CManagedTeeRenderInfo> &pTee)
{
	if(!g_Config.m_ClLucentChatMentions || !Highlighted || ClientId < 0 || ClientId >= MAX_CLIENTS || Client()->State() != IClient::STATE_ONLINE)
		return;
	// Bound the backlog without disturbing cards already on screen.
	if(m_Queue.size() >= 64)
		return;
	m_Queue.push_back({GameClient()->m_aClients[ClientId].m_aName, pText, -1, pTee});
}

void CLucentChat::OnReset()
{
	m_Queue.clear();
	m_Width = {};
	m_LastRender = -1;
}

void CLucentChat::OnRender()
{
	if(!g_Config.m_ClLucentChatMentions || Client()->State() != IClient::STATE_ONLINE)
	{
		OnReset();
		return;
	}
	if(m_Queue.empty())
	{
		OnReset();
		return;
	}
	const double Now = time() / (double)time_freq();
	while(!m_Queue.empty() && m_Queue.front().m_Start >= 0 && Now - m_Queue.front().m_Start >= 4.8)
		m_Queue.pop_front();
	if(m_Queue.empty())
	{
		OnReset();
		return;
	}
	int Visible = 0;
	for(auto &Message : m_Queue)
	{
		if(Visible++ == 3)
			break;
		if(Message.m_Start < 0)
			Message.m_Start = Now;
	}
	if(!g_Config.m_ClShowhud || GameClient()->m_Menus.IsActive() || GameClient()->m_Scoreboard.IsActive())
		return;
	const float Width = 300.f * Graphics()->ScreenAspect();
	const auto OldScreen = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(Width, 300);
	const float Top = g_Config.m_LucentHud ? GameClient()->m_Hud.LucentTimerBottom() + 8.f : 48.f;
	RenderCards(m_Queue, m_Width, m_LastRender, Now, Width / 2, Top, std::max(80.f, std::min(220.f, Width - 20.f)), false);
	Graphics()->MapScreen(OldScreen);
}

void CLucentChat::RenderCards(const std::deque<CMention> &Messages, LucentHud::Alignment::CRowPosition &WidthState, double &LastRender, double Now, float CenterX, float Top, float MaxWidth, bool Preview)
{
	const float FontSize = LucentHud::TargetFontSize(LucentHud::CaptionSize, Graphics()->ScreenHeight(), Graphics()->GetScreen().Size().y);
	int Visible = 0;
	float TargetWidth = 80;
	for(const auto &Message : Messages)
	{
		if(Visible++ == 3)
			break;
		const float HeaderWidth = 22 + TextRender()->TextWidth(FontSize, Message.m_Name.c_str()) + 8 + TextRender()->TextWidth(FontSize, LCNTLocalize("Mention")) + 6;
		TargetWidth = std::max(TargetWidth, std::max(HeaderWidth, TextRender()->TextWidth(FontSize, Message.m_Text.c_str()) + 12));
	}
	WidthState.Update(std::min(TargetWidth, MaxWidth), LastRender < 0 ? 0.f : (float)(Now - LastRender));
	LastRender = Now;
	const float CardWidth = std::clamp(WidthState.m_Y, 80.f, MaxWidth);
	const float X = CenterX - CardWidth / 2;
	float Heights[3] = {}, Reveals[3] = {};
	float TotalHeight = 0, BackgroundAlpha = 0;
	Visible = 0;
	for(const auto &Message : Messages)
	{
		if(Visible == 3)
			break;
		CTextCursor Measure;
		Measure.SetPosition(vec2(0, 0));
		Measure.m_FontSize = FontSize;
		Measure.m_Flags = 0;
		Measure.m_LineWidth = CardWidth - 12;
		Measure.m_MaxLines = 3;
		TextRender()->TextEx(&Measure, Message.m_Text.c_str());
		Heights[Visible] = 22.f + std::min(3, Measure.m_LineCount) * FontSize;
		Reveals[Visible] = LucentChat::Opacity(Now - Message.m_Start);
		TotalHeight += Heights[Visible] * Reveals[Visible];
		BackgroundAlpha = std::max(BackgroundAlpha, Reveals[Visible]);
		++Visible;
	}
	LucentHud::Panel(Graphics(), {X, Top, CardWidth, TotalHeight}, BackgroundAlpha * CUIRect::DrawOpacity(), 1);
	const auto OldColor = TextRender()->GetTextColor();
	const auto OldOutline = TextRender()->GetTextOutlineColor();
	TextRender()->TextOutlineColor(0, 0, 0, 0);
	float Alpha = 1;
	const auto Text = [&](const char *pText, float Tx, float Ty, float Size, float LineWidth, int Lines, ColorRGBA Color) {
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(Tx, Ty));
		Cursor.m_FontSize = Size;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
		if(Lines == 1)
			Cursor.m_Flags |= TEXTFLAG_DISALLOW_NEWLINE;
		Cursor.m_LineWidth = LineWidth;
		Cursor.m_MaxLines = Lines;
		TextRender()->TextColor(Color.WithMultipliedAlpha(Alpha));
		TextRender()->TextEx(&Cursor, pText);
	};
	float Y = Top;
	Visible = 0;
	for(const auto &Message : Messages)
	{
		if(Visible == 3)
			break;
		Alpha = Reveals[Visible];
		const float RowHeight = Heights[Visible] * Reveals[Visible];
		const auto Screen = Graphics()->GetScreen();
		const auto ScreenSize = Screen.Size();
		const auto *pUiScreen = Ui()->Screen();
		const float ScaleX = pUiScreen->w / ScreenSize.x;
		const float ScaleY = pUiScreen->h / ScreenSize.y;
		CUIRect RowClip = {pUiScreen->x + (X - Screen.m_TopLeft.x) * ScaleX, pUiScreen->y + (Y - Screen.m_TopLeft.y) * ScaleY, CardWidth * ScaleX, RowHeight * ScaleY};
		Ui()->ClipEnable(&RowClip);
		Alpha *= CUIRect::DrawOpacity();
		if(Message.m_pTee)
		{
			auto Info = Message.m_pTee->TeeRenderInfo();
			Info.m_Size = 12;
			RenderTools()->RenderIdleTeeAtCenter(&Info, vec2(X + 12, Y + 10), EMOTE_NORMAL, vec2(1, .1f), Alpha);
		}
		else if(Preview)
		{
			CTeeRenderInfo Info;
			Info.Apply(GameClient()->m_Skins.Find("default"));
			Info.m_Size = 12;
			RenderTools()->RenderIdleTeeAtCenter(&Info, vec2(X + 12, Y + 10), EMOTE_NORMAL, vec2(1, .1f), Alpha);
		}
		const char *pCaption = LCNTLocalize("Mention");
		const float CaptionWidth = TextRender()->TextWidth(FontSize, pCaption);
		Text(Message.m_Name.c_str(), X + 22, Y + 7, FontSize, CardWidth - CaptionWidth - 36, 1, ColorRGBA(.9f, .9f, .93f, 1));
		Text(pCaption, X + CardWidth - CaptionWidth - 6, Y + 7, FontSize, CaptionWidth + 1, 1, ColorRGBA(.6f, .6f, .65f, 1));
		Text(Message.m_Text.c_str(), X + 6, Y + 17, FontSize, CardWidth - 12, 3, ColorRGBA(.8f, .8f, .84f, 1));
		Ui()->ClipDisable();
		Y += RowHeight;
		++Visible;
	}
	TextRender()->TextColor(OldColor);
	TextRender()->TextOutlineColor(OldOutline);
}

void CLucentChat::RenderPreview(CUIRect View, double Elapsed)
{
	const auto OldScreen = Graphics()->GetScreen();
	const float Scale = std::min(2.f, View.w / 150.f);
	if(Scale <= 0)
		return;
	Graphics()->MapScreen(CScreenRect(OldScreen.m_TopLeft / Scale, OldScreen.m_BottomRight / Scale));
	const float CenterX = View.Center().x / Scale;
	const float Top = View.y / Scale + 8;
	const auto OldColor = TextRender()->GetTextColor();
	const auto OldOutline = TextRender()->GetTextOutlineColor();
	TextRender()->TextOutlineColor(0, 0, 0, 0);
	const float TimerWidth = TextRender()->TextWidth(9, "2d 03:54:13") + 12;
	LucentHud::Panel(Graphics(), {CenterX - TimerWidth / 2, Top, TimerWidth, 33}, CUIRect::DrawOpacity(), 1);
	TextRender()->TextColor(ColorRGBA(.9f, .9f, .93f, CUIRect::DrawOpacity()));
	TextRender()->Text(CenterX - TextRender()->TextWidth(9, "2d 03:54:13") / 2, Top + 5, 9, "2d 03:54:13");
	TextRender()->TextColor(ColorRGBA(.6f, .6f, .65f, CUIRect::DrawOpacity()));
	TextRender()->Text(CenterX - TextRender()->TextWidth(5, "11:15") / 2, Top + 23, 5, "11:15");
	TextRender()->TextColor(OldColor);
	TextRender()->TextOutlineColor(OldOutline);
	const double Phase = std::fmod(std::max(0.0, Elapsed), 8.0);
	const double Now = time() / (double)time_freq();
	std::deque<CMention> Messages;
	if(g_Config.m_ClLucentChatMentions && Phase >= .5 && Phase < 5.3)
		Messages.push_back({"Yao", LCNTLocalize("Hey, come here!"), Now - (Phase - .5), nullptr});
	if(g_Config.m_ClLucentChatMentions && Phase >= 2 && Phase < 6.8)
		Messages.push_back({"Lucent", LCNTLocalize("Join us! I will show you the next part of the map."), Now - (Phase - 2), nullptr});
	if(!Messages.empty())
		RenderCards(Messages, m_PreviewWidth, m_LastPreview, Now, CenterX, Top + 41, std::max(80.f, std::min(220.f, View.w / Scale - 12)), true);
	else
		ResetPreview();
	Graphics()->MapScreen(OldScreen);
}
