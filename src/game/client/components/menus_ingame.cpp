/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ghost.h"
#include "menus.h"
#include "motd.h"
#include "voting.h"

#include <base/color.h>
#include <base/dbg.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/demo.h>
#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/friends.h>
#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/touch_controls.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/gameview/presentation.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/panel.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/editor/editor.h>
#include <game/localization.h>

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

void CMenus::RenderGame(CUIRect MainView)
{
	CUIRect Button, ButtonBars, ButtonBar, ButtonBar2;
	bool ShowDDRaceButtons = MainView.w > 855.0f;
	MainView.HSplitTop(45.0f + (g_Config.m_ClTouchControls ? 35.0f : 0.0f), &ButtonBars, &MainView);
	ButtonBars.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	ButtonBars.Margin(10.0f, &ButtonBars);
	ButtonBars.HSplitTop(25.0f, &ButtonBar, &ButtonBars);
	if(g_Config.m_ClTouchControls)
	{
		ButtonBars.HSplitTop(10.0f, nullptr, &ButtonBars);
		ButtonBars.HSplitTop(25.0f, &ButtonBar2, &ButtonBars);
	}

	ButtonBar.VSplitRight(120.0f, &ButtonBar, &Button);
	static CButtonContainer s_DisconnectButton;
	if(DoButton_Menu(&s_DisconnectButton, Localize("Disconnect"), 0, &Button))
	{
		if((GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0) ||
			GameClient()->m_TouchControls.HasEditingChanges() ||
			GameClient()->m_Menus.m_MenusIngameTouchControls.UnsavedChanges())
		{
			char aBuf[256] = {'\0'};
			if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				str_copy(aBuf, Localize("Are you sure that you want to disconnect?"));
			}
			if(GameClient()->m_TouchControls.HasEditingChanges() ||
				GameClient()->m_Menus.m_MenusIngameTouchControls.UnsavedChanges())
			{
				if(aBuf[0] != '\0')
				{
					str_append(aBuf, "\n\n");
				}
				str_append(aBuf, Localize("There's an unsaved change in the touch controls editor, you might want to save it."));
			}
			PopupConfirm(Localize("Disconnect"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnect);
		}
		else
		{
			Client()->Disconnect();
			RefreshBrowserTab(true);
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(170.0f, &ButtonBar, &Button);

	static CButtonContainer s_DummyButton;
	if(!Client()->DummyAllowed())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connect Dummy"), 1, &Button);
		Ui()->DoToolTip(&s_DummyButton, &Button, Localize("Dummy is not allowed on this server"));
	}
	else if(Client()->DummyConnectingDelayed())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connect Dummy"), 1, &Button);
		Ui()->DoToolTip(&s_DummyButton, &Button, Localize("Please wait…"));
	}
	else if(Client()->DummyConnecting())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connecting dummy"), 1, &Button);
	}
	else if(DoButton_Menu(&s_DummyButton, Client()->DummyConnected() ? Localize("Disconnect Dummy") : Localize("Connect Dummy"), 0, &Button))
	{
		if(!Client()->DummyConnected())
		{
			Client()->DummyConnect();
		}
		else
		{
			if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				PopupConfirm(Localize("Disconnect Dummy"), Localize("Are you sure that you want to disconnect your dummy?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnectDummy);
			}
			else
			{
				Client()->DummyDisconnect(nullptr);
				SetActive(false);
			}
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(140.0f, &ButtonBar, &Button);
	static CButtonContainer s_DemoButton;
	const bool Recording = DemoRecorder(RECORDER_MANUAL)->IsRecording();
	if(DoButton_Menu(&s_DemoButton, Recording ? Localize("Stop record") : Localize("Record demo"), 0, &Button))
	{
		if(!Recording)
			Client()->DemoRecorder_Start(GameClient()->Map()->BaseName(), true, RECORDER_MANUAL);
		else
			Client()->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
	}

	bool Paused = false;
	bool Spec = false;
	if(GameClient()->m_Snap.m_LocalClientId >= 0)
	{
		Paused = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Paused;
		Spec = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Spec;
	}

	if(GameClient()->m_Snap.m_pLocalInfo && GameClient()->m_Snap.m_pGameInfoObj && !Paused && !Spec)
	{
		if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
			static CButtonContainer s_SpectateButton;
			if(!Client()->DummyConnecting() && DoButton_Menu(&s_SpectateButton, Localize("Spectate"), 0, &Button))
			{
				if(g_Config.m_ClDummy == 0 || Client()->DummyConnected())
				{
					GameClient()->SendSwitchTeam(TEAM_SPECTATORS);
					SetActive(false);
				}
			}
		}

		if(GameClient()->IsTeamPlay())
		{
			if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_RED)
			{
				ButtonBar.VSplitLeft(100.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinRedButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinRedButton, Localize("Join red"), 0, &Button))
				{
					GameClient()->SendSwitchTeam(TEAM_RED);
					SetActive(false);
				}
			}

			if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_BLUE)
			{
				ButtonBar.VSplitLeft(100.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinBlueButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinBlueButton, Localize("Join blue"), 0, &Button))
				{
					GameClient()->SendSwitchTeam(TEAM_BLUE);
					SetActive(false);
				}
			}
		}
		else
		{
			if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_GAME)
			{
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinGameButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinGameButton, Localize("Join game"), 0, &Button))
				{
					GameClient()->SendSwitchTeam(TEAM_GAME);
					SetActive(false);
				}
			}
		}

		if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && (ShowDDRaceButtons || !GameClient()->IsTeamPlay()))
		{
			ButtonBar.VSplitLeft(65.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

			static CButtonContainer s_KillButton;
			if(DoButton_Menu(&s_KillButton, Localize("Kill"), 0, &Button))
			{
				GameClient()->SendKill();
				SetActive(false);
			}
		}
	}

	if(GameClient()->m_ReceivedDDNetPlayer && GameClient()->m_Snap.m_pLocalInfo && (ShowDDRaceButtons || !GameClient()->IsTeamPlay()))
	{
		if(GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS || Paused || Spec)
		{
			ButtonBar.VSplitLeft((!Paused && !Spec) ? 65.0f : 120.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

			static CButtonContainer s_PauseButton;
			if(DoButton_Menu(&s_PauseButton, (!Paused && !Spec) ? Localize("Pause") : Localize("Join game"), 0, &Button))
			{
				Console()->ExecuteLine("say /pause", IConsole::CLIENT_ID_UNSPECIFIED);
				SetActive(false);
			}
		}
	}

	if(GameClient()->m_Snap.m_pLocalInfo && (GameClient()->m_Snap.m_pLocalInfo->m_Team == TEAM_SPECTATORS || Paused || Spec))
	{
		ButtonBar.VSplitLeft(32.0f, &Button, &ButtonBar);
		ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

		static CButtonContainer s_AutoCameraButton;

		bool Active = GameClient()->m_Camera.m_AutoSpecCamera && GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera();
		bool Enabled = g_Config.m_ClSpecAutoSync;
		if(Ui()->DoButton_FontIcon(&s_AutoCameraButton, FontIcon::CAMERA, !Active, &Button, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, Enabled))
		{
			GameClient()->m_Camera.ToggleAutoSpecCamera();
		}
		GameClient()->m_Camera.UpdateAutoSpecCameraTooltip();
		Ui()->DoToolTip(&s_AutoCameraButton, &Button, GameClient()->m_Camera.AutoSpecCameraTooltip());
	}

	if(g_Config.m_ClTouchControls)
	{
		ButtonBar2.VSplitLeft(200.0f, &Button, &ButtonBar2);
		static char s_TouchControlsEditCheckbox;
		if(DoButton_CheckBox(&s_TouchControlsEditCheckbox, Localize("Edit touch controls"), GameClient()->m_TouchControls.IsEditingActive(), &Button))
		{
			if(GameClient()->m_TouchControls.IsEditingActive() && m_MenusIngameTouchControls.UnsavedChanges())
			{
				m_MenusIngameTouchControls.m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
				m_MenusIngameTouchControls.m_pNewSelectedButton = nullptr;
				PopupConfirm(Localize("Unsaved changes"), Localize("Save all changes before turning off the editor?"), Localize("Save"), Localize("Cancel"), &CMenus::PopupConfirmTurnOffEditor);
			}
			else
			{
				GameClient()->m_TouchControls.SetEditingActive(!GameClient()->m_TouchControls.IsEditingActive());
				if(GameClient()->m_TouchControls.IsEditingActive())
				{
					GameClient()->m_TouchControls.ResetVirtualVisibilities();
					m_MenusIngameTouchControls.m_EditElement = CMenusIngameTouchControls::EElementType::LAYOUT;
				}
				else
				{
					m_MenusIngameTouchControls.ResetButtonPointers();
				}
			}
		}

		ButtonBar2.VSplitRight(80.0f, &ButtonBar2, &Button);
		static CButtonContainer s_CloseButton;
		if(DoButton_Menu(&s_CloseButton, Localize("Close"), 0, &Button))
		{
			SetActive(false);
		}

		ButtonBar2.VSplitRight(5.0f, &ButtonBar2, nullptr);
		ButtonBar2.VSplitRight(160.0f, &ButtonBar2, &Button);
		static CButtonContainer s_RemoveConsoleButton;
		if(DoButton_Menu(&s_RemoveConsoleButton, Localize("Remote console"), 0, &Button))
		{
			Console()->ExecuteLine("toggle_remote_console", IConsole::CLIENT_ID_UNSPECIFIED);
		}

		ButtonBar2.VSplitRight(5.0f, &ButtonBar2, nullptr);
		ButtonBar2.VSplitRight(120.0f, &ButtonBar2, &Button);
		static CButtonContainer s_LocalConsoleButton;
		if(DoButton_Menu(&s_LocalConsoleButton, Localize("Console"), 0, &Button))
		{
			Console()->ExecuteLine("toggle_local_console", IConsole::CLIENT_ID_UNSPECIFIED);
		}
		// Only when these are all false, the preview page is rendered. Once the page is not rendered, update is needed upon next rendering.
		if(!GameClient()->m_TouchControls.IsEditingActive() || m_MenusIngameTouchControls.m_CurrentMenu != CMenusIngameTouchControls::EMenuType::MENU_BUTTONS || GameClient()->m_TouchControls.IsButtonEditing())
			m_MenusIngameTouchControls.m_NeedUpdatePreview = true;
		// Quit preview all buttons automatically.
		if(!GameClient()->m_TouchControls.IsEditingActive() || m_MenusIngameTouchControls.m_CurrentMenu != CMenusIngameTouchControls::EMenuType::MENU_PREVIEW)
			GameClient()->m_TouchControls.SetPreviewAllButtons(false);
		if(GameClient()->m_TouchControls.IsEditingActive())
		{
			// Resolve issues if needed before rendering, so the elements could have a correct value on this frame.
			// Issues need to be resolved before popup. So CheckCachedSettings could not be bad.
			m_MenusIngameTouchControls.ResolveIssues();
			// Do Popups if needed.
			CTouchControls::CPopupParam PopupParam = GameClient()->m_TouchControls.RequiredPopup();
			if(PopupParam.m_PopupType != CTouchControls::EPopupType::NUM_POPUPS)
			{
				m_MenusIngameTouchControls.DoPopupType(PopupParam);
				return;
			}
			if(m_MenusIngameTouchControls.m_FirstEnter)
			{
				m_MenusIngameTouchControls.m_aCachedVisibilities[(int)CTouchControls::EButtonVisibility::DEMO_PLAYER] = CMenusIngameTouchControls::EVisibilityType::EXCLUDE;
				m_MenusIngameTouchControls.m_ColorActive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorActive()).Pack(true);
				m_MenusIngameTouchControls.m_ColorInactive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorInactive()).Pack(true);
				m_MenusIngameTouchControls.m_FirstEnter = false;
			}
			// Their width is all 505.0f, height is adjustable, you can directly change its h value, so no need for changing where tab is.
			CUIRect SelectingTab;
			MainView.HSplitTop(40.0f, nullptr, &MainView);
			MainView.VMargin((MainView.w - CMenusIngameTouchControls::BUTTON_EDITOR_WIDTH) / 2.0f, &MainView);
			MainView.HSplitTop(25.0f, &SelectingTab, &MainView);

			m_MenusIngameTouchControls.RenderSelectingTab(SelectingTab);
			switch(m_MenusIngameTouchControls.m_CurrentMenu)
			{
			case CMenusIngameTouchControls::EMenuType::MENU_FILE: m_MenusIngameTouchControls.RenderTouchControlsEditor(MainView); break;
			case CMenusIngameTouchControls::EMenuType::MENU_BUTTONS: m_MenusIngameTouchControls.RenderTouchButtonEditor(MainView); break;
			case CMenusIngameTouchControls::EMenuType::MENU_SETTINGS: m_MenusIngameTouchControls.RenderConfigSettings(MainView); break;
			case CMenusIngameTouchControls::EMenuType::MENU_PREVIEW: m_MenusIngameTouchControls.RenderPreviewSettings(MainView); break;
			default: dbg_assert_failed("Unknown selected tab value = %d.", (int)m_MenusIngameTouchControls.m_CurrentMenu);
			}
		}
	}
}

