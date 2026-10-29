/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/console.h>
#include <game/client/components/skins.h>
#include <game/client/components/skins7.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/settings/player.h>
#include <game/client/render.h>
#include <game/localization.h>

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	LucentSettings::CPlayerBindings Bindings;
	Bindings.m_Profiles = {{{g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), g_Config.m_PlayerClan, sizeof(g_Config.m_PlayerClan), &g_Config.m_PlayerCountry, Client()->PlayerName(), {}},
		{g_Config.m_ClDummyName, sizeof(g_Config.m_ClDummyName), g_Config.m_ClDummyClan, sizeof(g_Config.m_ClDummyClan), &g_Config.m_ClDummyCountry, Client()->DummyName(), {}}}};
	for(int Profile = 0; Profile < 2; ++Profile)
		if(Client()->State() == IClient::STATE_ONLINE && GameClient()->m_aNextChangeInfo[Profile] > Client()->GameTick(Profile))
		{
			char aChangeInfo[128], aTimeLeft[32];
			str_format(aTimeLeft, sizeof(aTimeLeft), Localize("%ds left"), (GameClient()->m_aNextChangeInfo[Profile] - Client()->GameTick(Profile) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
			str_format(aChangeInfo, sizeof(aChangeInfo), "%s: %s", Localize("Player info change cooldown"), aTimeLeft);
			Bindings.m_Profiles[Profile].m_Cooldown = aChangeInfo;
		}
	Bindings.m_Changed = [this](bool Dummy) {
		// Pending edits and popup selection always notify the profile they were bound to.
		const bool Previous = m_Dummy;
		m_Dummy = Dummy;
		SetNeedSendInfo();
		m_Dummy = Previous;
	};
	Bindings.m_RenderTee = [this](bool Dummy, CUIRect Rect, float Alpha) {
		if(Rect.w <= 0 || Rect.h <= 0)
			return;
		CTeeRenderInfo Info;
		Info.m_Size = std::min(30.f, std::min(Rect.w, Rect.h));
		if(Client()->IsSixup())
		{
			char aaParts[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_ARRAY_SIZE];
			char *apParts[protocol7::NUM_SKINPARTS];
			int aCustom[protocol7::NUM_SKINPARTS], aColors[protocol7::NUM_SKINPARTS];
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
			{
				str_copy(aaParts[Part], CSkins7::ms_apSkinVariables[int(Dummy)][Part], sizeof(aaParts[Part]));
				apParts[Part] = aaParts[Part];
				aCustom[Part] = *CSkins7::ms_apUCCVariables[int(Dummy)][Part];
				aColors[Part] = *CSkins7::ms_apColorVariables[int(Dummy)][Part];
			}
			GameClient()->m_Skins7.ValidateSkinParts(apParts, aCustom, aColors, 0);
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
			{
				GameClient()->m_Skins7.FindSkinPart(Part, apParts[Part], false)->ApplyTo(Info.m_aSixup[g_Config.m_ClDummy]);
				GameClient()->m_Skins7.ApplyColorTo(Info.m_aSixup[g_Config.m_ClDummy], aCustom[Part], aColors[Part], Part);
			}
		}
		else
		{
			const char *pName = Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
			const auto *pSkin = GameClient()->m_Skins.FindContainerOrNullptr(*pName ? pName : "default");
			Info.Apply(pSkin && !pSkin->IsSpecial() && pSkin->Skin() ? pSkin->Skin().get() : GameClient()->m_Skins.Find("default"));
			Info.ApplyColors(Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor,
				Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody, Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet);
		}
		const int Eyes = Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
		RenderTools()->RenderIdleTeeAtCenter(&Info, Rect.Center(), Eyes, vec2(1, 0), Alpha);
	};
	LucentSettings::RenderPlayer(Ui(), TextRender(), MainView, &GameClient()->m_CountryFlags, m_Dummy, Bindings, GameClient()->m_GameConsole.IsActive());
}
