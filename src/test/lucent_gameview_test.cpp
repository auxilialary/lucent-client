#include "test.h"

#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/lucent/gameview/presentation.h>
#include <game/client/lucent/settings/presentation.h>
#include <game/client/lucent/settings/workspace.h>

#include <gtest/gtest.h>

namespace
{
	struct CConfigRestore
	{
		CConfig m_Saved = g_Config;
		~CConfigRestore() { g_Config = m_Saved; }
	};
	struct CConfigSession
	{
		std::unique_ptr<IKernel> m_pKernel{IKernel::Create()};
		IConsole *m_pConsole;
		IConfigManager *m_pConfig;
		explicit CConfigSession(IStorage *pStorage)
		{
			m_pKernel->RegisterInterface(pStorage, false);
			m_pConsole = CreateConsole(CFGFLAG_CLIENT).release();
			m_pKernel->RegisterInterface(m_pConsole);
			m_pConfig = CreateConfigManager();
			m_pKernel->RegisterInterface(m_pConfig);
			m_pConsole->Init();
			m_pConfig->Init();
		}
	};
}

TEST(LucentGameView, ConfigSurvivesFreshSessionsAndReset)
{
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_LucentCursorSize, 100);
		// Set every real registered GameView variable through its console command.
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	{ \
		char aCommand[128]; \
		str_format(aCommand, sizeof(aCommand), "%s %d", #ScriptName, Max); \
		Session.m_pConsole->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED); \
		EXPECT_EQ(g_Config.m_##Name, Max); \
	}
#include <game/client/lucent/gameview/config_variables.h>
#undef MACRO_CONFIG_INT
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_LucentCursorSize, 100);
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) EXPECT_EQ(g_Config.m_##Name, Max);
#include <game/client/lucent/gameview/config_variables.h>
#undef MACRO_CONFIG_INT
		Session.m_pConsole->ExecuteLine("lucent_cursor_size 1", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentCursorSize, 5);
		Session.m_pConsole->ExecuteLine("lucent_cursor_size 9999", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentCursorSize, 500);
		Session.m_pConsole->ExecuteLine("lucent_weapon_gun_x -9999", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_LucentWeaponGunX, -100);
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Session.m_pConfig->Reset(#ScriptName);
#include <game/client/lucent/gameview/config_variables.h>
#undef MACRO_CONFIG_INT
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) EXPECT_EQ(g_Config.m_##Name, Def);
#include <game/client/lucent/gameview/config_variables.h>
#undef MACRO_CONFIG_INT
	}
}

TEST(LucentSettingsWorkspace, SerializedNavigationSurvivesFreshClientSession)
{
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_ClLucentSettingsWorkspace, LucentSettings::WorkspaceDDNet);
		EXPECT_EQ(g_Config.m_ClLucentSettingsPage, 0);
		Session.m_pConsole->ExecuteLine("ui_settings_page 6", IConsole::CLIENT_ID_UNSPECIFIED);
		Session.m_pConsole->ExecuteLine("cl_lucent_settings_workspace 1", IConsole::CLIENT_ID_UNSPECIFIED);
		Session.m_pConsole->ExecuteLine("cl_lucent_settings_page 2", IConsole::CLIENT_ID_UNSPECIFIED);
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		EXPECT_EQ(g_Config.m_ClLucentSettingsWorkspace, LucentSettings::WorkspaceDDNet);
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
		EXPECT_EQ(g_Config.m_ClLucentSettingsWorkspace, LucentSettings::WorkspaceLucent);
		EXPECT_EQ(g_Config.m_ClLucentSettingsPage, 2);
		EXPECT_EQ(g_Config.m_UiSettingsPage, 6);
		Session.m_pConsole->ExecuteLine("cl_lucent_settings_workspace 0", IConsole::CLIENT_ID_UNSPECIFIED);
		EXPECT_EQ(g_Config.m_ClLucentSettingsPage, 2);
		EXPECT_EQ(g_Config.m_UiSettingsPage, 6);
		ASSERT_TRUE(Session.m_pConfig->Save());
	}
	{
		CConfigSession Session(Storage.get());
		ASSERT_TRUE(Session.m_pConsole->ExecuteFile(CONFIG_FILE, IConsole::CLIENT_ID_UNSPECIFIED));
		EXPECT_EQ(g_Config.m_ClLucentSettingsWorkspace, LucentSettings::WorkspaceDDNet);
		EXPECT_EQ(g_Config.m_ClLucentSettingsPage, 2);
		EXPECT_EQ(g_Config.m_UiSettingsPage, 6);
	}
}

TEST(LucentSettingsWorkspace, PreviewDoesNotChangeRuntimeCursor)
{
	CConfigRestore Restore;
	for(int Percent : {5, 42, 100, 500})
	{
		g_Config.m_LucentCursorSize = Percent;
		const float Size = 64.f * LucentGameView::CursorScale(g_Config.m_LucentCursorSize);
		const auto Preview = LucentSettings::CPreviewLayout::Calculate({0, 0, 332, 112}, {Size, Size}, {0, 0}, true);
		EXPECT_EQ(g_Config.m_LucentCursorSize, Percent);
		EXPECT_FLOAT_EQ(Preview.m_Sprite.w, Size * Preview.m_Fit);
		EXPECT_LE(Preview.m_Fit, 1.f);
		if(Percent == 5)
		{
			EXPECT_GT(Preview.m_Magnification, 1.f);
			EXPECT_FLOAT_EQ(Preview.m_Sprite.w, Size);
		}
	}
}

TEST(LucentSettingsWorkspace, CursorWheelUsesRegisteredConfig)
{
	CConfigRestore Restore;
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;
	auto Storage = Info.CreateTestStorage();
	ASSERT_NE(Storage, nullptr);
	CConfigSession Session(Storage.get());
	for(int Start : {5, 100, 500})
		for(int Direction : {-1, 1})
		{
			g_Config.m_LucentCursorSize = Start;
			const int Next = LucentSettings::CPreviewLayout::WheelValue(Start, Direction);
			char aCommand[64];
			str_format(aCommand, sizeof(aCommand), "lucent_cursor_size %d", Next);
			Session.m_pConsole->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED);
			EXPECT_EQ(g_Config.m_LucentCursorSize, Next);
			EXPECT_GE(Next, 5);
			EXPECT_LE(Next, 500);
			EXPECT_FLOAT_EQ(LucentGameView::CursorScale(g_Config.m_LucentCursorSize), Next / 100.f);
		}
}