void CMenus::PopupConfirmDisconnect()
{
	Client()->Disconnect();
}

void CMenus::PopupConfirmDisconnectDummy()
{
	Client()->DummyDisconnect(nullptr);
	SetActive(false);
}

void CMenus::PopupConfirmDiscardTouchControlsChanges()
{
	if(GameClient()->m_TouchControls.LoadConfigurationFromFile(IStorage::TYPE_ALL))
	{
		m_MenusIngameTouchControls.m_ColorActive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorActive()).Pack(true);
		m_MenusIngameTouchControls.m_ColorInactive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorInactive()).Pack(true);
		GameClient()->m_TouchControls.SetEditingChanges(false);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load touch controls from file. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::PopupConfirmResetTouchControls()
{
	bool Success = false;
	for(int StorageType = IStorage::TYPE_SAVE + 1; StorageType < Storage()->NumPaths(); ++StorageType)
	{
		if(GameClient()->m_TouchControls.LoadConfigurationFromFile(StorageType))
		{
			Success = true;
			break;
		}
	}
	if(Success)
	{
		m_MenusIngameTouchControls.m_ColorActive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorActive()).Pack(true);
		m_MenusIngameTouchControls.m_ColorInactive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorInactive()).Pack(true);
		GameClient()->m_TouchControls.SetEditingChanges(true);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load default touch controls from file. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::PopupConfirmImportTouchControlsClipboard()
{
	if(GameClient()->m_TouchControls.LoadConfigurationFromClipboard())
	{
		m_MenusIngameTouchControls.m_ColorActive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorActive()).Pack(true);
		m_MenusIngameTouchControls.m_ColorInactive = color_cast<ColorHSLA>(GameClient()->m_TouchControls.BackgroundColorInactive()).Pack(true);
		GameClient()->m_TouchControls.SetEditingChanges(true);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load touch controls from clipboard. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::PopupConfirmDeleteButton()
{
	GameClient()->m_TouchControls.DeleteSelectedButton();
	m_MenusIngameTouchControls.ResetCachedSettings();
	GameClient()->m_TouchControls.SetEditingChanges(true);
}

void CMenus::PopupCancelDeselectButton()
{
	m_MenusIngameTouchControls.ResetButtonPointers();
	m_MenusIngameTouchControls.SetUnsavedChanges(false);
	m_MenusIngameTouchControls.ResetCachedSettings();
}

void CMenus::PopupConfirmSelectedNotVisible()
{
	if(m_MenusIngameTouchControls.UnsavedChanges())
	{
		// The m_pSelectedButton can't nullptr, because this function is triggered when selected button not visible.
		m_MenusIngameTouchControls.m_pOldSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		m_MenusIngameTouchControls.m_pNewSelectedButton = nullptr;
		m_MenusIngameTouchControls.m_CloseMenu = true;
		m_MenusIngameTouchControls.ChangeSelectedButtonWhileHavingUnsavedChanges();
	}
	else
	{
		m_MenusIngameTouchControls.ResetButtonPointers();
		GameClient()->m_Menus.SetActive(false);
	}
}

void CMenus::PopupConfirmChangeSelectedButton()
{
	if(m_MenusIngameTouchControls.CheckCachedSettings())
	{
		GameClient()->m_TouchControls.SetSelectedButton(m_MenusIngameTouchControls.m_pNewSelectedButton);
		m_MenusIngameTouchControls.SaveCachedSettingsToTarget(m_MenusIngameTouchControls.m_pOldSelectedButton);
		// Update wild pointer.
		if(m_MenusIngameTouchControls.m_pNewSelectedButton != nullptr)
			m_MenusIngameTouchControls.m_pNewSelectedButton = GameClient()->m_TouchControls.SelectedButton();
		GameClient()->m_TouchControls.SetEditingChanges(true);
		m_MenusIngameTouchControls.SetUnsavedChanges(false);
		PopupCancelChangeSelectedButton();
	}
}

void CMenus::PopupCancelChangeSelectedButton()
{
	GameClient()->m_TouchControls.SetSelectedButton(m_MenusIngameTouchControls.m_pNewSelectedButton);
	m_MenusIngameTouchControls.CacheAllSettingsFromTarget(m_MenusIngameTouchControls.m_pNewSelectedButton);
	m_MenusIngameTouchControls.SetUnsavedChanges(false);
	if(m_MenusIngameTouchControls.m_pNewSelectedButton != nullptr)
	{
		m_MenusIngameTouchControls.UpdateSampleButton();
	}
	else
	{
		m_MenusIngameTouchControls.ResetButtonPointers();
	}
	if(m_MenusIngameTouchControls.m_CloseMenu)
		GameClient()->m_Menus.SetActive(false);
}

void CMenus::PopupConfirmTurnOffEditor()
{
	if(m_MenusIngameTouchControls.CheckCachedSettings())
	{
		m_MenusIngameTouchControls.SaveCachedSettingsToTarget(m_MenusIngameTouchControls.m_pOldSelectedButton);
		GameClient()->m_TouchControls.SetEditingActive(!GameClient()->m_TouchControls.IsEditingActive());
		m_MenusIngameTouchControls.ResetButtonPointers();
	}
}

void CMenus::PopupConfirmOpenWiki()
{
	Client()->ViewLink(Localize("https://wiki.ddnet.org/wiki/Touch_controls"));
}

void CMenus::RenderPlayers(CUIRect MainView)
{
	CUIRect Button, Button2, ButtonBar, PlayerList, Player;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	// list background color
	MainView.Margin(10.0f, &PlayerList);
	PlayerList.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	PlayerList.Margin(10.0f, &PlayerList);

	// headline
	PlayerList.HSplitTop(34.0f, &ButtonBar, &PlayerList);
	ButtonBar.VSplitRight(231.0f, &Player, &ButtonBar);
	Ui()->DoLabel(&Player, Localize("Player"), 24.0f, TEXTALIGN_ML);

	ButtonBar.HMargin(1.0f, &ButtonBar);
	float Width = ButtonBar.h * 2.0f;
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_EMOTICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_FRIEND, &Button);

	int TotalPlayers = 0;
	for(const auto &pInfoByName : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfoByName)
			continue;

		int Index = pInfoByName->m_ClientId;

		if(Index == GameClient()->m_Snap.m_LocalClientId)
			continue;

		TotalPlayers++;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(24.0f, TotalPlayers, 1, 3, -1, &PlayerList);

	// options
	static char s_aPlayerIds[MAX_CLIENTS][4] = {{0}};

	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_Snap.m_apInfoByName[i])
			continue;

		int Index = GameClient()->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == GameClient()->m_Snap.m_LocalClientId)
			continue;

		CGameClient::CClientData &CurrentClient = GameClient()->m_aClients[Index];
		const CListboxItem Item = s_ListBox.DoNextItem(&CurrentClient);

		Count++;

		if(!Item.m_Visible)
			continue;

		CUIRect Row = Item.m_Rect;
		if(Count % 2 == 1)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
		Row.VSplitRight(s_ListBox.ScrollbarWidthMax() - s_ListBox.ScrollbarWidth(), &Row, nullptr);
		Row.VSplitRight(300.0f, &Player, &Row);

		// player info
		Player.VSplitLeft(28.0f, &Button, &Player);

		CTeeRenderInfo TeeInfo = CurrentClient.m_RenderInfo;
		TeeInfo.m_Size = Button.h;

		RenderTools()->RenderIdleTeeAtCenter(&TeeInfo, vec2(Button.x + Button.h / 2, Button.y + Button.h / 2));
		Ui()->DoButtonLogic(&s_aPlayerIds[Index][3], 0, &Button, BUTTONFLAG_NONE);
		Ui()->DoToolTip(&s_aPlayerIds[Index][3], &Button, CurrentClient.m_aSkinName);

		Player.HSplitTop(1.5f, nullptr, &Player);
		Player.VSplitMid(&Player, &Button);
		Row.VSplitRight(210.0f, &Button2, &Row);

		Ui()->DoLabel(&Player, CurrentClient.m_aName, 14.0f, TEXTALIGN_ML);
		Ui()->DoLabel(&Button, CurrentClient.m_aClan, 14.0f, TEXTALIGN_ML);

		GameClient()->m_CountryFlags.Render(CurrentClient.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f),
			Button2.x, Button2.y + Button2.h / 2.0f - 0.75f * Button2.h / 2.0f, 1.5f * Button2.h, 0.75f * Button2.h);

		// ignore chat button
		Row.HMargin(2.0f, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(g_Config.m_ClShowChatFriends && !CurrentClient.m_Friend)
			DoButton_Toggle(&s_aPlayerIds[Index][0], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIds[Index][0], CurrentClient.m_ChatIgnore, &Button, true))
			CurrentClient.m_ChatIgnore ^= 1;

		// ignore emoticon button
		Row.VSplitLeft(30.0f, nullptr, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(g_Config.m_ClShowChatFriends && !CurrentClient.m_Friend)
			DoButton_Toggle(&s_aPlayerIds[Index][1], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIds[Index][1], CurrentClient.m_EmoticonIgnore, &Button, true))
			CurrentClient.m_EmoticonIgnore ^= 1;

		// friend button
		Row.VSplitLeft(10.0f, nullptr, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(DoButton_Toggle(&s_aPlayerIds[Index][2], CurrentClient.m_Friend, &Button, true))
		{
			if(CurrentClient.m_Friend)
				GameClient()->Friends()->RemoveFriend(CurrentClient.m_aName, CurrentClient.m_aClan);
			else
				GameClient()->Friends()->AddFriend(CurrentClient.m_aName, CurrentClient.m_aClan);

			GameClient()->Client()->ServerBrowserUpdate();
		}
	}

	s_ListBox.DoEnd();
}

