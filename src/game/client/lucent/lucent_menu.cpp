/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "lucent_menu.h"

#include "lucent_menu_effects.h"

#include <base/time.h>

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <cmath>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

bool CLucentMenu::Button(int Index, const char *pLabel, const CUIRect &Rect, float Alpha, bool Enabled, bool Activate, float Offset)
{
	if(Alpha <= 0 && !(Enabled && m_Focus == Index && Activate))
		return false;
	const bool Focus = m_Focus == Index;
	CUIRect Visual = Rect;
	const float Highlight = m_aHighlight[Index];
	Visual.x += Offset + (Index < 3 ? -2.5f : 2.5f) * Highlight;
	const bool Accent = Index == 0;
	const double PulseTime = g_Config.m_ClLucentLogoAnimation && Index < 6 && m_aPulseStart[Index] > 0 ? m_LastFrame - m_aPulseStart[Index] : m_EffectTime;
	CLucentMenuEffects::Chip(Graphics(), Visual, Index, Alpha, Highlight, PulseTime);
	CUIRect Label = Visual;
	const float Inset = std::min(34.0f, Rect.w * 0.27f);
	Label.x += Inset;
	Label.w -= Inset + 10;
	SLabelProperties Props;
	// Width is fitted before rendering; keep the label on one line.
	Props.m_MaxWidth = -1;
	const ColorRGBA PreviousColor = TextRender()->GetTextColor();
	TextRender()->TextColor(ColorRGBA(mix(Accent ? 0.96f : 0.88f, 1.0f, Highlight), mix(Accent ? 0.85f : 0.87f, 0.82f, Highlight), mix(0.92f, 0.9f, Highlight), Alpha));
	const float FontSize = std::min(14.0f, 14.0f * (Label.w - 2) / std::max(1.0f, TextRender()->TextWidth(14.0f, pLabel)));
	Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML, Props);
	TextRender()->TextColor(PreviousColor);
	return !m_BlockInput && Enabled && (Ui()->DoButtonLogic(&m_aButtons[Index], 0, &Rect, BUTTONFLAG_LEFT) || (Focus && Activate));
}

