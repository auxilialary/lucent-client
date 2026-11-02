/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/dbg.h>
#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/console.h>
#include <game/client/components/emoticon.h>
#include <game/client/components/skins7.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/settings/skin_gallery.h>
#include <game/client/lucent/settings/skin_preview.h>
#include <game/client/lucent/settings/skins.h>
#include <game/client/lucent/ui.h>
#include <game/client/lucent/ui/selection_corners.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <map>
#include <vector>

void CMenus::RenderSettingsTee7(CUIRect MainView)
{
	CUIRect SkinPreview, NormalSkinPreview, RedTeamSkinPreview, BlueTeamSkinPreview, QuickSearch, DirectoryButton, RefreshButton, SaveDeleteButton, LeftTab, RightTab;
	static auto s_ResourceTime = GameClient()->m_Skins7.LastRefreshTime();
	if(s_ResourceTime != GameClient()->m_Skins7.LastRefreshTime())
	{
		m_pSelectedSkin = nullptr;
		m_SkinList7LastRefreshTime = std::nullopt;
		m_SkinPartsList7LastRefreshTime = std::nullopt;
		s_ResourceTime = GameClient()->m_Skins7.LastRefreshTime();
	}

	m_pSelectedSkin = nullptr;
	for(const auto &Skin : GameClient()->m_Skins7.GetSkins())
		if(!str_comp(Skin.m_aName, CSkins7::ms_apSkinNameVariables[int(m_Dummy)]))
		{
			m_pSelectedSkin = &Skin;
			break;
		}
	const auto Layout = LucentSettings::CSkinManagerLayout::Calculate(MainView);
	CUIRect Profile = Layout.m_Profile;
	MainView = Layout.m_Gallery;
	CUIRect Tabs;
	MainView.HSplitTop(28, &Tabs, &MainView);
	Tabs.VSplitMid(&LeftTab, &RightTab, 8);
	static CButtonContainer s_BasicTabButton, s_CustomTabButton;
	if(LucentUi::FilterTab(Ui(), TextRender(), &s_BasicTabButton, Localize("Skins"), LeftTab, !m_CustomSkinMenu))
		m_CustomSkinMenu = false;
	if(LucentUi::FilterTab(Ui(), TextRender(), &s_CustomTabButton, Localize("Custom"), RightTab, m_CustomSkinMenu))
	{
		m_CustomSkinMenu = true;
		if(m_pSelectedSkin)
		{
			if(m_pSelectedSkin->m_Flags & CSkins7::SKINFLAG_STANDARD)
			{
				m_SkinNameInput.Set("copy_");
				m_SkinNameInput.Append(m_pSelectedSkin->m_aName);
			}
			else
				m_SkinNameInput.Set(m_pSelectedSkin->m_aName);
		}
	}
	MainView.HSplitTop(28, &QuickSearch, &MainView);
	CUIRect Footer;
	Profile.HSplitBottom(std::min(28.f, Profile.h), &Profile, &Footer);
	Footer.VSplitRight(28, &DirectoryButton, &RefreshButton);
	static CScrollRegion s_ProfileScroll;
	CScrollRegionParams ProfileParams;
	ProfileParams.m_ScrollbarThickness = 8;
	ProfileParams.m_ScrollbarMargin = 1;
	s_ProfileScroll.Begin(&Profile, &ProfileParams);
	Profile.HSplitTop(18, &SkinPreview, &Profile);
	CUIRect Tees;
	Profile.HSplitTop(48, &Tees, &Profile);
	Tees.VSplitLeft(Tees.w / 3, &NormalSkinPreview, &Tees);
	Tees.VSplitMid(&RedTeamSkinPreview, &BlueTeamSkinPreview);
	CUIRect SkinNameLabel;
	Profile.HSplitTop(22, &SkinNameLabel, &Profile);
	Profile.HSplitTop(28, &SaveDeleteButton, &Profile);
	// validate skin parts for solo mode
	char aSkinParts[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_ARRAY_SIZE];
	char *apSkinPartsPtr[protocol7::NUM_SKINPARTS];
	int aUCCVars[protocol7::NUM_SKINPARTS];
	int aColorVars[protocol7::NUM_SKINPARTS];
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		str_copy(aSkinParts[Part], CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], protocol7::MAX_SKIN_ARRAY_SIZE);
		apSkinPartsPtr[Part] = aSkinParts[Part];
		aUCCVars[Part] = *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part];
		aColorVars[Part] = *CSkins7::ms_apColorVariables[(int)m_Dummy][Part];
	}
	GameClient()->m_Skins7.ValidateSkinParts(apSkinPartsPtr, aUCCVars, aColorVars, 0);

	CTeeRenderInfo OwnSkinInfo;
	OwnSkinInfo.m_Size = 38.0f;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		GameClient()->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false)->ApplyTo(OwnSkinInfo.m_aSixup[g_Config.m_ClDummy]);
		GameClient()->m_Skins7.ApplyColorTo(OwnSkinInfo.m_aSixup[g_Config.m_ClDummy], aUCCVars[Part], aColorVars[Part], Part);
	}

	char aBuf[128 + IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Your skin"));
	Ui()->DoLabel(&SkinPreview, aBuf, 14.0f, TEXTALIGN_ML);

	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &OwnSkinInfo, OffsetToMid);
	{
		// interactive tee: tee looking towards cursor, and it is happy when you touch it
		const vec2 TeePosition = NormalSkinPreview.Center() + OffsetToMid;
		const vec2 DeltaPosition = Ui()->MousePos() - TeePosition;
		const float Distance = length(DeltaPosition);
		const float InteractionDistance = 20.0f;
		const vec2 TeeDirection = Distance < InteractionDistance ? normalize(vec2(DeltaPosition.x, std::max(DeltaPosition.y, 0.5f))) : normalize(DeltaPosition);
		const int TeeEmote = Distance < InteractionDistance ? EMOTE_HAPPY : (m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, TeeEmote, TeeDirection, TeePosition, CUIRect::DrawOpacity());
		static char s_InteractiveTeeButtonId;
		if(Distance < InteractionDistance && Ui()->DoButtonLogic(&s_InteractiveTeeButtonId, 0, &NormalSkinPreview, BUTTONFLAG_LEFT))
		{
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_PLAYER_SPAWN, 1.0f);
		}
	}

	// validate skin parts for team game mode
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		str_copy(aSkinParts[Part], CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], protocol7::MAX_SKIN_ARRAY_SIZE);
		apSkinPartsPtr[Part] = aSkinParts[Part];
		aUCCVars[Part] = *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part];
		aColorVars[Part] = *CSkins7::ms_apColorVariables[(int)m_Dummy][Part];
	}
	GameClient()->m_Skins7.ValidateSkinParts(apSkinPartsPtr, aUCCVars, aColorVars, GAMEFLAG_TEAMS);

	CTeeRenderInfo TeamSkinInfo;
	TeamSkinInfo.m_Size = OwnSkinInfo.m_Size;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		GameClient()->m_Skins7.FindSkinPart(Part, apSkinPartsPtr[Part], false)->ApplyTo(TeamSkinInfo.m_aSixup[g_Config.m_ClDummy]);
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aUseCustomColors[Part] = aUCCVars[Part];
	}

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = GameClient()->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_RED, Part);
	}
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes, vec2(1, 0), RedTeamSkinPreview.Center() + OffsetToMid, CUIRect::DrawOpacity());

	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		TeamSkinInfo.m_aSixup[g_Config.m_ClDummy].m_aColors[Part] = GameClient()->m_Skins7.GetTeamColor(aUCCVars[Part], aColorVars[Part], TEAM_BLUE, Part);
	}
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeamSkinInfo, m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes, vec2(-1, 0), BlueTeamSkinPreview.Center() + OffsetToMid, CUIRect::DrawOpacity());
	SLabelProperties NameProps;
	NameProps.m_MaxWidth = SkinNameLabel.w;
	NameProps.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&SkinNameLabel, CSkins7::ms_apSkinNameVariables[int(m_Dummy)], 11, TEXTALIGN_ML, NameProps);
	if(!m_CustomSkinMenu)
		for(int Part : {protocol7::SKINPART_BODY, protocol7::SKINPART_FEET})
		{
			CUIRect Row, Toggle, Color;
			Profile.HSplitTop(30, &Row, &Profile);
			Row.VSplitMid(&Toggle, &Color, 6);
			int *pCustom = CSkins7::ms_apUCCVariables[int(m_Dummy)][Part];
			unsigned *pColor = CSkins7::ms_apColorVariables[int(m_Dummy)][Part];
			if(LucentUi::FilledCheckBox(Ui(), pCustom, Localize(CSkins7::ms_apSkinPartNamesLocalized[Part], "skins"), *pCustom, Toggle, Ui()->Enabled(), true))
			{
				*pCustom ^= 1;
				SetNeedSendInfo();
			}
			static CButtonContainer s_ColorButtons[protocol7::NUM_SKINPARTS];
			LucentUi::CInteractionScope ColorInput(Ui(), *pCustom != 0);
			CUIRect Swatch;
			Color.VSplitRight(std::min(24.f, Color.w * .2f), nullptr, &Swatch);
			Swatch.Margin(4, &Swatch);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_ColorButtons[Part], Localize("Color"), Color, false, 11, TEXTALIGN_ML, false))
			{
				const bool ProfileId = m_Dummy;
				LucentSettings::OpenSkinColorPopup(Ui(), TextRender(), Swatch, pColor, false, ColorHSLA::DARKEST_LGT7, [this, ProfileId]() {
					const bool Previous = m_Dummy;
					m_Dummy = ProfileId;
					SetNeedSendInfo();
					m_Dummy = Previous;
				});
			}
			Swatch.Draw(color_cast<ColorRGBA>(ColorHSLA(*pColor).UnclampLighting(ColorHSLA::DARKEST_LGT7)).WithAlpha(1), IGraphics::CORNER_ALL, LucentUi::RADIUS);
		}
	CUIRect Eyes, Emotions;
	Profile.HSplitTop(18, &Emotions, &Profile);
	Ui()->DoLabel(&Emotions, Localize("Emotions"), 11, TEXTALIGN_ML);
	Profile.HSplitTop(32, &Eyes, &Profile);
	const float EyeWidth = Eyes.w / NUM_EMOTES;
	static CButtonContainer s_Eyes[NUM_EMOTES];
	static std::array<std::array<LucentStyle::Motion::CTransition, NUM_EMOTES>, 2> s_EyeSelection;
	std::array<CUIRect, NUM_EMOTES> EyeRects;
	int *pEyes = m_Dummy ? &g_Config.m_ClDummyDefaultEyes : &g_Config.m_ClPlayerDefaultEyes;
	for(int Eye = 0; Eye < NUM_EMOTES; ++Eye)
	{
		CUIRect Button;
		Eyes.VSplitLeft(EyeWidth, &Button, &Eyes);
		EyeRects[Eye] = Button;
		if(LucentUi::Button(Ui(), TextRender(), &s_Eyes[Eye], "", Button, false, true, true))
		{
			*pEyes = Eye;
			if(int(m_Dummy) == g_Config.m_ClDummy)
				GameClient()->m_Emoticon.EyeEmote(Eye);
		}
		LucentSettings::RenderSkinPreview(RenderTools(), OwnSkinInfo, Button, Eye);
	}

	for(int Eye = 0; Eye < NUM_EMOTES; ++Eye)
		LucentUi::SelectionCorners(Ui(), EyeRects[Eye], s_EyeSelection[int(m_Dummy)][Eye], *pEyes == Eye);

	CUIRect Random;
	Profile.HSplitTop(28, &Random, &Profile);
	static CButtonContainer s_RandomSkinButton;
	if(LucentUi::TextButton(Ui(), TextRender(), &s_RandomSkinButton, Localize("Create a random skin"), Random, Ui()->CheckActiveItem(&s_RandomSkinButton), 11, TEXTALIGN_ML, false))
	{
		GameClient()->m_Skins7.RandomizeSkin(m_Dummy);
		SetNeedSendInfo();
	}

	if(m_CustomSkinMenu)
	{
		static CButtonContainer s_CustomSkinSaveButton;
		if(LucentUi::Button(Ui(), TextRender(), &s_CustomSkinSaveButton, Localize("Save"), SaveDeleteButton))
		{
			m_Popup = POPUP_SAVE_SKIN;
			m_SkinNameInput.SelectAll();
			Ui()->SetActiveItem(&m_SkinNameInput);
		}
	}
	else if(m_pSelectedSkin && (m_pSelectedSkin->m_Flags & CSkins7::SKINFLAG_STANDARD) == 0)
	{
		static CButtonContainer s_CustomSkinDeleteButton;
		if(LucentUi::Button(Ui(), TextRender(), &s_CustomSkinDeleteButton, Localize("Delete"), SaveDeleteButton) || Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE))
		{
			str_format(aBuf, sizeof(aBuf), Localize("Are you sure that you want to delete '%s'?"), m_pSelectedSkin->m_aName);
			str_copy(m_aDeleteSkinName7, m_pSelectedSkin->m_aName);
			PopupConfirm(Localize("Delete skin"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDeleteSkin7);
		}
	}

	s_ProfileScroll.AddRect({Profile.x, Profile.y, Profile.w, 1});
	s_ProfileScroll.End();
	if(m_CustomSkinMenu)
		RenderSettingsTeeCustom7(MainView);
	else
		RenderSkinSelection7(MainView);
	static CLineInput s_SkinFilterInput(g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString));
	if(Ui()->DoEditBox_Search(&s_SkinFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive()))
	{
		m_SkinList7LastRefreshTime = std::nullopt;
		m_SkinPartsList7LastRefreshTime = std::nullopt;
	}

	static CButtonContainer s_DirectoryButton;
	if(LucentUi::TextButton(Ui(), TextRender(), &s_DirectoryButton, Localize("Skins directory"), DirectoryButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins7", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("skins7", IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}
	Ui()->DoToolTip(&s_DirectoryButton, &DirectoryButton, Localize("Open the directory to add custom skins"));

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static CButtonContainer s_SkinRefreshButton;
	if(LucentUi::IconButton(Ui(), TextRender(), &s_SkinRefreshButton, FontIcon::ARROW_ROTATE_RIGHT, Localize("Refresh"), RefreshButton) ||
		(Ui()->Enabled() && !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R)))))
	{
		// reset render flags for possible loading screen
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		GameClient()->RefreshSkins(CSkinDescriptor::FLAG_SEVEN);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::PopupConfirmDeleteSkin7()
{
	const auto &Skins = GameClient()->m_Skins7.GetSkins();
	const auto Selected = std::find_if(Skins.begin(), Skins.end(), [&](const auto &Skin) { return !str_comp(Skin.m_aName, m_aDeleteSkinName7); });
	if(Selected == Skins.end() || !GameClient()->m_Skins7.RemoveSkin(&*Selected))
	{
		PopupMessage(Localize("Error"), Localize("Unable to delete skin"), Localize("Ok"));
		return;
	}

	m_pSelectedSkin = nullptr;
	m_DeletedSkinIndex7 = m_SelectedSkinIndex7;
}

void CMenus::RenderSettingsTeeCustom7(CUIRect MainView)
{
	static std::array<CButtonContainer, protocol7::NUM_SKINPARTS> s_PartButtons;
	const int Columns = std::clamp<int>(int(MainView.w / 100), 1, protocol7::NUM_SKINPARTS);
	const int Rows = (protocol7::NUM_SKINPARTS + Columns - 1) / Columns;
	CUIRect Tabs, Controls;
	MainView.HSplitTop(Rows * 28.f, &Tabs, &MainView);
	for(int Row = 0; Row < Rows; ++Row)
	{
		CUIRect Line;
		Tabs.HSplitTop(28, &Line, &Tabs);
		const int Count = std::min(Columns, protocol7::NUM_SKINPARTS - Row * Columns);
		const float Width = Line.w / Count;
		for(int Column = 0; Column < Count; ++Column)
		{
			const int Part = Row * Columns + Column;
			CUIRect Button;
			Line.VSplitLeft(Width, &Button, &Line);
			if(LucentUi::FilterTab(Ui(), TextRender(), &s_PartButtons[Part], Localize(CSkins7::ms_apSkinPartNamesLocalized[Part], "skins"), Button, m_TeePartSelected == Part))
				m_TeePartSelected = Part;
		}
	}
	MainView.HSplitTop(30, &Controls, &MainView);
	CUIRect Toggle, Color;
	Controls.VSplitMid(&Toggle, &Color, 6);
	int *pCustom = CSkins7::ms_apUCCVariables[int(m_Dummy)][m_TeePartSelected];
	unsigned *pColor = CSkins7::ms_apColorVariables[int(m_Dummy)][m_TeePartSelected];
	if(LucentUi::FilledCheckBox(Ui(), pCustom, Localize("Custom colors"), *pCustom, Toggle, Ui()->Enabled(), true))
	{
		*pCustom ^= 1;
		SetNeedSendInfo();
	}
	static CButtonContainer s_ColorButton;
	{
		LucentUi::CInteractionScope ColorInput(Ui(), *pCustom != 0);
		CUIRect Swatch;
		Color.VSplitRight(std::min(24.f, Color.w * .2f), nullptr, &Swatch);
		Swatch.Margin(4, &Swatch);
		if(LucentUi::TextButton(Ui(), TextRender(), &s_ColorButton, Localize("Color"), Color, false, 11, TEXTALIGN_ML, false))
		{
			const bool Profile = m_Dummy;
			const bool Alpha = m_TeePartSelected == protocol7::SKINPART_MARKING;
			LucentSettings::OpenSkinColorPopup(Ui(), TextRender(), Swatch, pColor, Alpha, ColorHSLA::DARKEST_LGT7, [this, Profile]() {
				const bool Previous = m_Dummy;
				m_Dummy = Profile;
				SetNeedSendInfo();
				m_Dummy = Previous;
			});
		}
		Swatch.Draw(color_cast<ColorRGBA>(ColorHSLA(*pColor, m_TeePartSelected == protocol7::SKINPART_MARKING).UnclampLighting(ColorHSLA::DARKEST_LGT7)).WithAlpha(1), IGraphics::CORNER_ALL, LucentUi::RADIUS);
	}
	RenderSkinPartSelection7(MainView);
}

void CMenus::RenderSkinSelection7(CUIRect MainView)
{
	static bool s_FavoritesOnly = false;
	CUIRect Filter;
	MainView.HSplitTop(28, &Filter, &MainView);
	if(LucentUi::FilledCheckBox(Ui(), &s_FavoritesOnly, Localize("Favorites only"), s_FavoritesOnly, Filter))
		s_FavoritesOnly = !s_FavoritesOnly;
	std::vector<const CSkins7::CSkin *> Skins;
	m_pSelectedSkin = nullptr;
	m_SelectedSkinIndex7 = -1;
	for(const auto &Skin : GameClient()->m_Skins7.GetSkins())
	{
		if(Skin.m_Flags & CSkins7::SKINFLAG_SPECIAL)
			continue;
		if(g_Config.m_ClSkinFilterString[0] && !str_utf8_find_nocase(Skin.m_aName, g_Config.m_ClSkinFilterString))
			continue;
		if(s_FavoritesOnly && !GameClient()->m_Skins.IsFavorite(Skin.m_aName))
			continue;
		if(!str_comp(Skin.m_aName, CSkins7::ms_apSkinNameVariables[int(m_Dummy)]))
		{
			m_pSelectedSkin = &Skin;
			m_SelectedSkinIndex7 = Skins.size();
		}
		Skins.push_back(&Skin);
	}
	static std::map<std::string, CButtonContainer> s_FavoriteIds;
	const auto SelectSkin = [&](int Index) {
		m_pSelectedSkin = Skins[Index];
		m_SelectedSkinIndex7 = Index;
		str_copy(CSkins7::ms_apSkinNameVariables[int(m_Dummy)], m_pSelectedSkin->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
		{
			str_copy(CSkins7::ms_apSkinVariables[int(m_Dummy)][Part], m_pSelectedSkin->m_apParts[Part]->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
			*CSkins7::ms_apUCCVariables[int(m_Dummy)][Part] = m_pSelectedSkin->m_aUseCustomColors[Part];
			*CSkins7::ms_apColorVariables[int(m_Dummy)][Part] = m_pSelectedSkin->m_aPartColors[Part];
		}
		SetNeedSendInfo();
	};
	if(m_DeletedSkinIndex7 >= 0)
	{
		const int Index = std::min(m_DeletedSkinIndex7, int(Skins.size()) - 1);
		m_DeletedSkinIndex7 = -1;
		if(Index >= 0)
			SelectSkin(Index);
	}
	const auto GetItem = [&](int Index) {
		const auto *pSkin = Skins[Index];
		return LucentSettings::CSkinGalleryItem{pSkin->m_aName, pSkin, &s_FavoriteIds[pSkin->m_aName], Index == m_SelectedSkinIndex7, GameClient()->m_Skins.IsFavorite(pSkin->m_aName)};
	};
	const auto Paint = [&](int Index, CUIRect Rect) {
		CTeeRenderInfo Info;
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
		{
			Skins[Index]->m_apParts[Part]->ApplyTo(Info.m_aSixup[g_Config.m_ClDummy]);
			GameClient()->m_Skins7.ApplyColorTo(Info.m_aSixup[g_Config.m_ClDummy], Skins[Index]->m_aUseCustomColors[Part], Skins[Index]->m_aPartColors[Part], Part);
		}
		LucentSettings::RenderSkinPreview(RenderTools(), Info, Rect, m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes);
	};
	const auto Favorite = [&](int Index) {
		const char *pName = Skins[Index]->m_aName;
		if(GameClient()->m_Skins.IsFavorite(pName))
			GameClient()->m_Skins.RemoveFavorite(pName);
		else
			GameClient()->m_Skins.AddFavorite(pName);
	};
	static bool s_LastDummy = false;
	static std::string s_LastSkin;
	const char *pName = CSkins7::ms_apSkinNameVariables[int(m_Dummy)];
	const bool Scroll = s_LastDummy != m_Dummy || s_LastSkin != pName || m_SkinList7LastRefreshTime != GameClient()->m_Skins7.LastRefreshTime();
	s_LastDummy = m_Dummy;
	s_LastSkin = pName;
	LucentSettings::RenderSkinGallery(Ui(), TextRender(), MainView, Skins.size(), Scroll, GetItem, Paint, SelectSkin, Favorite, LucentSettings::SkinGalleryKeyboardFocused());
	m_SkinList7LastRefreshTime = GameClient()->m_Skins7.LastRefreshTime();
}

void CMenus::RenderSkinPartSelection7(CUIRect MainView)
{
	static std::vector<const CSkins7::CSkinPart *> s_avpList[protocol7::NUM_SKINPARTS];
	static CListBox s_ListBox;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		if(!m_SkinPartsList7LastRefreshTime.has_value() || m_SkinPartsList7LastRefreshTime.value() != GameClient()->m_Skins7.LastRefreshTime())
		{
			s_avpList[Part].clear();
			for(const CSkins7::CSkinPart &SkinPart : GameClient()->m_Skins7.GetSkinParts(Part))
			{
				if((SkinPart.m_Flags & CSkins7::SKINFLAG_SPECIAL) != 0)
					continue;

				if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(SkinPart.m_aName, g_Config.m_ClSkinFilterString))
					continue;

				s_avpList[Part].emplace_back(&SkinPart);
			}
		}
	}

	m_SkinPartsList7LastRefreshTime = GameClient()->m_Skins7.LastRefreshTime();
	int OldSelected = -1;
	for(int i = 0; i < (int)s_avpList[m_TeePartSelected].size(); ++i)
	{
		const CSkins7::CSkinPart *pPart = s_avpList[m_TeePartSelected][i];
		if(!str_comp(pPart->m_aName, CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected]))
		{
			OldSelected = i;
			break;
		}
	}
	s_ListBox.SetColors(LucentUi::Accent(LucentStyle::Style::Selected), LucentUi::Accent(LucentStyle::Style::Hover));
	s_ListBox.SetActive(Ui()->Enabled() && !CLineInput::GetActiveInput());
	s_ListBox.SetScrollbarWidth(8);
	s_ListBox.SetScrollbarMargin(1);
	s_ListBox.DoStart(72.0f, s_avpList[m_TeePartSelected].size(), LucentSettings::CSkinGalleryGeometry::Columns(MainView.w), 1, OldSelected, &MainView, false);

	for(const CSkins7::CSkinPart *pPart : s_avpList[m_TeePartSelected])
	{
		CListboxItem Item = s_ListBox.DoNextItem(pPart);
		if(!Item.m_Visible)
			continue;

		CUIRect Label;
		Item.m_Rect.Margin(5.0f, &Item.m_Rect);
		Item.m_Rect.HSplitBottom(12.0f, &Item.m_Rect, &Label);

		CTeeRenderInfo Info;
		for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
		{
			const CSkins7::CSkinPart *pPreviewPart = (m_TeePartSelected == Part ? pPart : GameClient()->m_Skins7.FindSkinPart(Part, CSkins7::ms_apSkinVariables[(int)m_Dummy][Part], false));
			pPreviewPart->ApplyTo(Info.m_aSixup[g_Config.m_ClDummy]);
			GameClient()->m_Skins7.ApplyColorTo(Info.m_aSixup[g_Config.m_ClDummy], *CSkins7::ms_apUCCVariables[(int)m_Dummy][Part], *CSkins7::ms_apColorVariables[(int)m_Dummy][Part], Part);
		}
		Info.m_Size = 50.0f;

		if(m_TeePartSelected == protocol7::SKINPART_HANDS)
		{
			// RenderTools()->RenderTeeHand(&Info, Item.m_Rect.Center(), vec2(1.0f, 0.0f), -pi*0.5f, vec2(18, 0));
		}
		int TeePartEmote = EMOTE_NORMAL;
		if(m_TeePartSelected == protocol7::SKINPART_EYES)
		{
			TeePartEmote = (int)(Client()->GlobalTime() * 0.5f) % NUM_EMOTES;
		}
		RenderTools()->RenderIdleTeeAtCenter(&Info, Item.m_Rect.Center(), TeePartEmote, vec2(1, 0), CUIRect::DrawOpacity());

		SLabelProperties NameProps;
		NameProps.m_MaxWidth = Label.w;
		NameProps.m_EllipsisAtEnd = true;
		NameProps.m_EnableWidthCheck = false;
		Ui()->DoLabel(&Label, pPart->m_aName, 12, TEXTALIGN_MC, NameProps);
		Ui()->DoToolTip(pPart, &Item.m_Rect, pPart->m_aName);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != -1 && NewSelected != OldSelected)
	{
		str_copy(CSkins7::ms_apSkinVariables[(int)m_Dummy][m_TeePartSelected], s_avpList[m_TeePartSelected][NewSelected]->m_aName, protocol7::MAX_SKIN_ARRAY_SIZE);
		CSkins7::ms_apSkinNameVariables[m_Dummy][0] = '\0';
		SetNeedSendInfo();
	}
}