void CMenus::RenderServerInfo(CUIRect MainView)
{
	const float FontSizeTitle = 32.0f;
	const float FontSizeBody = 20.0f;

	const CServerInfo &CurrentServerInfo = Client()->ServerInfo();

	CUIRect ServerInfo, GameInfo, Motd;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitMid(&ServerInfo, &Motd, 10.0f);
	ServerInfo.VSplitMid(&ServerInfo, &GameInfo, 10.0f);

	ServerInfo.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	ServerInfo.Margin(10.0f, &ServerInfo);

	CUIRect Label;
	ServerInfo.HSplitTop(FontSizeTitle, &Label, &ServerInfo);
	ServerInfo.HSplitTop(5.0f, nullptr, &ServerInfo);
	Ui()->DoLabel(&Label, Localize("Server info"), FontSizeTitle, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	ServerInfo.HSplitTop(FontSizeBody, nullptr, &ServerInfo);
	Ui()->DoLabel(&Label, CurrentServerInfo.m_aName, FontSizeBody, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Address"), CurrentServerInfo.m_aAddress);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	if(GameClient()->m_Snap.m_pLocalInfo)
	{
		ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Ping"), GameClient()->m_Snap.m_pLocalInfo->m_Latency);
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
	}

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Version"), CurrentServerInfo.m_aVersion);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Password"), CurrentServerInfo.m_Flags & SERVER_FLAG_PASSWORD ? Localize("Yes") : Localize("No"));
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	const CCommunity *pCommunity = ServerBrowser()->Community(CurrentServerInfo.m_aCommunityId);
	if(pCommunity != nullptr)
	{
		ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Community"));
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

		const CCommunityIcon *pIcon = m_CommunityIcons.Find(pCommunity->Id());
		if(pIcon != nullptr)
		{
			Label.VSplitLeft(TextRender()->TextWidth(FontSizeBody, aBuf) + 8.0f, nullptr, &Label);
			Label.VSplitLeft(2.0f * Label.h, &Label, nullptr);
			m_CommunityIcons.Render(pIcon, Label, true);
			static char s_CommunityTooltipButtonId;
			Ui()->DoButtonLogic(&s_CommunityTooltipButtonId, 0, &Label, BUTTONFLAG_NONE);
			Ui()->DoToolTip(&s_CommunityTooltipButtonId, &Label, pCommunity->Name());
		}
	}

	// copy info button
	{
		CUIRect Button;
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		Button.VSplitRight(200.0f, &ServerInfo, &Button);
		static CButtonContainer s_CopyButton;
		if(DoButton_Menu(&s_CopyButton, Localize("Copy info"), 0, &Button))
		{
			char aInfo[256];
			str_format(
				aInfo,
				sizeof(aInfo),
				"%s\n"
				"Address: ddnet://%s\n"
				"My IGN: %s\n",
				CurrentServerInfo.m_aName,
				CurrentServerInfo.m_aAddress,
				Client()->PlayerName());
			Input()->SetClipboardText(aInfo);
		}
	}

	// favorite checkbox
	{
		CUIRect Button;
		TRISTATE IsFavorite = Favorites()->IsFavorite(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		static int s_AddFavButton = 0;
		if(DoButton_CheckBox(&s_AddFavButton, Localize("Favorite"), IsFavorite != TRISTATE::NONE, &Button))
		{
			if(IsFavorite != TRISTATE::NONE)
				Favorites()->Remove(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
			else
				Favorites()->Add(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
		}
	}

	GameInfo.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	GameInfo.Margin(10.0f, &GameInfo);

	GameInfo.HSplitTop(FontSizeTitle, &Label, &GameInfo);
	GameInfo.HSplitTop(5.0f, nullptr, &GameInfo);
	Ui()->DoLabel(&Label, Localize("Game info"), FontSizeTitle, TEXTALIGN_ML);

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Game type"), CurrentServerInfo.m_aGameType);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Map"), CurrentServerInfo.m_aMap);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	const auto *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	if(pGameInfoObj)
	{
		if(pGameInfoObj->m_ScoreLimit)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), pGameInfoObj->m_ScoreLimit);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}

		if(pGameInfoObj->m_TimeLimit)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), pGameInfoObj->m_TimeLimit);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}

		if(pGameInfoObj->m_RoundCurrent && pGameInfoObj->m_RoundNum)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), Localize("Round %d/%d"), pGameInfoObj->m_RoundCurrent, pGameInfoObj->m_RoundNum);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}
	}

	if(GameClient()->m_GameInfo.m_DDRaceTeam)
	{
		const char *pTeamMode = nullptr;
		switch(Config()->m_SvTeam)
		{
		case SV_TEAM_FORBIDDEN:
			pTeamMode = Localize("forbidden", "Team status");
			break;
		case SV_TEAM_ALLOWED:
			if(g_Config.m_SvSoloServer)
				pTeamMode = Localize("solo", "Team status");
			else
				pTeamMode = Localize("allowed", "Team status");
			break;
		case SV_TEAM_MANDATORY:
			pTeamMode = Localize("required", "Team status");
			break;
		case SV_TEAM_FORCED_SOLO:
			pTeamMode = Localize("solo", "Team status");
			break;
		default:
			dbg_assert_failed("unknown team mode");
		}
		int MinTeamSize = GameClient()->MinTeamSize();
		int MaxTeamSize = GameClient()->MaxTeamSize();
		if((Config()->m_SvTeam == SV_TEAM_ALLOWED || Config()->m_SvTeam == SV_TEAM_MANDATORY) && (MinTeamSize != DefaultConfig::SvMinTeamSize || MaxTeamSize != DefaultConfig::SvMaxTeamSize))
		{
			if(MinTeamSize != DefaultConfig::SvMinTeamSize && MaxTeamSize != DefaultConfig::SvMaxTeamSize)
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d, %s %d)", Localize("Teams"), pTeamMode, Localize("minimum", "Team size"), MinTeamSize, Localize("maximum", "Team size"), MaxTeamSize);
			else if(MinTeamSize != DefaultConfig::SvMinTeamSize)
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d)", Localize("Teams"), pTeamMode, Localize("minimum", "Team size"), MinTeamSize);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d)", Localize("Teams"), pTeamMode, Localize("maximum", "Team size"), MaxTeamSize);
		}
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Teams"), pTeamMode);
		GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
	}

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %d/%d", Localize("Players"), GameClient()->m_Snap.m_NumPlayers, CurrentServerInfo.m_MaxClients);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	RenderServerInfoMotd(Motd);
}

void CMenus::RenderServerInfoMotd(CUIRect Motd)
{
	const float MotdFontSize = 16.0f;
	Motd.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Motd.Margin(10.0f, &Motd);

	CUIRect MotdHeader;
	Motd.HSplitTop(2.0f * MotdFontSize, &MotdHeader, &Motd);
	Motd.HSplitTop(5.0f, nullptr, &Motd);
	Ui()->DoLabel(&MotdHeader, Localize("MOTD"), 2.0f * MotdFontSize, TEXTALIGN_ML);

	if(!GameClient()->m_Motd.ServerMotd()[0])
		return;

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 5 * MotdFontSize;
	s_ScrollRegion.Begin(&Motd, &ScrollParams);

	static float s_MotdHeight = 0.0f;
	static int64_t s_MotdLastUpdateTime = -1;
	if(!m_MotdTextContainerIndex.Valid() || s_MotdLastUpdateTime == -1 || s_MotdLastUpdateTime != GameClient()->m_Motd.ServerMotdUpdateTime())
	{
		CTextCursor Cursor;
		Cursor.m_FontSize = MotdFontSize;
		Cursor.m_LineWidth = Motd.w;
		TextRender()->RecreateTextContainer(m_MotdTextContainerIndex, &Cursor, GameClient()->m_Motd.ServerMotd());
		s_MotdHeight = Cursor.Height();
		s_MotdLastUpdateTime = GameClient()->m_Motd.ServerMotdUpdateTime();
	}

	CUIRect MotdTextArea;
	Motd.HSplitTop(s_MotdHeight, &MotdTextArea, &Motd);
	s_ScrollRegion.AddRect(MotdTextArea);

	if(m_MotdTextContainerIndex.Valid())
		TextRender()->RenderTextContainer(m_MotdTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), MotdTextArea.x, MotdTextArea.y);

	s_ScrollRegion.End();
}

namespace
{
	const char *VoteLocalize(const char *pText)
	{
		return Localize(pText, "Lucent Voting");
	}

	void VoteLabel(CUi *pUi, ITextRender *pTextRender, const CUIRect &Rect, const char *pText, float Size, ColorRGBA Color, int Align = TEXTALIGN_ML, bool Fit = false)
	{
		if(Rect.w <= 0 || Rect.h <= 0)
			return;
		SLabelProperties Props;
		Props.m_MaxWidth = Fit ? -1.f : Rect.w;
		Props.m_EllipsisAtEnd = !Fit;
		Props.m_EnableWidthCheck = Fit;
		if(Fit)
			Props.m_MinimumFontSize = 1;
		const auto Previous = pTextRender->GetTextColor();
		pTextRender->TextColor(Color);
		pUi->DoLabel(&Rect, pText, Size, Align, Props);
		pTextRender->TextColor(Previous);
	}