void CLucentMenu::RenderMainMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);
	// Cover the full viewport, including the vanilla menu margin.
	Ui()->Screen()->Draw(ColorRGBA(0.018f, 0.015f, 0.027f, 1.0f), 0, 0);
	const bool Frozen = m_RadialTransition.Active();
	const double Now = Frozen ? m_LastFrame : static_cast<double>(time_get()) / time_freq();
	const bool Animate = g_Config.m_ClLucentLogoAnimation != 0;
	const bool Focused = Kernel()->RequestInterface<IEngineGraphics>()->WindowActive() != 0 && !GameClient()->m_GameConsole.IsActive();
	if(m_LastFrame < 0 || Now - m_LastFrame > 1.0)
	{
		m_Interaction.Reset();
		m_Progress = 0;
		m_Focus = -1;
		m_Secondary.Reset();
		m_ServerControls.Reset();
		std::fill(std::begin(m_aHighlight), std::end(m_aHighlight), 0.0f);
		std::fill(std::begin(m_aWasHot), std::end(m_aWasHot), false);
		std::fill(std::begin(m_aPulseStart), std::end(m_aPulseStart), 0.0);
	}
	if(m_AnimationEpoch < 0)
		m_AnimationEpoch = Now;
	m_EffectTime = Animate ? Now - m_AnimationEpoch : 0;
	const float Dt = m_LastFrame < 0 ? 0 : static_cast<float>(std::min(Now - m_LastFrame, 0.05));
	m_LastFrame = Now;
	const float Response = Animate ? 1 - std::exp(-Dt * 12) : 1;
	const float CenterX = MainView.x + MainView.w / 2;
	const float CenterY = MainView.y + MainView.h * 0.43f;
	const float LogoWidth = std::min(400.0f, MainView.w * 0.38f);
	const float Scale = LogoWidth / 400;
	CUIRect Logo = {CenterX - 180 * Scale, CenterY - 44 * Scale, 360 * Scale, 88 * Scale};
	const float Width = std::min(164.0f, MainView.w * 0.205f);
	const float Gap = std::min(58.0f, MainView.w * 0.06f);
	const float Pitch = 55.0f;
	const float Height = 34.0f;
	CUIRect aRects[6];
	const float aShiftX[] = {8, -4, 3, -3, 6, -9};
	const float aShiftY[] = {-5, 0, 4, -1, 3, 0};
	for(int i = 0; i < 6; ++i)
	{
		const float X = i < 3 ? Logo.x - Gap - Width : Logo.x + Logo.w + Gap;
		aRects[i] = {std::clamp(X + aShiftX[i], MainView.x + 12, MainView.x + MainView.w - Width - 12), CenterY + (i % 3 - 1) * Pitch - Height / 2 + aShiftY[i], Width, Height};
	}
	// Use the actual group extents, including each row's asymmetric offset.
	const auto Bounds = [&aRects](int First) {
		CUIRect Result = aRects[First];
		for(int i = First + 1; i < First + 3; ++i)
		{
			const float Right = std::max(Result.x + Result.w, aRects[i].x + aRects[i].w);
			const float Bottom = std::max(Result.y + Result.h, aRects[i].y + aRects[i].h);
			Result.x = std::min(Result.x, aRects[i].x);
			Result.y = std::min(Result.y, aRects[i].y);
			Result.w = Right - Result.x;
			Result.h = Bottom - Result.y;
		}
		return Result;
	};
	const CUIRect Left = Bounds(0), Right = Bounds(3);
	CUIRect More = {CenterX - 42, Logo.y + Logo.h + 24, 84, 26};
	CUIRect aExtra[5];
	for(int i = 0; i < 5; ++i)
		aExtra[i] = {CenterX - 90, More.y + More.h + 8 + i * 28, 180, 26};
	const vec2 Mouse = Frozen ? m_RadialTransition.Origin() : vec2(Ui()->MouseX(), Ui()->MouseY());
	const bool Running = GameClient()->m_LocalServer.IsServerRunning() || GameClient()->m_LocalServer.IsStarting();
	const CUIRect ServerPanel = CLucentServerControls::Bounds(aRects[3], Right);
	bool Open = m_Interaction.IsOpen();
	bool Enter = false;
	if(!Frozen)
	{
		if(distance(Mouse, m_LastMouse) > 1.0f)
		{
			m_Focus = -1;
			m_LastMouse = Mouse;
		}
		bool Keyboard = false;
		int aOrder[14] = {0, 1, 2, 3};
		int Count = 4;
		if(m_ServerControls.Interactive())
		{
			aOrder[Count++] = 12;
			aOrder[Count++] = 13;
		}
		for(int i = 4; i < 7; ++i)
			aOrder[Count++] = i;
		if(m_Secondary.IsOpen())
			for(int i = 7; i < 12; ++i)
				aOrder[Count++] = i;
		const auto AdvanceFocus = [&](int Direction) {
			int Position = -1;
			for(int i = 0; i < Count; ++i)
				if(aOrder[i] == m_Focus)
					Position = i;
			m_Focus = aOrder[Position < 0 ? (Direction < 0 ? Count - 1 : 0) : (Position + Count + Direction) % Count];
		};
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_TAB) || Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN))
		{
			AdvanceFocus(Input()->ShiftIsPressed() ? -1 : 1);
			Keyboard = true;
		}
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_UP))
		{
			AdvanceFocus(-1);
			Keyboard = true;
		}
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_LEFT))
		{
			m_Focus = m_Focus >= 3 && m_Focus < 6 ? m_Focus - 3 : 0;
			Keyboard = true;
		}
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_RIGHT))
		{
			m_Focus = m_Focus >= 0 && m_Focus < 3 ? m_Focus + 3 : 3;
			Keyboard = true;
		}
		if(Keyboard)
			m_LastMouse = Mouse;
		Enter = Focused && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
		const bool Escape = Focused && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE);
		if(Escape)
		{
			if(m_ServerControls.IsOpen())
			{
				m_ServerControls.Close();
				m_Focus = 3;
			}
			else if(m_Secondary.IsOpen())
			{
				m_Secondary.Close();
				m_Focus = 6;
			}
			else if(m_Interaction.IsOpen())
			{
				m_Interaction.Close();
				m_Focus = -1;
			}
			else
				GameClient()->m_Menus.ShowQuitPopup();
		}
		bool OverControl = false;
		if(m_Progress > 0 || m_Interaction.IsOpen())
		{
			for(const auto &Rect : aRects)
				OverControl |= CLucentMenuInteraction::Contains(Rect, Mouse);
			OverControl |= CLucentMenuInteraction::Contains(More, Mouse);
			if(m_ServerControls.IsOpen())
			{
				CUIRect Corridor = ServerPanel;
				Corridor.y = Right.y + Right.h;
				Corridor.h += 10;
				OverControl |= CLucentMenuInteraction::Contains(Corridor, Mouse);
			}
			if(m_Secondary.Progress(0) > 0)
			{
				// A continuous corridor includes the gaps below More and between rows.
				CUIRect Corridor = {aExtra[0].x, More.y, aExtra[0].w, More.h};
				for(int i = 0; i < 5; ++i)
					if(m_Secondary.Progress(i) > 0.05f)
						Corridor.h = aExtra[i].y + aExtra[i].h - Corridor.y;
				OverControl |= CLucentMenuInteraction::Contains(Corridor, Mouse);
			}
		}
		if(!Focused)
		{
			m_Interaction.Close();
			m_Focus = -1;
		}
		Open = Focused && m_Interaction.Update(Now, Mouse, Logo, Left, Right, OverControl, Keyboard || m_Focus >= 0, m_Progress > 0);
		const float Target = Open ? 1.0f : 0.0f;
		m_Progress = Animate ? std::clamp(m_Progress + (Open ? Dt : -Dt) / 0.35f, 0.0f, 1.0f) : Target;
		m_Secondary.Update(Dt, Animate, Open, m_Progress > 0, Focused);
		if(m_ServerControls.IsOpen() && Ui()->MouseButtonClicked(0) &&
			!CLucentMenuInteraction::Contains(aRects[3], Mouse) && !CLucentMenuInteraction::Contains(CLucentServerControls::HitArea(ServerPanel), Mouse))
			m_ServerControls.Close();
		m_ServerControls.Update(Dt, Animate, Running, Open, Focused);
		if(!m_ServerControls.IsOpen() && m_Focus >= 12)
			m_Focus = Open ? 3 : -1;
		if(!Open && m_Focus >= 7 && m_Focus < 12)
			m_Focus = -1;
		m_LogoEnergy = mix(m_LogoEnergy, Ui()->MouseInside(&Logo) ? 1.0f : 0.0f, Response);
		const vec2 Cursor = Mouse - vec2(CenterX, CenterY);
		const vec2 Parallax(std::clamp(Cursor.x / 160.0f, -1.0f, 1.0f) * 2 * Scale, std::clamp(Cursor.y / 100.0f, -1.0f, 1.0f) * 1.5f * Scale);
		m_OrbitOffset = mix(m_OrbitOffset, Animate ? Parallax : vec2(0, 0), Response);
		for(int i = 6; i < 14; ++i)
		{
			CUIRect Rect = i == 6 ? More : i < 12 ? aExtra[i - 7] :
								CLucentServerControls::Row(ServerPanel, i - 12, m_ServerControls.Progress());
			if(i >= 7 && i < 12)
				Rect.y -= 10 * (1 - m_Secondary.Progress(i - 7));
			const bool Hot = Open && (i == 6 || (i >= 7 && i < 12 && m_Secondary.Interactive(i - 7)) || (i >= 12 && m_ServerControls.Interactive())) && (Ui()->MouseInside(&Rect) || m_Focus == i || (i == 6 && m_Secondary.IsOpen()));
			m_aHighlight[i] = mix(m_aHighlight[i], Hot ? 1.0f : 0.0f, Response);
		}
	}
	// Lines form first; both lines and chips retract smoothly towards the orbit.
	for(int i = 0; i < 6; ++i)
	{
		if(!Frozen)
		{
			const bool Hot = Open && (Ui()->MouseInside(&aRects[i]) || m_Focus == i || (i == 3 && m_ServerControls.IsOpen()));
			if(Hot && !m_aWasHot[i])
				m_aPulseStart[i] = Now;
			m_aWasHot[i] = Hot;
			m_aHighlight[i] = mix(m_aHighlight[i], Hot ? 1.0f : 0.0f, Response);
		}
		CUIRect ConnectionTarget = aRects[i];
		ConnectionTarget.x += (i < 3 ? -2.5f : 2.5f) * m_aHighlight[i];
		CLucentMenuEffects::Connection(Graphics(), vec2(CenterX, CenterY) + m_OrbitOffset, Scale, ConnectionTarget, i, m_Progress, m_aHighlight[i], Animate && m_aPulseStart[i] > 0 ? static_cast<float>(Now - m_aPulseStart[i]) : -1);
	}
	// Cached wordmark with local orbit reactions for each navigation direction.
	const float Activation = 1 - std::pow(1 - m_Progress, 3.0f);
	float aOrbitReaction[6];
	for(int i = 0; i < 6; ++i)
	{
		aOrbitReaction[i] = m_aHighlight[i] * 0.65f;
	}
	m_Logo.Render(Graphics(), TextRender(), CenterX, CenterY - 29 * Scale, LogoWidth, m_EffectTime, m_LogoEnergy, m_OrbitOffset, aOrbitReaction);
	bool aClick[6] = {};
	const char *apLabels[] = {Localize("Play", "Start menu"), Localize("Demos"), Localize("Editor"), Localize("Run server"), Localize("Settings"), Localize("Quit")};
	for(int i = 0; i < 6; ++i)
	{
		const float T = Animate ? std::clamp((m_Progress * 0.35f - 0.065f - (i % 3) * 0.02f) / 0.245f, 0.0f, 1.0f) : m_Progress;
		const float Ease = 1 - std::pow(1 - T, 3.0f);
		const float Offset = (i < 3 ? 1 : -1) * 32 * (1 - Ease);
		// Stable targets: only the label moves, never the clickable rectangle.
		aClick[i] = Button(i, apLabels[i], aRects[i], Ease, Open, Enter, Offset);
	}
	int NewPage = -1;
	if(aClick[0] || (Enter && m_Focus < 0) || CheckHotKey(KEY_P))
	{
		const int Page = g_Config.m_UiPage;
		GameClient()->m_Menus.SetMenuPage(Page >= CMenus::PAGE_INTERNET && Page <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? Page : CMenus::PAGE_INTERNET);
		m_ServerControls.Reset();
		m_RadialTransition.Start(Now, Mouse, Animate);
		Ui()->SetActiveItem(nullptr);
		Ui()->SetHotItem(nullptr);
		Ui()->ClearHotkeys();
		m_BlockInput = true;
		Ui()->SetEnabled(false);
	}
	if(aClick[1] || CheckHotKey(KEY_D))
		NewPage = CMenus::PAGE_DEMOS;
	if(aClick[2] || CheckHotKey(KEY_E))
	{
		m_ServerControls.Reset();
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
	}
	if(CheckHotKey(KEY_J) && Running)
	{
		m_ServerControls.Close();
		GameClient()->m_LocalServer.Connect();
	}
	if(aClick[3] || CheckHotKey(KEY_R))
	{
		// Recheck at dispatch: a fast second activation must never start another
		// process or turn a harmless chip click into an accidental stop.
		if(!CLucentServerControls::CanStart(GameClient()->m_LocalServer.IsServerRunning(), GameClient()->m_LocalServer.IsStarting()))
			m_ServerControls.Toggle();
		else if(GameClient()->m_LocalServer.RunServer({}))
			m_ServerControls.Open();
		m_Secondary.Close();
		m_ServerControls.Update(0, Animate, GameClient()->m_LocalServer.IsServerRunning() || GameClient()->m_LocalServer.IsStarting(), Open, Focused);
	}
	const float ServerReveal = m_ServerControls.Progress();
	if(ServerReveal > 0)
	{
		CUIRect Surface = ServerPanel;
		Surface.y -= 8 * (1 - ServerReveal);
		Surface.Draw(ColorRGBA(0.035f, 0.025f, 0.045f, Activation * ServerReveal * 0.98f), IGraphics::CORNER_ALL, 10);
	}
	for(int Row = 0; Row < 2; ++Row)
	{
		const CUIRect Rect = CLucentServerControls::Row(ServerPanel, Row, ServerReveal);
		if(!Button(12 + Row, Row == 0 ? Localize("Join") : Localize("Stop server"), Rect, Activation * ServerReveal, Open && m_ServerControls.Interactive(), Enter))
			continue;
		m_ServerControls.Close();
		m_Focus = 3;
		if(Row == 0)
			GameClient()->m_LocalServer.Connect();
		else
			GameClient()->m_LocalServer.KillServer();
		Ui()->SetActiveItem(nullptr);
	}
	if(aClick[4] || CheckHotKey(KEY_S))
		NewPage = CMenus::PAGE_SETTINGS;
	if(aClick[5] || CheckHotKey(KEY_Q))
	{
		m_ServerControls.Reset();
		if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
			GameClient()->m_Menus.ShowQuitPopup();
		else
			Client()->Quit();
	}
	if(Button(6, Localize("More"), More, Activation, Open, Enter))
	{
		m_ServerControls.Close();
		m_Secondary.Toggle();
		// The disabled-animation path resolves in the activation frame too.
		m_Secondary.Update(0, Animate, Open, m_Progress > 0, Focused);
	}
	const char *apExtra[] = {Localize("News"), Localize("Website"), Localize("Tutorial"), Localize("Learn"), Localize("Discord")};
	for(int i = 0; i < 5; ++i)
	{
		const float Reveal = m_Secondary.Progress(i);
		CUIRect Row = aExtra[i];
		Row.y -= 10 * (1 - Reveal);
		if(!Button(7 + i, apExtra[i], Row, Activation * Reveal, Open && m_Secondary.Interactive(i), Enter))
			continue;
		if(i == 0)
			NewPage = CMenus::PAGE_NEWS;
		if(i == 1)
			Client()->ViewLink("https://ddnet.org/");
		if(i == 2)
			GameClient()->m_Menus.JoinTutorial();
		if(i == 3)
			Client()->ViewLink(Localize("https://wiki.ddnet.org/"));
		if(i == 4)
			Client()->ViewLink(Localize("https://ddnet.org/discord"));
	}
	if(CheckHotKey(KEY_N))
		NewPage = CMenus::PAGE_NEWS;
	// Keep secondary DDNet controls accessible without cluttering the resting view.
	if(m_Secondary.IsOpen())
	{
		const float VMargin = std::max(20.0f, MainView.w / 2 - 190.0f);
		// render version
		CUIRect CurVersion, ConsoleButton;
		MainView.HSplitBottom(45.0f, nullptr, &CurVersion);
		CurVersion.VSplitRight(40.0f, &CurVersion, nullptr);
		CurVersion.HSplitTop(20.0f, &ConsoleButton, &CurVersion);
		CurVersion.HSplitTop(5.0f, nullptr, &CurVersion);
		ConsoleButton.VSplitRight(40.0f, nullptr, &ConsoleButton);
		Ui()->DoLabel(&CurVersion, GAME_RELEASE_VERSION, 14.0f, TEXTALIGN_MR);

		static CButtonContainer s_ConsoleButton;
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &ConsoleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f)))
		{
			GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
		}
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		CUIRect VersionUpdate;
		MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
		VersionUpdate.VMargin(VMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
		CUIRect UpdateButton;
		VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &UpdateButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		char aBuf[128];
		const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
		const bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");

		if(State == IUpdater::CLEAN && NeedUpdate)
		{
			static CButtonContainer s_VersionUpdate;
			if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				Updater()->InitiateUpdate();
			}
		}
		else if(State == IUpdater::NEED_RESTART)
		{
			static CButtonContainer s_VersionUpdate;
			if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				Client()->Restart();
			}
		}
		else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
		{
			Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
		}

		if(State == IUpdater::CLEAN && NeedUpdate)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		}
		else if(State == IUpdater::CLEAN)
		{
			aBuf[0] = '\0';
		}
		else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
		{
			char aCurrentFile[64];
			Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
			str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
		}
		else if(State == IUpdater::FAIL)
		{
			str_copy(aBuf, Localize("Update failed! Check log…"));
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		}
		else if(State == IUpdater::NEED_RESTART)
		{
			str_copy(aBuf, Localize("Lucent Client updated!"));
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		}
		Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
#elif defined(CONF_INFORM_UPDATE)
		if(str_comp(Client()->LatestVersion(), "0") != 0)
		{
			CUIRect DownloadButton;
			VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &DownloadButton);
			VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

			static CButtonContainer s_DownloadButton;
			if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download"), 0, &DownloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
			{
				Client()->ViewLink("https://ddnet.org/downloads/");
			}

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
			SLabelProperties UpdateLabelProps;
			UpdateLabelProps.SetColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
			Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML, UpdateLabelProps);
		}
#endif
	}

	if(NewPage != -1)
	{
		m_Interaction.Reset();
		m_Secondary.Reset();
		m_ServerControls.Reset();
		m_Focus = -1;
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

bool CLucentMenu::CheckHotKey(int Key) const
{
	return !m_BlockInput && !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && Input()->KeyPress(Key) && !GameClient()->m_GameConsole.IsActive();
}
