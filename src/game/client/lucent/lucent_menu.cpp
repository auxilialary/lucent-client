/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "lucent_menu.h"

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
	const bool Hover = Enabled && Ui()->MouseInside(&Rect);
	const bool Focus = m_Focus == Index;
	CUIRect Visual = Rect;
	Visual.x += Offset;
	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w - 8;
	Props.SetColor(Hover || Focus ? ColorRGBA(1, 0.72f, 0.84f, Alpha) : ColorRGBA(0.92f, 0.92f, 0.96f, Alpha));
	const float FontSize = std::min(16.0f, 16.0f * (Rect.w - 8) / std::max(1.0f, TextRender()->TextWidth(16.0f, pLabel)));
	Ui()->DoLabel(&Visual, pLabel, FontSize, TEXTALIGN_MC, Props);
	if(Hover || Focus)
	{
		CUIRect Line = {Rect.x + 16, Rect.y + Rect.h - 2, Rect.w - 32, 1};
		Line.Draw(ColorRGBA(1, 0.72f, 0.84f, Alpha * 0.6f), 0, 0);
	}
	return Enabled && (Ui()->DoButtonLogic(&m_aButtons[Index], 0, &Rect, BUTTONFLAG_LEFT) || (Focus && Activate));
}

void CLucentMenu::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);
	const double Now = static_cast<double>(time_get()) / time_freq();
	const bool Animate = g_Config.m_ClLucentLogoAnimation && (g_Config.m_GfxRefreshRate == 0 || g_Config.m_GfxRefreshRate >= 30);
	if(m_LastFrame < 0 || Now - m_LastFrame > 1.0)
	{
		m_Interaction.Reset();
		m_Progress = 0;
		m_Focus = -1;
		m_Extras = false;
	}
	const float Dt = m_LastFrame < 0 ? 0 : static_cast<float>(std::min(Now - m_LastFrame, 0.05));
	m_LastFrame = Now;
	const float CenterX = MainView.x + MainView.w / 2;
	const float CenterY = MainView.y + MainView.h * 0.43f;
	const float LogoWidth = std::min(400.0f, MainView.w * 0.43f);
	const float Scale = LogoWidth / 400;
	CUIRect Logo = {CenterX - 180 * Scale, CenterY - 44 * Scale, 360 * Scale, 88 * Scale};
	const float Width = std::min(180.0f, MainView.w * 0.23f);
	const float Gap = MainView.w * 0.035f;
	CUIRect Left = {Logo.x - Gap - Width, CenterY - 63, Width, 126};
	CUIRect Right = {Logo.x + Logo.w + Gap, Left.y, Width, Left.h};
	CUIRect aRects[6];
	for(int i = 0; i < 6; ++i)
		aRects[i] = {i < 3 ? Left.x : Right.x, Left.y + (i % 3) * 42, Width, 42};
	CUIRect More = {CenterX - 90, Logo.y + Logo.h, 180, 30};
	CUIRect aExtra[5];
	for(int i = 0; i < 5; ++i)
		aExtra[i] = {CenterX - 90, More.y + More.h + i * 26, 180, 26};
	const vec2 Mouse(Ui()->MouseX(), Ui()->MouseY());
	if(distance(Mouse, m_LastMouse) > 1.0f)
	{
		m_Focus = -1;
		m_LastMouse = Mouse;
	}
	bool Keyboard = false;
	const int Count = m_Extras ? 12 : 7;
	if(Ui()->ConsumeHotkey(CUi::HOTKEY_TAB) || Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN))
	{
		m_Focus = Input()->ShiftIsPressed() ? (m_Focus < 0 ? Count - 1 : (m_Focus + Count - 1) % Count) : (m_Focus + 1) % Count;
		Keyboard = true;
	}
	if(Ui()->ConsumeHotkey(CUi::HOTKEY_UP))
	{
		m_Focus = m_Focus < 0 ? Count - 1 : (m_Focus + Count - 1) % Count;
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
	const bool Enter = Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
	const bool Escape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE);
	if(Escape)
	{
		if(m_Interaction.IsOpen())
		{
			m_Interaction.Close();
			m_Focus = -1;
		}
		else
			GameClient()->m_Menus.ShowQuitPopup();
	}
	bool OverControl = false;
	if(m_Interaction.IsOpen())
	{
		for(const auto &Rect : aRects)
			OverControl |= CLucentMenuInteraction::Contains(Rect, Mouse);
		OverControl |= CLucentMenuInteraction::Contains(More, Mouse);
		if(m_Extras)
			for(const auto &Rect : aExtra)
				OverControl |= CLucentMenuInteraction::Contains(Rect, Mouse);
	}
	const bool Open = m_Interaction.Update(Now, Mouse, Logo, Left, Right, OverControl, Keyboard || m_Focus >= 0);
	const float Target = Open ? 1.0f : 0.0f;
	m_Progress = Animate ? std::clamp(m_Progress + (Open ? Dt : -Dt) / 0.35f, 0.0f, 1.0f) : Target;
	if(!Open && m_Progress <= 0)
		m_Extras = false;
	// Reuse cached logo text and brighten the existing orbit on hover.
	const float Activation = 1 - std::pow(1 - m_Progress, 3.0f);
	m_Logo.Render(Graphics(), TextRender(), CenterX, CenterY - 29 * Scale, LogoWidth, Activation);
	bool aClick[6] = {};
	const char *apLabels[] = {Localize("Play", "Start menu"), Localize("Demos"), Localize("Editor"), Localize("Run server"), Localize("Settings"), Localize("Quit")};
	const bool Running = GameClient()->m_LocalServer.IsServerRunning();
	for(int i = 0; i < 6; ++i)
	{
		const float T = Animate ? std::clamp((m_Progress * 0.35f - (i % 3) * 0.035f) / 0.28f, 0.0f, 1.0f) : m_Progress;
		const float Ease = 1 - std::pow(1 - T, 3.0f);
		const float Offset = (i < 3 ? 1 : -1) * 20 * (1 - Ease);
		// Stable targets: only the label moves, never the clickable rectangle.
		if(i == 3 && Running)
		{
			CUIRect Join = aRects[i], Stop = aRects[i];
			Join.w *= 0.5f;
			Stop.x += Join.w;
			Stop.w = Join.w;
			if(Button(3, Localize("Join"), Join, Ease, Open, Enter))
				GameClient()->m_LocalServer.Connect();
			if(Button(12, Localize("Stop server"), Stop, Ease, Open, false))
				GameClient()->m_LocalServer.KillServer();
		}
		else
		{
			aClick[i] = Button(i, apLabels[i], aRects[i], Ease, Open, Enter, Offset);
		}
	}
	int NewPage = -1;
	if(aClick[0] || (Enter && m_Focus < 0) || CheckHotKey(KEY_P))
		NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	if(aClick[1] || CheckHotKey(KEY_D))
		NewPage = CMenus::PAGE_DEMOS;
	if(aClick[2] || CheckHotKey(KEY_E))
	{
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
	}
	if(CheckHotKey(KEY_J) && Running)
		GameClient()->m_LocalServer.Connect();
	if(aClick[3] || CheckHotKey(KEY_R))
	{
		if(Running)
			GameClient()->m_LocalServer.KillServer();
		else
			GameClient()->m_LocalServer.RunServer({});
	}
	if(aClick[4] || CheckHotKey(KEY_S))
		NewPage = CMenus::PAGE_SETTINGS;
	if(aClick[5] || CheckHotKey(KEY_Q))
	{
		if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
			GameClient()->m_Menus.ShowQuitPopup();
		else
			Client()->Quit();
	}
	if(Button(6, Localize("More"), More, Activation, Open, Enter))
		m_Extras = !m_Extras;
	const char *apExtra[] = {Localize("News"), Localize("Website"), Localize("Tutorial"), Localize("Learn"), Localize("Discord")};
	for(int i = 0; m_Extras && i < 5; ++i)
	{
		if(!Button(7 + i, apExtra[i], aExtra[i], Activation, Open, Enter))
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
	if(m_Extras)
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
			str_copy(aBuf, Localize("DDNet Client updated!"));
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
		m_Focus = -1;
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

bool CLucentMenu::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && Input()->KeyPress(Key) && !GameClient()->m_GameConsole.IsActive();
}