	void VoteField(CUi *pUi, ITextRender *pTextRender, const CUIRect &Rect, std::string_view Text, float Size, ColorRGBA Color, int Align = TEXTALIGN_ML, bool Fit = false)
	{
		if(Text.empty())
			return;
		char aText[VOTE_DESC_LENGTH] = {};
		Text.copy(aText, sizeof(aText) - 1);
		VoteLabel(pUi, pTextRender, Rect, aText, Size, Color, Align, Fit);
	}

	CUIRect VoteCard(const CUIRect &Rect, bool Selected, bool Hover)
	{
		CUIRect Card;
		Rect.HMargin(1.5f, &Card);
		if(Selected || Hover)
			Card.Draw(LucentUi::Accent(Selected ? .19f : .075f), IGraphics::CORNER_ALL, 4);
		if(Selected)
		{
			const CUIRect Mark{Card.x + 2, Card.y + 4, 1.5f, Card.h - 8};
			Mark.Draw(LucentUi::Accent(.85f), IGraphics::CORNER_ALL, .75f);
		}
		Card.VMargin(8, &Card);
		return Card;
	}

	void VoteBadge(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, std::string_view Label, bool Accent)
	{
		if(Label.empty() || Rect.w <= 0)
			return;
		Rect.Draw(LucentUi::Accent(Accent ? .13f : .055f), IGraphics::CORNER_ALL, 3);
		Rect.VMargin(2, &Rect);
		VoteField(pUi, pTextRender, Rect, Label, 10.f, Accent ? LucentStyle::Style::PrimaryText : LucentStyle::Style::Text, TEXTALIGN_MC);
	}

	void VoteListStyle(CListBox &List, bool Reset)
	{
		if(Reset)
			List.Reset();
		List.SetScrollbarWidth(8);
		List.SetScrollbarMargin(2);
	}
}

