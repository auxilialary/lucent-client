/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menu_background.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/settings/general.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
	using C = LucentSettings::EGeneralControl;
	LucentSettings::CGeneralState State;
	State.m_Visible[int(C::SMOOTH_CAMERA)] = g_Config.m_ClDyncam != 0;
	State.m_Enabled[int(C::MAX_DEMOS)] = g_Config.m_ClAutoDemoRecord != 0;
	State.m_Enabled[int(C::MAX_SCREENSHOTS)] = g_Config.m_ClAutoScreenshot != 0;
	State.m_Enabled[int(C::MAX_STATBOARD)] = g_Config.m_ClAutoStatboardScreenshot != 0;
	State.m_Enabled[int(C::MAX_CSV)] = g_Config.m_ClAutoCSV != 0;
	State.m_Enabled[int(C::REPLAY_LENGTH)] = g_Config.m_ClReplays != 0;
	const void *apLimitIds[] = {&g_Config.m_ClAutoDemoMax, &g_Config.m_ClAutoScreenshotMax,
		&g_Config.m_ClAutoStatboardScreenshotMax, &g_Config.m_ClAutoCSVMax, &g_Config.m_ClReplayLength};
	for(size_t i = 0; i < LucentSettings::CGeneralPresentation::Limits.size(); ++i)
		if(!State.m_Enabled[int(LucentSettings::CGeneralPresentation::Limits[i])] && Ui()->ActiveItem() == apLimitIds[i])
			Ui()->SetActiveItem(nullptr);
	LucentSettings::RenderGeneral(Ui(), TextRender(), MainView, State, [&](C Control, CUIRect Button, bool Focused, bool Activate, bool VisualEnabled, const CUIRect *pValueRect) {
		char aBuf[128 + IO_MAX_PATH_LENGTH];
		int InactiveLimit;
		switch(Control)
		{
		case C::DYNAMIC_CAMERA:
		{
			const bool IsDyncam = g_Config.m_ClDyncam || g_Config.m_ClMouseFollowfactor > 0;
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClDyncam, Localize("Dynamic Camera"), IsDyncam, &Button, VisualEnabled))
			{
				if(IsDyncam)
				{
					g_Config.m_ClDyncam = 0;
					g_Config.m_ClMouseFollowfactor = 0;
				}
				else
				{
					g_Config.m_ClDyncam = 1;
				}
			}
			break;
		}
		case C::SMOOTH_CAMERA:
		{
			if(g_Config.m_ClDyncam)
			{
				if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClDyncamSmoothness, Localize("Smooth Dynamic Camera"), g_Config.m_ClDyncamSmoothness, &Button, VisualEnabled))
				{
					if(g_Config.m_ClDyncamSmoothness)
					{
						g_Config.m_ClDyncamSmoothness = 0;
					}
					else
					{
						g_Config.m_ClDyncamSmoothness = 50;
						g_Config.m_ClDyncamStabilizing = 50;
					}
				}
			}
			break;
		}
		case C::WEAPON_PICKUP:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoswitchWeapons, Localize("Switch weapon on pickup"), g_Config.m_ClAutoswitchWeapons, &Button, VisualEnabled))
				g_Config.m_ClAutoswitchWeapons ^= 1;
			break;
		}
		case C::WEAPON_EMPTY:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoswitchWeaponsOutOfAmmo, Localize("Switch weapon when out of ammo"), g_Config.m_ClAutoswitchWeaponsOutOfAmmo, &Button, VisualEnabled))
				g_Config.m_ClAutoswitchWeaponsOutOfAmmo ^= 1;
			break;
		}
		case C::SKIP_START:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClSkipStartMenu, Localize("Skip the main menu"), g_Config.m_ClSkipStartMenu, &Button, VisualEnabled))
				g_Config.m_ClSkipStartMenu ^= 1;
			break;
		}
		case C::POWER_SAVE:
		{
			static int s_LowerRefreshRate;
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &s_LowerRefreshRate, Localize("Save power by lowering update rate (higher input latency)"), g_Config.m_ClRefreshRate <= 480 && g_Config.m_ClRefreshRate != 0, &Button, VisualEnabled))
				g_Config.m_ClRefreshRate = g_Config.m_ClRefreshRate > 480 || g_Config.m_ClRefreshRate == 0 ? 480 : 0;
			break;
		}
		case C::AUTO_DEMO:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoDemoRecord, Localize("Automatically record demos"), g_Config.m_ClAutoDemoRecord, &Button, VisualEnabled))
				g_Config.m_ClAutoDemoRecord ^= 1;
			break;
		}
		case C::MAX_DEMOS:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClAutoDemoMax)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClAutoDemoMax;
			Ui()->DoScrollbarOption(&g_Config.m_ClAutoDemoMax, Ui()->Enabled() && g_Config.m_ClAutoDemoRecord ? &g_Config.m_ClAutoDemoMax : &InactiveLimit, &Button, Localize("Max demos"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT | CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE, "", pValueRect);
			break;
		}
		case C::AUTO_SCREENSHOT:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoScreenshot, Localize("Automatically take game over screenshot"), g_Config.m_ClAutoScreenshot, &Button, VisualEnabled))
				g_Config.m_ClAutoScreenshot ^= 1;
			break;
		}
		case C::MAX_SCREENSHOTS:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClAutoScreenshotMax)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClAutoScreenshotMax;
			Ui()->DoScrollbarOption(&g_Config.m_ClAutoScreenshotMax, Ui()->Enabled() && g_Config.m_ClAutoScreenshot ? &g_Config.m_ClAutoScreenshotMax : &InactiveLimit, &Button, Localize("Max Screenshots"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT | CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE, "", pValueRect);
			break;
		}
		case C::AUTO_STATBOARD:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoStatboardScreenshot, Localize("Automatically take statboard screenshot"), g_Config.m_ClAutoStatboardScreenshot, &Button, VisualEnabled))
			{
				g_Config.m_ClAutoStatboardScreenshot ^= 1;
			}
			break;
		}
		case C::MAX_STATBOARD:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClAutoStatboardScreenshotMax)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClAutoStatboardScreenshotMax;
			Ui()->DoScrollbarOption(&g_Config.m_ClAutoStatboardScreenshotMax, Ui()->Enabled() && g_Config.m_ClAutoStatboardScreenshot ? &g_Config.m_ClAutoStatboardScreenshotMax : &InactiveLimit, &Button, Localize("Max Screenshots"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT | CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE, "", pValueRect);
			break;
		}
		case C::AUTO_CSV:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoCSV, Localize("Automatically create statboard csv"), g_Config.m_ClAutoCSV, &Button, VisualEnabled))
			{
				g_Config.m_ClAutoCSV ^= 1;
			}
			break;
		}
		case C::MAX_CSV:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClAutoCSVMax)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClAutoCSVMax;
			Ui()->DoScrollbarOption(&g_Config.m_ClAutoCSVMax, Ui()->Enabled() && g_Config.m_ClAutoCSV ? &g_Config.m_ClAutoCSVMax : &InactiveLimit, &Button, Localize("Max CSVs"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT | CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE, "", pValueRect);
			break;
		}
		case C::SETTINGS_FILE:
		{
			static CButtonContainer s_SettingsButtonId;
			if(LucentSettings::GeneralFileButton(Ui(), TextRender(), &s_SettingsButtonId, Localize("Settings file"), &Button, Focused, Activate))
			{
				Storage()->GetCompletePath(IStorage::TYPE_SAVE, CONFIG_FILE, aBuf, sizeof(aBuf));
				Client()->ViewFile(aBuf);
			}
			Ui()->DoToolTip(&s_SettingsButtonId, &Button, Localize("Open the settings file"));
			break;
		}
		case C::SAVES_FILE:
		{
			static CButtonContainer s_SavesButtonId;
			if(LucentSettings::GeneralFileButton(Ui(), TextRender(), &s_SavesButtonId, Localize("Saves file"), &Button, Focused, Activate))
			{
				Storage()->GetCompletePath(IStorage::TYPE_SAVE, SAVES_FILE, aBuf, sizeof(aBuf));
				Client()->ViewFile(aBuf);
			}
			Ui()->DoToolTip(&s_SavesButtonId, &Button, Localize("Open the saves file"));
			break;
		}
		case C::CONFIG_DIRECTORY:
		{
			static CButtonContainer s_ConfigButtonId;
			if(LucentSettings::GeneralFileButton(Ui(), TextRender(), &s_ConfigButtonId, Localize("Config directory"), &Button, Focused, Activate))
			{
				Storage()->GetCompletePath(IStorage::TYPE_SAVE, "", aBuf, sizeof(aBuf));
				Client()->ViewFile(aBuf);
			}
			Ui()->DoToolTip(&s_ConfigButtonId, &Button, Localize("Open the directory that contains the configuration and user files"));
			break;
		}
		case C::UPDATE_RATE:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClRefreshRate)
				Ui()->SetActiveItem(nullptr);
			str_copy(aBuf, " ");
			str_append(aBuf, Localize("Hz", "Hertz"));
			InactiveLimit = g_Config.m_ClRefreshRate;
			Ui()->DoScrollbarOption(&g_Config.m_ClRefreshRate, Ui()->Enabled() ? &g_Config.m_ClRefreshRate : &InactiveLimit, &Button, Localize("Update Rate"), 10, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE | CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT | CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE, aBuf, pValueRect);
			break;
		}
		case C::DEFAULT_ZOOM:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClDefaultZoom)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClDefaultZoom;
			if(Ui()->DoScrollbarOption(&g_Config.m_ClDefaultZoom, Ui()->Enabled() ? &g_Config.m_ClDefaultZoom : &InactiveLimit, &Button, Localize("Default zoom"), 0, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT, "", pValueRect) && Ui()->Enabled())
				GameClient()->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);
			break;
		}
		case C::BEST_RACE_DEMO:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClAutoRaceRecord, Localize("Save the best demo of each race"), g_Config.m_ClAutoRaceRecord, &Button, VisualEnabled))
				g_Config.m_ClAutoRaceRecord ^= 1;
			break;
		}
		case C::REPLAYS:
		{
			if(LucentSettings::GeneralCheckbox(Ui(), TextRender(), &g_Config.m_ClReplays, Localize("Enable replays"), g_Config.m_ClReplays, &Button, VisualEnabled))
			{
				g_Config.m_ClReplays ^= 1;
				if(Client()->State() == IClient::STATE_ONLINE)
					Client()->DemoRecorder_UpdateReplayRecorder();
			}
			break;
		}
		case C::REPLAY_LENGTH:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClReplayLength)
				Ui()->SetActiveItem(nullptr);
			InactiveLimit = g_Config.m_ClReplayLength;
			Ui()->DoScrollbarOption(&g_Config.m_ClReplayLength, Ui()->Enabled() && g_Config.m_ClReplays ? &g_Config.m_ClReplayLength : &InactiveLimit, &Button, Localize("Default length"), 10, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT, "", pValueRect);
			break;
		}
		case C::COUNT: break;
		}
	});
}

void CMenus::RenderThemeSelection(CUIRect MainView)
{
	const std::vector<CTheme> &vThemes = GameClient()->m_MenuBackground.GetThemes();

	int SelectedTheme = -1;
	for(int i = 0; i < (int)vThemes.size(); i++)
	{
		if(str_comp(vThemes[i].m_Name.c_str(), g_Config.m_ClMenuMap) == 0)
		{
			SelectedTheme = i;
			break;
		}
	}
	LucentSettings::GeneralThemes(Ui(), TextRender(), MainView, vThemes, SelectedTheme, [this](const CTheme &Theme) {
		str_copy(g_Config.m_ClMenuMap, Theme.m_Name.c_str());
		GameClient()->m_MenuBackground.LoadMenuBackground(Theme.m_HasDay, Theme.m_HasNight);
	});
}