bool CMenus::RenderServerControlServer(CUIRect List, CUIRect Sidebar, bool ResetScroll)
{
	using namespace LucentStyle::Style;
	using namespace LucentVoting;
	const auto &Voting = GameClient()->m_Voting;
	auto &Catalog = m_VotingCatalog;
	const bool Changed = m_VotingCatalogRevision != Voting.OptionRevision();
	if(Changed)
	{
		m_VotingCatalogRevision = Voting.OptionRevision();
		Catalog.Rebuild(Voting.FirstOption());
	}
	if(!Voting.IsReceivingOptions())
	{
		if(!Catalog.m_HasDecorations)
			m_RawVoteOptions = false;
		std::erase_if(m_vVoteCategories, [&](const std::string &Name) {
			return std::none_of(Catalog.m_vCategories.begin(), Catalog.m_vCategories.end(), [&](const auto &Category) { return Category.m_Count > 0 && Category.m_Name == Name; });
		});
	}

	int SidebarTarget = -1;
	if(Sidebar.h > 0)
	{
		Sidebar.Draw(Surface.WithAlpha(.88f), IGraphics::CORNER_ALL, 6);
		Sidebar.Margin(6, &Sidebar);
		CUIRect Heading;
		Sidebar.HSplitTop(18, &Heading, &Sidebar);
		Heading.VMargin(4, &Heading);
		VoteLabel(Ui(), TextRender(), Heading, VoteLocalize("Categories"), 10, PrimaryText);

		static int s_RawMode, s_All, s_Unsectioned;
		if(Catalog.m_HasDecorations)
		{
			CUIRect Mode;
			Sidebar.HSplitBottom(22, &Sidebar, &Mode);
			const char *pMode = m_RawVoteOptions ? VoteLocalize("Grouped list") : VoteLocalize("Original list");
			if(LucentUi::Button(Ui(), TextRender(), &s_RawMode, pMode, Mode, m_RawVoteOptions, true, true))
			{
				m_RawVoteOptions = !m_RawVoteOptions;
				ResetScroll = true;
			}
		}

		static CScrollRegion s_CategoryScroll;
		if(Changed)
			s_CategoryScroll.StopScrolling();
		if(!m_RawVoteOptions && Sidebar.h > 0)
		{
			const char *pCurrentType = nullptr;
			int NumTypeChoices = 0;
			for(const int Index : Catalog.m_vMapTypes)
			{
				const auto *pOption = Catalog.m_vOptions[Index].m_pOption;
				const auto Type = ParseMapType(pOption->m_aDescription);
				if(Type.m_Active)
					pCurrentType = Type.m_pName;
				else
				{
					++NumTypeChoices;
					if(pOption == m_pCallvoteSelection && str_comp(pOption->m_aDescription, m_aCallvoteSelection) == 0)
						SidebarTarget = Index;
				}
			}
			if(pCurrentType)
			{
				static int s_TypeToggle;
				CUIRect Toggle, Chevron, Badge;
				const char *pBadge = VoteLocalize("Current type");
				const float BadgeWidth = TextRender()->TextWidth(9, pBadge) + 10;
				const bool StackBadge = Sidebar.w < TextRender()->TextWidth(10.5f, pCurrentType) + BadgeWidth + 36;
				Sidebar.HSplitTop(StackBadge ? 36.f : 24.f, &Toggle, &Sidebar);
				const bool Hover = Ui()->Enabled() && Ui()->MouseHovered(&Toggle);
				CUIRect Label = VoteCard(Toggle, false, Hover);
				Label.VSplitRight(10, &Label, &Chevron);
				Label.VSplitRight(5, &Label, nullptr);
				if(StackBadge)
				{
					CUIRect BadgeLine;
					Label.HSplitBottom(16, &Label, &BadgeLine);
					BadgeLine.VSplitLeft(std::min(BadgeWidth, BadgeLine.w), &Badge, nullptr);
					Badge.HMargin(1, &Badge);
				}
				else
				{
					Label.VSplitRight(BadgeWidth, &Label, &Badge);
					Label.VSplitRight(5, &Label, nullptr);
					Badge.HMargin(3, &Badge);
				}
				Badge.Draw(LucentUi::Accent(.13f), IGraphics::CORNER_ALL, 3);
				Badge.VMargin(3, &Badge);
				VoteLabel(Ui(), TextRender(), Badge, pBadge, 9, PrimaryText, TEXTALIGN_MC);
				VoteLabel(Ui(), TextRender(), Label, pCurrentType, 10.5f, PrimaryText, TEXTALIGN_ML, true);
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				VoteLabel(Ui(), TextRender(), Chevron, m_VoteMapTypesExpanded ? FontIcon::CHEVRON_UP : FontIcon::CHEVRON_DOWN, 8, DetailText, TEXTALIGN_MC);
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				if(Hover)
					Ui()->DoToolTip(&s_TypeToggle, &Toggle, pCurrentType);
				if(Ui()->DoButtonLogic(&s_TypeToggle, m_VoteMapTypesExpanded, &Toggle, BUTTONFLAG_LEFT))
				{
					m_VoteMapTypesExpanded = !m_VoteMapTypesExpanded;
					s_CategoryScroll.StopScrolling();
				}
			}
			const auto &Clock = LucentStyle::Motion::Clock();
			const float TypeReveal = m_VoteMapTypesTransition.Update(pCurrentType && m_VoteMapTypesExpanded, Clock.m_Now, Clock.m_Animate, LucentStyle::Motion::Selection);
			constexpr float CategoryHeight = 22.f;
			const float TypeHeight = CategoryHeight * NumTypeChoices * TypeReveal;
			CScrollRegionParams Params;
			Params.m_ScrollbarThickness = 7;
			Params.m_ScrollbarMargin = 2;
			Params.m_ScrollUnit = 46;
			const int NumCategories = std::count_if(Catalog.m_vCategories.begin(), Catalog.m_vCategories.end(), [](const auto &Category) { return Category.m_Count > 0; });
			const float SidebarHeight = CategoryHeight * (1 + NumCategories) + TypeHeight + (pCurrentType ? 18.f : 0.f) + BottomPadding;
			s_CategoryScroll.Begin(&Sidebar, &Params, SidebarHeight);
			const auto DrawCategory = [&](const CUIRect &Row, const void *pId, const char *pLabel, int Count, bool Selected) {
				const bool Hover = Ui()->Enabled() && Ui()->MouseHovered(&Row);
				CUIRect Label = VoteCard(Row, Selected, Hover), Badge;
				if(Count >= 0)
				{
					char aCount[16];
					str_format(aCount, sizeof(aCount), "%d", Count);
					const float BadgeWidth = std::min(Label.w * .46f, std::max(18.f, TextRender()->TextWidth(10, aCount) + 7));
					Label.VSplitRight(BadgeWidth, &Label, &Badge);
					Label.VSplitRight(4, &Label, nullptr);
					Badge.HMargin(3, &Badge);
					VoteBadge(Ui(), TextRender(), Badge, aCount, Selected);
				}
				VoteLabel(Ui(), TextRender(), Label, pLabel, 10.5f, Selected ? PrimaryText : Text, TEXTALIGN_ML, true);
				if(Hover && TextRender()->TextWidth(10.5f, pLabel) > Label.w)
					Ui()->DoToolTip(pId, &Row, pLabel);
				return Ui()->DoButtonLogic(pId, Selected, &Row, BUTTONFLAG_LEFT) != 0;
			};
			const auto DrawFilter = [&](const void *pId, const char *pLabel, int Count, bool Selected) {
				CUIRect Row;
				Sidebar.HSplitTop(CategoryHeight, &Row, &Sidebar);
				return s_CategoryScroll.AddRect(Row) && DrawCategory(Row, pId, pLabel, Count, Selected);
			};
			const auto DrawSidebarHeading = [&](const char *pLabel) {
				CUIRect Row;
				Sidebar.HSplitTop(18, &Row, &Sidebar);
				if(s_CategoryScroll.AddRect(Row))
				{
					Row.VMargin(8, &Row);
					VoteLabel(Ui(), TextRender(), Row, pLabel, 9.5f, DetailText);
				}
			};
			if(pCurrentType)
			{
				if(TypeHeight > 0)
				{
					CUIRect Reveal;
					Sidebar.HSplitTop(TypeHeight, &Reveal, &Sidebar);
					if(s_CategoryScroll.AddRect(Reveal))
					{
						Ui()->ClipEnable(&Reveal);
						CUIRect Row{Reveal.x, Reveal.y, Reveal.w, CategoryHeight};
						for(const int Index : Catalog.m_vMapTypes)
						{
							const auto *pOption = Catalog.m_vOptions[Index].m_pOption;
							const auto Type = ParseMapType(pOption->m_aDescription);
							if(Type.m_Active)
								continue;
							if(Row.y >= Reveal.y + Reveal.h)
								break;
							if(!s_CategoryScroll.RectClipped(Row))
							{
								LucentUi::CInteractionScope Input(Ui(), m_VoteMapTypesExpanded && Row.y + Row.h <= Reveal.y + Reveal.h + .001f);
								if(DrawCategory(Row, pOption, Type.m_pName, -1, SidebarTarget == Index))
								{
									SidebarTarget = Index;
									m_pCallvoteSelection = nullptr;
								}
							}
							Row.y += CategoryHeight;
						}
						Ui()->ClipDisable();
					}
				}
				DrawSidebarHeading(VoteLocalize("Map filters"));
			}
			int Total = 0;
			for(const auto &Category : Catalog.m_vCategories)
				Total += Category.m_Count;
			if(DrawFilter(&s_All, VoteLocalize("All maps"), Total, m_vVoteCategories.empty()))
			{
				m_vVoteCategories.clear();
				SidebarTarget = -1;
				ResetScroll = true;
			}
			for(const auto &Category : Catalog.m_vCategories)
			{
				if(Category.m_Count == 0)
					continue;
				const auto Selected = std::find(m_vVoteCategories.begin(), m_vVoteCategories.end(), Category.m_Name);
				const char *pLabel = Category.m_pHeading ? Category.m_Name.c_str() :
						     VoteLocalize(Catalog.m_vMapTypes.empty() ? "Server options" : "Unsectioned maps");
				const void *pId = Category.m_pHeading ? static_cast<const void *>(Category.m_pHeading) : &s_Unsectioned;
				if(DrawFilter(pId, pLabel, Category.m_Count, Selected != m_vVoteCategories.end()))
				{
					if(Selected != m_vVoteCategories.end())
						m_vVoteCategories.erase(Selected);
					else
						m_vVoteCategories.push_back(Category.m_Name);
					SidebarTarget = -1;
					ResetScroll = true;
				}
			}
			CUIRect Padding;
			Sidebar.HSplitTop(BottomPadding, &Padding, nullptr);
			s_CategoryScroll.AddRect(Padding);
			s_CategoryScroll.End();
		}
		else if(Sidebar.h > 0)
		{
			VoteLabel(Ui(), TextRender(), Sidebar, VoteLocalize("Server options"), 10.5f, DetailText, TEXTALIGN_MC);
		}
	}

	const auto vVisible = Catalog.Filter(m_FilterInput.GetString(), m_vVoteCategories, m_RawVoteOptions);
	int Selected = Catalog.VisibleSelection(vVisible, m_pCallvoteSelection, m_aCallvoteSelection);

	static CListBox s_ListBox;
	VoteListStyle(s_ListBox, ResetScroll);
	if(ResetScroll || Changed)
		s_ListBox.ScrollToSelected();
	const bool TwoLines = !m_RawVoteOptions && std::any_of(vVisible.begin(), vVisible.end(), [&](int Index) {
		const auto &Option = Catalog.m_vOptions[Index];
		return Option.m_pDetails[0] || !LucentVoting::ParseMapLabel(Option.m_pOption->m_aDescription).m_Author.empty();
	});
	const float RowHeight = TwoLines ? 31.f : 23.f;
	int Sections = 0, LastSection = -1;
	if(!m_RawVoteOptions)
	{
		for(const int Index : vVisible)
		{
			const int Section = Catalog.Section(Index);
			if(Section != 0 && Section != LastSection)
				++Sections;
			LastSection = Section;
		}
	}
	s_ListBox.DoStart(RowHeight, vVisible.size(), 1, 3, Selected, &List, false, IGraphics::CORNER_ALL, false, ListHeight(RowHeight, vVisible.size(), Sections));
	LastSection = -1;
	for(const int Index : vVisible)
	{
		const auto &Option = Catalog.m_vOptions[Index];
		const int SectionId = Catalog.Section(Index);
		if(!m_RawVoteOptions && SectionId != 0 && SectionId != LastSection)
		{
			const auto Section = s_ListBox.DoSection(SectionHeight);
			if(Section.m_Visible)
			{
				CUIRect Label = Section.m_Rect;
				Label.VMargin(7, &Label);
				CUIRect Line;
				Label.HSplitTop(1, &Line, &Label);
				LucentUi::Divider(Line, .13f);
				const char *pLabel = SectionId == ServerOptionsSection ? VoteLocalize("Server options") : Catalog.m_vCategories[SectionId].m_Name.c_str();
				VoteLabel(Ui(), TextRender(), Label, pLabel, 10, PrimaryText);
			}
		}
		LastSection = SectionId;

		const auto Item = s_ListBox.DoNextItem(Option.m_pOption, false, 4, false);
		if(!Item.m_Visible)
			continue;
		const bool Hover = Ui()->Enabled() && Ui()->MouseHovered(&Item.m_Rect);
		CUIRect Name = VoteCard(Item.m_Rect, Item.m_Selected, Hover);
		const char *pDescription = Option.m_pOption->m_aDescription;
		const char *pTooltip = nullptr;
		const auto Measure = [&](float Size, std::string_view Label) {
			return Label.empty() ? 0.f : TextRender()->TextWidth(Size, Label.data(), Label.size());
		};
		const auto Map = m_RawVoteOptions ? LucentVoting::CMapLabel{} : LucentVoting::ParseMapLabel(pDescription);
		if(Map.m_Rating.empty())
		{
			VoteLabel(Ui(), TextRender(), Name, pDescription, 11, Item.m_Selected ? PrimaryText : Text);
			if(Measure(11, pDescription) > Name.w)
				pTooltip = pDescription;
		}
		else
		{
			const auto Details = LucentVoting::ParseMapDetails(Option.m_pDetails);
			constexpr float TextGap = 8, BadgeGap = 5;
			char aRating[] = "0 ★";
			aRating[0] = Map.m_Rating.front();
			// Keep the metadata anchor independent of the map and mapper lengths.
			const float MetadataOffset = std::min(208.f, Name.w * .432f);
			CUIRect Metadata;
			Name.VSplitLeft(MetadataOffset, &Name, &Metadata);
			Name.VSplitRight(std::min(TextGap, Name.w), &Name, nullptr);
			if(std::max(Measure(11.5f, Map.m_Title), Measure(10, Map.m_Author)) > Name.w)
				pTooltip = pDescription;

			if(!Map.m_Author.empty() || !Details.m_Finishes.empty())
			{
				CUIRect Author;
				Name.HSplitTop(15, &Name, &Author);
				VoteField(Ui(), TextRender(), Author, Map.m_Author, 10, DetailText, TEXTALIGN_ML, true);
			}
			VoteField(Ui(), TextRender(), Name, Map.m_Title, 11.5f, Item.m_Selected ? PrimaryText : Text, TEXTALIGN_ML, true);

			Metadata.HMargin(std::max(0.f, (Metadata.h - 15) / 2), &Metadata);
			const std::string_view aLabels[] = {aRating, Details.m_Finishes, Details.m_Time};
			float aBadgeWidths[3] = {};
			float TotalWidth = 0;
			for(int i = 0; i < 3; ++i)
			{
				if(aLabels[i].empty())
					continue;
				aBadgeWidths[i] = Measure(10, aLabels[i]) + 10;
				TotalWidth += aBadgeWidths[i] + (TotalWidth > 0 ? BadgeGap : 0);
			}
			const float Scale = std::min(1.f, Metadata.w / TotalWidth);
			if(Scale < 1 && !pTooltip && !Details.m_Finishes.empty())
				pTooltip = Option.m_pDetails;
			for(int i = 0; i < 3; ++i)
			{
				if(aBadgeWidths[i] == 0)
					continue;
				if(i > 0)
					Metadata.VSplitLeft(BadgeGap * Scale, nullptr, &Metadata);
				CUIRect Badge;
				Metadata.VSplitLeft(aBadgeWidths[i] * Scale, &Badge, &Metadata);
				VoteBadge(Ui(), TextRender(), Badge, aLabels[i], i == 0);
			}
		}
		if(Hover && pTooltip)
			Ui()->DoToolTip(Option.m_pOption, &Item.m_Rect, pTooltip);
	}
	if(!vVisible.empty())
		s_ListBox.DoSection(BottomPadding);
	Selected = s_ListBox.DoEnd();
	m_CallvoteSelectedOption = Selected >= 0 && Selected < (int)vVisible.size() ? vVisible[Selected] : SidebarTarget;
	m_pCallvoteSelection = m_CallvoteSelectedOption >= 0 ? Catalog.m_vOptions[m_CallvoteSelectedOption].m_pOption : nullptr;
	str_copy(m_aCallvoteSelection, m_pCallvoteSelection ? m_pCallvoteSelection->m_aDescription : "");
	if(vVisible.empty())
	{
		if(Voting.IsReceivingOptions())
		{
			CUIRect Spinner = List;
			Spinner.HSplitMid(&Spinner, &List);
			Ui()->RenderProgressSpinner(vec2(Spinner.Center().x, Spinner.y + Spinner.h - 8), 5.f);
			List.HSplitTop(24, &List, nullptr);
		}
		const char *pEmpty = Voting.IsReceivingOptions() ? Localize("Loading…") :
				    Catalog.m_vOptions.empty() ? VoteLocalize("No vote options") : Localize("No results");
		VoteLabel(Ui(), TextRender(), List, pEmpty, 12, DetailText, TEXTALIGN_MC);
	}
	return s_ListBox.WasItemActivated();
}

bool CMenus::RenderServerControlKick(CUIRect List, CUIRect Sidebar, bool FilterSpectators, bool ResetScroll)
{
	using namespace LucentStyle::Style;
	int NumOptions = 0, Selected = -1;
	int aPlayerIds[MAX_CLIENTS];
	for(const auto *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo || pInfo->m_ClientId == GameClient()->m_Snap.m_LocalClientId || (FilterSpectators && pInfo->m_Team == TEAM_SPECTATORS))
			continue;
		const int Id = pInfo->m_ClientId;
		const char *pName = GameClient()->m_aClients[Id].m_aName;
		if(!m_FilterInput.IsEmpty() && !str_utf8_find_nocase(pName, m_FilterInput.GetString()))
			continue;
		if(m_CallvoteSelectedPlayer == Id && str_comp(pName, m_aCallvotePlayerName) == 0)
			Selected = NumOptions;
		aPlayerIds[NumOptions++] = Id;
	}
	if(Sidebar.h > 0)
	{
		Sidebar.Draw(Surface.WithAlpha(.88f), IGraphics::CORNER_ALL, 6);
		Sidebar.Margin(10, &Sidebar);
		CUIRect Title, Count;
		Sidebar.HSplitTop(18, &Title, &Sidebar);
		Sidebar.HSplitTop(28, &Count, &Sidebar);
		char aCount[16];
		str_format(aCount, sizeof(aCount), "%d", NumOptions);
		VoteLabel(Ui(), TextRender(), Title, Localize("Players"), 10, DetailText);
		VoteLabel(Ui(), TextRender(), Count, aCount, 22, PrimaryText);
	}

	static CListBox s_ListBox;
	static int s_aPlayerIds[MAX_CLIENTS];
	VoteListStyle(s_ListBox, ResetScroll);
	if(ResetScroll)
		s_ListBox.ScrollToSelected();
	constexpr float RowHeight = 25;
	s_ListBox.DoStart(RowHeight, NumOptions, 1, 3, Selected, &List, false, IGraphics::CORNER_ALL, false, LucentVoting::ListHeight(RowHeight, NumOptions, 0));
	for(int i = 0; i < NumOptions; ++i)
	{
		const int Id = aPlayerIds[i];
		const auto Item = s_ListBox.DoNextItem(&s_aPlayerIds[Id], false, 4, false);
		if(!Item.m_Visible)
			continue;
		const bool Hover = Ui()->Enabled() && Ui()->MouseHovered(&Item.m_Rect);
		CUIRect Tee, Name = VoteCard(Item.m_Rect, Item.m_Selected, Hover);
		Name.VSplitLeft(29, &Tee, &Name);
		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
		TeeInfo.m_Size = 20;
		RenderTools()->RenderIdleTeeAtCenter(&TeeInfo, Tee.Center());
		VoteLabel(Ui(), TextRender(), Name, GameClient()->m_aClients[Id].m_aName, 12, Item.m_Selected ? PrimaryText : Text);
	}
	if(NumOptions)
		s_ListBox.DoSection(LucentVoting::BottomPadding);
	Selected = s_ListBox.DoEnd();
	m_CallvoteSelectedPlayer = Selected >= 0 && Selected < NumOptions ? aPlayerIds[Selected] : -1;
	str_copy(m_aCallvotePlayerName, m_CallvoteSelectedPlayer >= 0 ? GameClient()->m_aClients[m_CallvoteSelectedPlayer].m_aName : "");
	if(!NumOptions)
		VoteLabel(Ui(), TextRender(), List, m_FilterInput.IsEmpty() ? VoteLocalize("No eligible players") : Localize("No results"), 12, DetailText, TEXTALIGN_MC);
	return s_ListBox.WasItemActivated();
}

void CMenus::RenderServerControl(CUIRect MainView)
{
	using namespace LucentStyle::Style;
	LucentUi::CScope Style(Ui());
	enum { OPTIONS, KICK, SPECTATE, NUM_TABS };
	static int s_Tab = OPTIONS;
	static int s_aTabIds[NUM_TABS], s_Call, s_Force, s_Remove, s_Add;
	const char *apTabs[] = {VoteLocalize("Maps"), VoteLocalize("Kick player"), VoteLocalize("Spectator")};
	bool ResetScroll = m_ControlPageOpening;
	const bool Hotkeys = !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive();

	CUIRect Tabs, Footer;
	MainView.HSplitTop(30, &Tabs, &MainView);
	Tabs.Draw(Surface.WithAlpha(.86f), IGraphics::CORNER_ALL, 6);
	Tabs.Margin(3, &Tabs);
	const float TabWidth = Tabs.w / NUM_TABS;
	for(int i = 0; i < NUM_TABS; ++i)
	{
		CUIRect Button;
		Tabs.VSplitLeft(TabWidth, &Button, &Tabs);
		Button.VMargin(2, &Button);
		if(LucentUi::Button(Ui(), TextRender(), &s_aTabIds[i], apTabs[i], Button, s_Tab == i, true, true) && s_Tab != i)
		{
			s_Tab = i;
			ResetScroll = true;
		}
	}

	const bool Options = s_Tab == OPTIONS;
	const bool Rcon = Client()->RconAuthed();
	CUIRect Admin{};
	if(Rcon)
		MainView.HSplitBottom(Options ? 72.f : 34.f, &MainView, &Admin);
	MainView.HSplitBottom(39, &MainView, &Footer);
	MainView.HMargin(9, &MainView);

	CUIRect Sidebar{};
	const auto SidebarLayout = LucentVoting::SidebarLayout(MainView.w, MainView.h, Options);
	if(SidebarLayout.m_Width > 0)
	{
		MainView.VSplitLeft(SidebarLayout.m_Width, &Sidebar, &MainView);
		MainView.VSplitLeft(9, nullptr, &MainView);
	}
	else if(SidebarLayout.m_Height > 0)
	{
		MainView.HSplitTop(SidebarLayout.m_Height, &Sidebar, &MainView);
		MainView.HSplitTop(7, nullptr, &MainView);
	}

	CUIRect Search, Heading;
	MainView.HSplitTop(28, &Search, &MainView);
	Search.Draw(Surface.WithAlpha(.88f), IGraphics::CORNER_ALL, 6);
	Search.Margin(4, &Search);
	Search.VMargin(5, &Search);
	if(m_ControlPageOpening)
	{
		m_ControlPageOpening = false;
		if(Hotkeys)
		{
			Ui()->SetActiveItem(&m_FilterInput);
			m_FilterInput.SelectAll();
		}
	}
	ResetScroll |= Ui()->DoEditBox_Search(&m_FilterInput, &Search, 11, Hotkeys);

	MainView.HSplitTop(7, nullptr, &MainView);
	MainView.Draw(Surface.WithAlpha(.89f), IGraphics::CORNER_ALL, 6);
	MainView.Margin(5, &MainView);
	MainView.HSplitTop(19, &Heading, &MainView);
	Heading.VMargin(7, &Heading);
	const bool Receiving = Options && GameClient()->m_Voting.IsReceivingOptions();
	if(Receiving && GameClient()->m_Voting.NumOptions() > 0)
	{
		CUIRect Spinner, Status;
		Heading.VSplitRight(16, &Heading, &Spinner);
		Heading.VSplitRight(std::min(75.f, Heading.w * .32f), &Heading, &Status);
		Ui()->RenderProgressSpinner(Spinner.Center(), 4);
		VoteLabel(Ui(), TextRender(), Status, Localize("Loading…"), 9.5f, DetailText, TEXTALIGN_MR);
	}
	VoteLabel(Ui(), TextRender(), Heading, Options ? VoteLocalize("Available votes") : Localize("Players"), 10, PrimaryText);
	const bool Activated = Options ? RenderServerControlServer(MainView, Sidebar, ResetScroll) :
					 RenderServerControlKick(MainView, Sidebar, s_Tab == SPECTATE, ResetScroll);
	const bool CanCall = Options ? m_CallvoteSelectedOption >= 0 : m_CallvoteSelectedPlayer >= 0;
	const bool TypeVote = Options && !m_RawVoteOptions && CanCall &&
			      std::find(m_VotingCatalog.m_vMapTypes.begin(), m_VotingCatalog.m_vMapTypes.end(), m_CallvoteSelectedOption) != m_VotingCatalog.m_vMapTypes.end();
	const auto Call = [&](bool Force) {
		if(!CanCall)
			return;
		const char *pReason = m_CallvoteReasonInput.GetString();
		auto &Voting = GameClient()->m_Voting;
		if(Options)
		{
			Voting.CallvoteOption(m_CallvoteSelectedOption, pReason, Force);
			if(!Force && g_Config.m_UiCloseWindowAfterChangingSetting)
				SetActive(false);
		}
		else
		{
			if(s_Tab == KICK)
				Voting.CallvoteKick(m_CallvoteSelectedPlayer, pReason, Force);
			else
				Voting.CallvoteSpectate(m_CallvoteSelectedPlayer, pReason, Force);
			SetActive(false);
		}
		m_CallvoteReasonInput.Clear();
	};

	CUIRect Rule, Reason, Button;
	Footer.HSplitTop(1, &Rule, &Footer);
	LucentUi::Divider(Rule, .18f);
	Footer.HMargin(5, &Footer);
	Footer.VSplitRight(std::min(150.f, Footer.w * .38f), &Reason, &Button);
	Reason.VSplitRight(8, &Reason, nullptr);
	if(Reason.w >= 150)
	{
		CUIRect Label;
		const char *pReason = Localize("Reason:");
		Reason.VSplitLeft(std::min(TextRender()->TextWidth(11, pReason) + 10, Reason.w * .45f), &Label, &Reason);
		VoteLabel(Ui(), TextRender(), Label, pReason, 11, DetailText);
		m_CallvoteReasonInput.SetEmptyText(VoteLocalize("Optional"));
	}
	else
		m_CallvoteReasonInput.SetEmptyText(Localize("Reason:"));
	if(Hotkeys && Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed())
	{
		Ui()->SetActiveItem(&m_CallvoteReasonInput);
		m_CallvoteReasonInput.SelectAll();
	}
	Ui()->DoEditBox(&m_CallvoteReasonInput, &Reason, 11);
	if(LucentUi::SceneButton(Ui(), TextRender(), &s_Call, VoteLocalize(TypeVote ? "Vote for map type" : "Start vote"), Button, CanCall) || Activated)
		Call(false);

	if(!Rcon)
		return;
	Admin.Draw(Surface.WithAlpha(.86f), IGraphics::CORNER_ALL, 6);
	Admin.Margin(5, &Admin);
	CUIRect Tools, ForceButton, RemoveButton;
	Admin.HSplitTop(23, &Tools, &Admin);
	Tools.VSplitLeft(std::min(125.f, Tools.w * .42f), &ForceButton, &Tools);
	Tools.VSplitLeft(6, nullptr, &Tools);
	if(LucentUi::Button(Ui(), TextRender(), &s_Force, Localize("Force vote"), ForceButton, false, CanCall, true))
		Call(true);
	if(!Options)
		return;
	Tools.VSplitLeft(std::min(90.f, Tools.w), &RemoveButton, nullptr);
	if(LucentUi::Button(Ui(), TextRender(), &s_Remove, Localize("Remove"), RemoveButton, false, CanCall, true))
		GameClient()->m_Voting.RemovevoteOption(m_CallvoteSelectedOption);

	static CLineInputBuffered<VOTE_DESC_LENGTH> s_Description;
	static CLineInputBuffered<VOTE_CMD_LENGTH> s_Command;
	s_Description.SetEmptyText(Localize("Vote description:"));
	s_Command.SetEmptyText(Localize("Vote command:"));
	CUIRect Description, Command, Add;
	Admin.HSplitTop(5, nullptr, &Admin);
	Admin.HSplitTop(24, &Admin, nullptr);
	Admin.VSplitRight(std::min(83.f, Admin.w * .25f), &Admin, &Add);
	Admin.VSplitRight(6, &Admin, nullptr);
	Admin.VSplitMid(&Description, &Command, 6);
	Ui()->DoEditBox(&s_Description, &Description, 11);
	Ui()->DoEditBox(&s_Command, &Command, 11);
	if(LucentUi::Button(Ui(), TextRender(), &s_Add, Localize("Add"), Add, false, !s_Description.IsEmpty() && !s_Command.IsEmpty(), true))
		GameClient()->m_Voting.AddvoteOption(s_Description.GetString(), s_Command.GetString());
}

void CMenus::RenderInGameNetwork(CUIRect MainView)
{
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	CUIRect TabBar, Button;
	MainView.HSplitTop(24.0f, &TabBar, &MainView);

	int NewPage = g_Config.m_UiPage;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_InternetButton;
	if(DoButton_MenuTab(&s_InternetButton, FontIcon::EARTH_AMERICAS, g_Config.m_UiPage == PAGE_INTERNET, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_INTERNET;
	}
	Ui()->DoToolTip(&s_InternetButton, &Button, Localize("Internet"));

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_LanButton;
	if(DoButton_MenuTab(&s_LanButton, FontIcon::NETWORK_WIRED, g_Config.m_UiPage == PAGE_LAN, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_LAN;
	}
	Ui()->DoToolTip(&s_LanButton, &Button, Localize("LAN"));

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_FavoritesButton;
	if(DoButton_MenuTab(&s_FavoritesButton, FontIcon::STAR, g_Config.m_UiPage == PAGE_FAVORITES, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_FAVORITES;
	}
	Ui()->DoToolTip(&s_FavoritesButton, &Button, Localize("Favorites"));

	const int MaxPage = PAGE_FAVORITES + ServerBrowser()->FavoriteCommunities().size();
	if(
		!Ui()->IsPopupOpen() &&
		CLineInput::GetActiveInput() == nullptr &&
		(g_Config.m_UiPage >= PAGE_INTERNET && g_Config.m_UiPage <= MaxPage) &&
		(m_MenuPage >= PAGE_INTERNET && m_MenuPage <= PAGE_FAVORITE_COMMUNITY_5))
	{
		if(Input()->KeyPress(KEY_RIGHT))
		{
			NewPage = g_Config.m_UiPage + 1;
			if(NewPage > MaxPage)
				NewPage = PAGE_INTERNET;
		}
		if(Input()->KeyPress(KEY_LEFT))
		{
			NewPage = g_Config.m_UiPage - 1;
			if(NewPage < PAGE_INTERNET)
				NewPage = MaxPage;
		}
	}

	size_t FavoriteCommunityIndex = 0;
	static CButtonContainer s_aFavoriteCommunityButtons[5];
	static_assert(std::size(s_aFavoriteCommunityButtons) == (size_t)PAGE_FAVORITE_COMMUNITY_5 - PAGE_FAVORITE_COMMUNITY_1 + 1);
	for(const CCommunity *pCommunity : ServerBrowser()->FavoriteCommunities())
	{
		TabBar.VSplitLeft(75.0f, &Button, &TabBar);
		const int Page = PAGE_FAVORITE_COMMUNITY_1 + FavoriteCommunityIndex;
		if(DoButton_MenuTab(&s_aFavoriteCommunityButtons[FavoriteCommunityIndex], FontIcon::ELLIPSIS, g_Config.m_UiPage == Page, &Button, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 10.0f, m_CommunityIcons.Find(pCommunity->Id())))
		{
			NewPage = Page;
		}
		Ui()->DoToolTip(&s_aFavoriteCommunityButtons[FavoriteCommunityIndex], &Button, pCommunity->Name());

		++FavoriteCommunityIndex;
		if(FavoriteCommunityIndex >= std::size(s_aFavoriteCommunityButtons))
			break;
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	if(NewPage != g_Config.m_UiPage)
	{
		SetMenuPage(NewPage);
	}

	RenderServerbrowser(MainView);
}

// ghost stuff
int CMenus::GhostlistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	const char *pMap = pSelf->GameClient()->Map()->BaseName();
	if(IsDir || !str_endswith(pInfo->m_pName, ".gho") || !str_startswith(pInfo->m_pName, pMap))
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/%s", pSelf->GameClient()->m_Ghost.GetGhostDir(), pInfo->m_pName);

	CGhostInfo Info;
	if(!pSelf->GameClient()->m_Ghost.GhostLoader()->GetGhostInfo(aFilename, &Info, pMap, pSelf->GameClient()->Map()->Sha256(), pSelf->GameClient()->Map()->Crc()))
		return 0;

	CGhostItem Item;
	str_copy(Item.m_aFilename, aFilename);
	str_copy(Item.m_aPlayer, Info.m_aOwner);
	Item.m_Date = pInfo->m_TimeModified;
	Item.m_Time = Info.m_Time;
	if(Item.m_Time > 0)
		pSelf->m_vGhosts.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_GhostPopulateStartTime > 500ms)
	{
		pSelf->RenderLoading(Localize("Loading ghost files"), "", 0);
	}

	return 0;
}

void CMenus::GhostlistPopulate()
{
	m_vGhosts.clear();
	m_GhostPopulateStartTime = time_get_nanoseconds();
	Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, GameClient()->m_Ghost.GetGhostDir(), GhostlistFetchCallback, this);
	SortGhostlist();

	CGhostItem *pOwnGhost = nullptr;
	for(auto &Ghost : m_vGhosts)
	{
		Ghost.m_Failed = false;
		if(str_comp(Ghost.m_aPlayer, Client()->PlayerName()) == 0 && (!pOwnGhost || Ghost.m_Time < pOwnGhost->m_Time))
			pOwnGhost = &Ghost;
	}

	if(pOwnGhost)
	{
		pOwnGhost->m_Own = true;
		pOwnGhost->m_Slot = GameClient()->m_Ghost.Load(pOwnGhost->m_aFilename);
	}
}

CMenus::CGhostItem *CMenus::GetOwnGhost()
{
	for(auto &Ghost : m_vGhosts)
		if(Ghost.m_Own)
			return &Ghost;
	return nullptr;
}

void CMenus::UpdateOwnGhost(CGhostItem Item)
{
	int Own = -1;
	for(size_t i = 0; i < m_vGhosts.size(); i++)
		if(m_vGhosts[i].m_Own)
			Own = i;

	if(Own == -1)
	{
		Item.m_Own = true;
	}
	else if(g_Config.m_ClRaceGhostSaveBest && (Item.HasFile() || !m_vGhosts[Own].HasFile()))
	{
		Item.m_Own = true;
		DeleteGhostItem(Own);
	}
	else if(m_vGhosts[Own].m_Time > Item.m_Time)
	{
		Item.m_Own = true;
		m_vGhosts[Own].m_Own = false;
		m_vGhosts[Own].m_Slot = -1;
	}
	else
	{
		Item.m_Own = false;
		Item.m_Slot = -1;
	}

	Item.m_Date = std::time(nullptr);
	Item.m_Failed = false;
	m_vGhosts.push_back(Item);
	SortGhostlist();
}

void CMenus::DeleteGhostItem(int Index)
{
	if(m_vGhosts[Index].HasFile())
		Storage()->RemoveFile(m_vGhosts[Index].m_aFilename, IStorage::TYPE_SAVE);
	m_vGhosts.erase(m_vGhosts.begin() + Index);
}

void CMenus::SortGhostlist()
{
	if(g_Config.m_GhSort == GHOST_SORT_NAME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (str_comp(Left.m_aPlayer, Right.m_aPlayer) > 0) : (str_comp(Left.m_aPlayer, Right.m_aPlayer) < 0);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_TIME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Time > Right.m_Time) : (Left.m_Time < Right.m_Time);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_DATE)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Date > Right.m_Date) : (Left.m_Date < Right.m_Date);
		});
}

void CMenus::RenderGhost(CUIRect MainView)
{
	// render background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitRight(5.0f, &MainView, nullptr);

	CUIRect Headers, Status;
	CUIRect View = MainView;

	View.HSplitTop(17.0f, &Headers, &View);
	View.HSplitBottom(28.0f, &View, &Status);

	// split of the scrollbar
	Headers.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(20.0f, &Headers, nullptr);

	class CColumn
	{
	public:
		const char *m_pCaption;
		int m_Id;
		int m_Sort;
		float m_Width;
		CUIRect m_Rect;
	};

	enum
	{
		COL_ACTIVE = 0,
		COL_NAME,
		COL_TIME,
		COL_DATE,
	};

	static CColumn s_aCols[] = {
		{"", -1, GHOST_SORT_NONE, 2.0f, {0}},
		{"", COL_ACTIVE, GHOST_SORT_NONE, 30.0f, {0}},
		{Localizable("Name"), COL_NAME, GHOST_SORT_NAME, 200.0f, {0}},
		{Localizable("Time"), COL_TIME, GHOST_SORT_TIME, 90.0f, {0}},
		{Localizable("Date"), COL_DATE, GHOST_SORT_DATE, 150.0f, {0}},
	};

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

		if(i + 1 < NumCols)
			Headers.VSplitLeft(2, nullptr, &Headers);
	}

	// do headers
	for(const auto &Col : s_aCols)
	{
		if(DoButton_GridHeader(&Col.m_Id, Localize(Col.m_pCaption), g_Config.m_GhSort == Col.m_Sort, &Col.m_Rect))
		{
			if(Col.m_Sort != GHOST_SORT_NONE)
			{
				if(g_Config.m_GhSort == Col.m_Sort)
					g_Config.m_GhSortOrder ^= 1;
				else
					g_Config.m_GhSortOrder = 0;
				g_Config.m_GhSort = Col.m_Sort;

				SortGhostlist();
			}
		}
	}

	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 0);

	int NumGhosts;
	int NumFailed;
	int NumActivated;
	const auto &&UpdateCounts = [&]() {
		NumGhosts = m_vGhosts.size();
		NumFailed = 0;
		NumActivated = 0;
		for(const CGhostItem &Ghost : m_vGhosts)
		{
			if(Ghost.m_Failed || !Ghost.HasFile())
			{
				NumFailed++;
			}
			else if(Ghost.Active())
			{
				NumActivated++;
			}
		}
	};
	UpdateCounts();

	static int s_SelectedIndex = 0;
	static CListBox s_ListBox;
	s_ListBox.DoStart(17.0f, NumGhosts, 1, 3, s_SelectedIndex, &View, false);

	for(int i = 0; i < NumGhosts; i++)
	{
		const CGhostItem *pGhost = &m_vGhosts[i];
		const CListboxItem Item = s_ListBox.DoNextItem(pGhost);

		if(!Item.m_Visible)
			continue;

		ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f);
		if(pGhost->m_Own)
			Color = color_cast<ColorRGBA>(ColorHSLA(0.33f, 1.0f, 0.75f));

		if(pGhost->m_Failed)
			Color = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f);

		TextRender()->TextColor(Color.WithAlpha(pGhost->HasFile() ? 1.0f : 0.5f));

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Item.m_Rect.y;
			Button.h = Item.m_Rect.h;
			Button.w = s_aCols[c].m_Rect.w;

			int Id = s_aCols[c].m_Id;

			if(Id == COL_ACTIVE)
			{
				if(pGhost->Active())
				{
					Graphics()->WrapClamp();
					Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[(SPRITE_OOP + 7) - SPRITE_OOP]);
					Graphics()->QuadsBegin();
					IGraphics::CQuadItem QuadItem(Button.x + Button.w / 2, Button.y + Button.h / 2, 20.0f, 20.0f);
					Graphics()->QuadsDraw(&QuadItem, 1);

					Graphics()->QuadsEnd();
					Graphics()->WrapNormal();
				}
			}
			else if(Id == COL_NAME)
			{
				Ui()->DoLabel(&Button, pGhost->m_aPlayer, 12.0f, TEXTALIGN_ML);
			}
			else if(Id == COL_TIME)
			{
				char aBuf[64];
				str_time(pGhost->m_Time / 10, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_ML);
			}
			else if(Id == COL_DATE)
			{
				char aBuf[64];
				str_timestamp_ex(pGhost->m_Date, aBuf, sizeof(aBuf), TimestampFormat::SPACE);
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_ML);
			}
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	s_SelectedIndex = s_ListBox.DoEnd();

	Status.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_B, 5.0f);
	Status.Margin(5.0f, &Status);

	CUIRect Button;
	Status.VSplitLeft(25.0f, &Button, &Status);

	static CButtonContainer s_ReloadButton;
	static CButtonContainer s_DirectoryButton;
	static CButtonContainer s_ActivateAll;

	if(Ui()->DoButton_FontIcon(&s_ReloadButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &Button, BUTTONFLAG_LEFT) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		GameClient()->m_Ghost.UnloadAll();
		GhostlistPopulate();
		UpdateCounts();
	}

	Status.VSplitLeft(5.0f, &Button, &Status);
	Status.VSplitLeft(175.0f, &Button, &Status);
	if(DoButton_Menu(&s_DirectoryButton, Localize("Ghosts directory"), 0, &Button))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ghosts", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("ghosts", IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}

	Status.VSplitLeft(5.0f, &Button, &Status);
	if(NumGhosts - NumFailed > 0)
	{
		Status.VSplitLeft(175.0f, &Button, &Status);
		bool ActivateAll = ((NumGhosts - NumFailed) != NumActivated) && GameClient()->m_Ghost.FreeSlots();

		const char *pActionText = ActivateAll ? Localize("Activate all") : Localize("Deactivate all");
		if(DoButton_Menu(&s_ActivateAll, pActionText, 0, &Button))
		{
			for(int i = 0; i < NumGhosts; i++)
			{
				CGhostItem *pGhost = &m_vGhosts[i];
				if(pGhost->m_Failed || !pGhost->HasFile())
					continue;

				if(ActivateAll)
				{
					if(!pGhost->Active())
					{
						if(!GameClient()->m_Ghost.FreeSlots())
							break;

						pGhost->m_Slot = GameClient()->m_Ghost.Load(pGhost->m_aFilename);
						if(pGhost->m_Slot == -1)
							pGhost->m_Failed = true;
					}
				}
				else
				{
					if(pGhost->Active())
					{
						GameClient()->m_Ghost.Unload(pGhost->m_Slot);
						pGhost->m_Slot = -1;
					}
				}
			}
		}
	}

	if(s_SelectedIndex == -1 || s_SelectedIndex >= (int)m_vGhosts.size())
		return;

	CGhostItem *pGhost = &m_vGhosts[s_SelectedIndex];

	CGhostItem *pOwnGhost = GetOwnGhost();
	int ReservedSlots = !pGhost->m_Own && !(pOwnGhost && pOwnGhost->Active());
	if(!pGhost->m_Failed && pGhost->HasFile() && (pGhost->Active() || GameClient()->m_Ghost.FreeSlots() > ReservedSlots))
	{
		Status.VSplitRight(120.0f, &Status, &Button);

		static CButtonContainer s_GhostButton;
		const char *pText = pGhost->Active() ? Localize("Deactivate") : Localize("Activate");
		if(DoButton_Menu(&s_GhostButton, pText, 0, &Button) || s_ListBox.WasItemActivated())
		{
			if(pGhost->Active())
			{
				GameClient()->m_Ghost.Unload(pGhost->m_Slot);
				pGhost->m_Slot = -1;
			}
			else
			{
				pGhost->m_Slot = GameClient()->m_Ghost.Load(pGhost->m_aFilename);
				if(pGhost->m_Slot == -1)
					pGhost->m_Failed = true;
			}
		}
		Status.VSplitRight(5.0f, &Status, nullptr);
	}

	Status.VSplitRight(120.0f, &Status, &Button);

	static CButtonContainer s_DeleteButton;
	if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
	{
		if(pGhost->Active())
			GameClient()->m_Ghost.Unload(pGhost->m_Slot);
		DeleteGhostItem(s_SelectedIndex);
		s_SelectedIndex = std::min(s_SelectedIndex, (int)m_vGhosts.size() - 1);
		if(s_SelectedIndex == -1)
		{
			return;
		}
		pGhost = &m_vGhosts[s_SelectedIndex];
	}

	Status.VSplitRight(5.0f, &Status, nullptr);

	bool Recording = GameClient()->m_Ghost.GhostRecorder()->IsRecording();
	if(!pGhost->HasFile() && !Recording && pGhost->Active())
	{
		static CButtonContainer s_SaveButton;
		Status.VSplitRight(120.0f, &Status, &Button);
		if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button))
			GameClient()->m_Ghost.SaveGhost(pGhost);
	}
}

void CMenus::RenderIngameHint()
{
	// With touch controls enabled there is a Close button in the menu and usually no Escape key available.
	if(g_Config.m_ClTouchControls)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, 300);
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->Text(5, 280, 5, Localize("Menu opened. Press Esc key again to close menu."), -1.0f);
	Ui()->MapScreen();
}

// Lucent shell keeps all legacy page implementations and their action handlers.
void CMenus::RenderLucentGameNavigation(CUIRect View)
{
	View.Draw(ColorRGBA(.035f, .045f, .065f, .88f), IGraphics::CORNER_ALL, 8.f);
	View.Margin(8.f, &View);
	CUIRect Row;
	View.HSplitTop(32.f, &Row, &View);
	Ui()->DoLabel(&Row, "LUCENT", 16.f, TEXTALIGN_MC);
	const float RowHeight = std::min(28.f, View.h / 12.f);
	static int s_aIds[12];
	const int aPages[] = {PAGE_GAME, PAGE_GAMEVIEW, PAGE_PLAYERS, PAGE_SERVER_INFO, PAGE_NETWORK, PAGE_GHOST, PAGE_CALLVOTE, PAGE_DEMOS, PAGE_SETTINGS};
	const char *apNames[] = {"Game", "GameView", "Players", "Server info", "Browser", "Ghost", "Call vote", "Demos", "Settings"};
	for(int i = 0; i < 9; ++i)
	{
		if(aPages[i] == PAGE_GHOST && !GameClient()->m_GameInfo.m_Race)
			continue;
		View.HSplitTop(RowHeight, &Row, &View);
		Row.HMargin(2.f, &Row);
		if(LucentUi::Button(Ui(), TextRender(), &s_aIds[i], Localize(apNames[i]), Row, m_GamePage == aPages[i]))
		{
			m_GamePage = aPages[i];
			if(m_GamePage == PAGE_CALLVOTE)
				m_ControlPageOpening = true;
		}
	}
	View.HSplitTop(RowHeight, &Row, &View);
	if(LucentUi::Button(Ui(), TextRender(), &s_aIds[9], Localize("Editor"), Row))
		g_Config.m_ClEditor = 1;
	View.HSplitTop(RowHeight, &Row, &View);
	if(LucentUi::Button(Ui(), TextRender(), &s_aIds[10], Localize("Quit"), Row))
	{
		if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0) || m_MenusIngameTouchControls.UnsavedChanges() || GameClient()->m_TouchControls.HasEditingChanges())
			m_Popup = POPUP_QUIT;
		else
			Client()->Quit();
	}
	View.HSplitTop(RowHeight, &Row, &View);
	if(LucentUi::Button(Ui(), TextRender(), &s_aIds[11], Localize("Back"), Row))
		SetActive(false);
}

void CMenus::RenderLucentGameView(CUIRect View)
{
	static CScrollRegion s_Scroll;
	CUIRect Title;
	View.HSplitTop(34.f, &Title, &View);
	Ui()->DoLabel(&Title, Localize("Tuning"), 22.f, TEXTALIGN_ML);
	s_Scroll.Begin(&View);
	auto Label = [&](const char *pText) {
		CUIRect Rect = LucentUi::PanelRow(View, s_Scroll, 32.f);
		Ui()->DoLabel(&Rect, pText, 12.f, TEXTALIGN_ML);
	};
	char aText[160];
	str_format(aText, sizeof(aText), "%s: %d (%s)", Localize("Tune zone"), GameClient()->m_aLocalTuneZone[g_Config.m_ClDummy], g_Config.m_ClDummy ? "dummy" : "main");
	if(GameClient()->m_aLocalTuneZone[g_Config.m_ClDummy] < 0)
		Label(Localize("Tune zone unavailable"));
	else
		Label(aText);
	if(GameClient()->m_aExpectingTuningForZone[g_Config.m_ClDummy] >= 0)
		Label(Localize("Waiting for tuning for the current zone"));
	Label(Localize("Active client tuning (read only; includes protocol defaults)"));
	const CTuningParams &Tuning = GameClient()->m_aTuning[g_Config.m_ClDummy];
	for(int i = 0; i < CTuningParams::Num(); ++i)
	{
		// 0.7 removed laser_damage; do not present it as a supported value.
		if(Client()->IsSixup() && i == 30)
			continue;
		float Value;
		if(Tuning.Get(i, &Value))
		{
			str_format(aText, sizeof(aText), "%s: %.2f", CTuningParams::Name(i), Value);
			Label(aText);
		}
	}
	s_Scroll.End();
}
