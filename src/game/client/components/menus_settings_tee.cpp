/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
#include "menus.h"

#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/console.h>
#include <game/client/components/emoticon.h>
#include <game/client/components/skins.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/settings/skin_gallery.h>
#include <game/client/lucent/settings/skin_preview.h>
#include <game/client/lucent/settings/skins.h>
#include <game/client/lucent/skins/texture_quality.h>
#include <game/client/lucent/ui.h>
#include <game/client/lucent/ui/selection_corners.h>
#include <game/localization.h>

#include <vector>

void CMenus::RenderSettingsTee(CUIRect MainView)
{
	using C = LucentSettings::ESkinControl;
	bool Refresh = false, RefreshBoth = false;
	if(!g_Config.m_ClDownloadCommunitySkins && str_comp(g_Config.m_ClSkinDownloadUrl, "https://skins.ddnet.org/skin/"))
	{
		str_copy(g_Config.m_ClSkinDownloadUrl, "https://skins.ddnet.org/skin/");
		Refresh = true;
	}
	static std::array<CLineInput, 2> s_Names;
	s_Names[0].SetBuffer(g_Config.m_ClPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
	s_Names[1].SetBuffer(g_Config.m_ClDummySkin, sizeof(g_Config.m_ClDummySkin));
	static CLineInput s_CommunityUrl(g_Config.m_ClSkinCommunityDownloadUrl, sizeof(g_Config.m_ClSkinCommunityDownloadUrl));
	static bool s_SourceDirty = false;
	static std::array<CButtonContainer, 6> s_Eyes;
	static std::array<CButtonContainer, 12> s_Buttons;
	const auto Label = [&](CUIRect Rect, const char *pText, ColorRGBA Color = LucentStyle::Style::Text, bool Elide = false) {
		const auto Previous = TextRender()->GetTextColor();
		TextRender()->TextColor(Color);
		SLabelProperties Props;
		Props.m_MaxWidth = std::max(0.f, Rect.w);
		Props.m_EnableWidthCheck = false;
		Props.m_EllipsisAtEnd = Elide;
		Ui()->DoLabel(&Rect, pText, 11, TEXTALIGN_ML, Props);
		TextRender()->TextColor(Previous);
	};
	const auto Notify = [&]() {
		SetNeedSendInfo();
		GameClient()->m_Skins.SkinList().ForceRefresh();
		m_SkinListScrollToSelected = true;
	};
	const auto GetInfo = [&]() {
		CTeeRenderInfo Info;
		const char *pName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
		const auto *pContainer = GameClient()->m_Skins.FindContainerOrNullptr(*pName ? pName : "default");
		Info.Apply(pContainer && !pContainer->IsSpecial() && pContainer->Skin() ? pContainer->Skin().get() : GameClient()->m_Skins.Find("default"));
		Info.ApplyColors(m_Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor, m_Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody, m_Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet);
		return Info;
	};
	const auto Check = [&](int *pValue, const char *pText, CUIRect Row) { return LucentUi::FilledCheckBox(Ui(), pValue, pText, *pValue, Row, Ui()->Enabled(), true, .87f); };
	const auto Control = [&](C Control, CUIRect Row) {
		int *pCustom = m_Dummy ? &g_Config.m_ClDummyUseCustomColor : &g_Config.m_ClPlayerUseCustomColor;
		int *pEyes = m_Dummy ? &g_Config.m_ClDummyDefaultEyes : &g_Config.m_ClPlayerDefaultEyes;
		CUIRect Caption, Value, Rail, Body = Row;
		const auto Field = [&](const char *pCaption, CLineInput &Input) {
			Body.HSplitTop(20, &Caption, &Body);
			Label(Caption, pCaption, LucentStyle::Style::MutedText);
			Body.HSplitTop(28, &Body, nullptr);
			return Ui()->DoClearableEditBox(&Input, &Body, 11);
		};
		switch(Control)
		{
		case C::PREVIEW: LucentSettings::RenderSkinPreview(RenderTools(), GetInfo(), Row, *pEyes); break;
		case C::NAME:
			s_Names[int(m_Dummy)].SetEmptyText("default");
			if(Ui()->DoClearableEditBox(&s_Names[int(m_Dummy)], &Row, 11))
				Notify();
			break;
		case C::STATUS:
		{
			const char *pName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
			const auto *pSkin = GameClient()->m_Skins.FindContainerOrNullptr(*pName ? pName : "default");
			const char *pMessage = nullptr;
			ColorRGBA MessageColor = LucentStyle::Style::MutedText;
			if(!pSkin || pSkin->IsSpecial())
			{
				pMessage = Localize("This skin name cannot be used.");
				MessageColor = LucentStyle::Style::ErrorText;
			}
			else if(pSkin->State() == CSkins::CSkinContainer::EState::ERROR)
			{
				pMessage = Localize("Skin could not be loaded due to an error. Check the local console for details.");
				MessageColor = LucentStyle::Style::ErrorText;
			}
			else if(pSkin->State() == CSkins::CSkinContainer::EState::NOT_FOUND)
				pMessage = Localize("Skin could not be found.");
			else if(pSkin->State() != CSkins::CSkinContainer::EState::LOADED)
				Ui()->RenderProgressSpinner(Row.Center(), 5);
			else if(g_Config.m_Debug)
			{
				const auto S = GameClient()->m_Skins.LoadingStats();
				char aStats[128];
				str_format(aStats, sizeof(aStats), "loaded: %zu / pending: %zu / error: %zu", S.m_NumLoaded, S.m_NumPending, S.m_NumError);
				Label(Row, aStats, LucentStyle::Style::Text, true);
			}
			if(pMessage)
			{
				Label(Row, pMessage, MessageColor, true);
				Ui()->DoToolTip(pName, &Row, pMessage);
			}
			break;
		}
		case C::CUSTOM_COLORS:
			if(Check(pCustom, Localize("Custom colors"), Row))
			{
				*pCustom ^= 1;
				Notify();
			}
			break;
		case C::BODY:
		case C::FEET:
		{
			unsigned *pColor = Control == C::BODY ? (m_Dummy ? &g_Config.m_ClDummyColorBody : &g_Config.m_ClPlayerColorBody) : (m_Dummy ? &g_Config.m_ClDummyColorFeet : &g_Config.m_ClPlayerColorFeet);
			const bool Profile = m_Dummy;
			const char *pLabel = Control == C::BODY ? Localize("Body") : Localize("Feet");
			const int Id = Control == C::BODY ? 8 : 9;
			CUIRect Swatch;
			Row.VSplitRight(std::min(24.f, Row.w * .2f), nullptr, &Swatch);
			Swatch.Margin(5, &Swatch);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Buttons[Id], pLabel, Row, false, 11, TEXTALIGN_ML, false))
				LucentSettings::OpenSkinColorPopup(Ui(), TextRender(), Swatch, pColor, false, ColorHSLA::DARKEST_LGT, [this, Profile]() {
					const bool Previous = m_Dummy;
					m_Dummy = Profile;
					SetNeedSendInfo();
					m_Dummy = Previous;
				});
			Swatch.Draw(color_cast<ColorRGBA>(ColorHSLA(*pColor).UnclampLighting(ColorHSLA::DARKEST_LGT)), IGraphics::CORNER_ALL, LucentUi::RADIUS);
			break;
		}
		case C::EMOTIONS:
			Label(Row, Localize("Emotions"), LucentStyle::Style::MutedText);
			break;
		case C::EYES:
		{
			const float Width = Row.w / NUM_EMOTES;
			static std::array<std::array<LucentStyle::Motion::CTransition, NUM_EMOTES>, 2> s_EyeSelection;
			std::array<CUIRect, NUM_EMOTES> EyeRects;

			for(int Eye = 0; Eye < NUM_EMOTES; ++Eye)
			{
				CUIRect Button;
				Row.VSplitLeft(Width, &Button, &Row);
				EyeRects[Eye] = Button;
				if(LucentUi::Button(Ui(), TextRender(), &s_Eyes[Eye], "", Button, false, true, true))
				{
					*pEyes = Eye;
					if(int(m_Dummy) == g_Config.m_ClDummy)
						GameClient()->m_Emoticon.EyeEmote(Eye);
				}
				LucentSettings::RenderSkinPreview(RenderTools(), GetInfo(), Button, Eye);
				Ui()->DoToolTip(&s_Eyes[Eye], &Button, Localize("Choose default eyes when joining a server"));
			}
			for(int Eye = 0; Eye < NUM_EMOTES; ++Eye)
				LucentUi::SelectionCorners(Ui(), EyeRects[Eye], s_EyeSelection[int(m_Dummy)][Eye], *pEyes == Eye);

			break;
		}
		case C::RANDOM:
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Buttons[0], Localize("Create a random skin"), Row, Ui()->CheckActiveItem(&s_Buttons[0]), 11, TEXTALIGN_ML, false))
			{
				GameClient()->m_Skins.RandomizeSkin(m_Dummy);
				Notify();
			}
			break;
		case C::PREFIX:
		{
			static std::array<CButtonContainer, 3> s_Prefixes;
			static std::array<std::array<LucentStyle::Motion::CTransition, 3>, 2> s_PrefixSelection;
			std::array<CUIRect, 3> PrefixRects;
			const auto &apValues = LucentSettings::SkinPrefixes;
			const char *apLabels[] = {Localize("No prefix"), "kitty", "santa"};
			const float Width = Row.w / 3;
			float Size = 11;
			for(const char *pLabel : apLabels)
				Size = std::min(Size, 11 * std::max(0.f, Width - 8) / std::max(1.f, TextRender()->TextWidth(11, pLabel)));
			for(int Index = 0; Index < 3; ++Index)
			{
				CUIRect Button;
				Row.VSplitLeft(Width, &Button, &Row);
				PrefixRects[Index] = Button;
				if(LucentUi::TextButton(Ui(), TextRender(), &s_Prefixes[Index], apLabels[Index], Button, LucentSettings::SkinPrefixIndex(g_Config.m_ClSkinPrefix) == Index, Size, TEXTALIGN_MC, false))
				{
					str_copy(g_Config.m_ClSkinPrefix, apValues[Index]);
					Refresh = true;
				}
			}
			const int Choice = LucentSettings::SkinPrefixIndex(g_Config.m_ClSkinPrefix);
			for(int Index = 0; Index < 3; ++Index)
				LucentUi::SelectionCorners(Ui(), PrefixRects[Index], s_PrefixSelection[int(m_Dummy)][Index], Choice == Index, Choice >= 0);
			// A divider beside a selected/fading segment would visually close its open edge.
			for(int Index = 0; Index < 2; ++Index)
				if(Choice != Index && Choice != Index + 1 && s_PrefixSelection[int(m_Dummy)][Index].m_Value == 0 && s_PrefixSelection[int(m_Dummy)][Index + 1].m_Value == 0)
				{
					const auto &Button = PrefixRects[Index];
					CUIRect{Button.x + Button.w, Button.y + 6, Ui()->PixelSize(), std::max(0.f, Button.h - 12)}.Draw(LucentUi::Accent(LucentStyle::Style::BorderIdle), 0, 0);
				}

			break;
		}
		case C::COOLDOWN:
			if(Client()->State() == IClient::STATE_ONLINE && GameClient()->m_aNextChangeInfo[m_Dummy] > Client()->GameTick(m_Dummy))
			{
				char aBuf[160], aTime[40];
				str_format(aTime, sizeof(aTime), Localize("%ds left"), (GameClient()->m_aNextChangeInfo[m_Dummy] - Client()->GameTick(m_Dummy) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Player info change cooldown"), aTime);
				Label(Row, aBuf, LucentStyle::Style::MutedText);
			}
			break;
		case C::DOWNLOAD:
			if(Check(&g_Config.m_ClDownloadSkins, Localize("Download skins"), Row))
			{
				g_Config.m_ClDownloadSkins ^= 1;
				Refresh = true;
			}
			break;
		case C::COMMUNITY:
			if(Check(&g_Config.m_ClDownloadCommunitySkins, Localize("Use community source"), Row))
			{
				g_Config.m_ClDownloadCommunitySkins ^= 1;
				if(!g_Config.m_ClDownloadCommunitySkins)
					str_copy(g_Config.m_ClSkinDownloadUrl, "https://skins.ddnet.org/skin/");
				Refresh = true;
			}
			break;
		case C::VANILLA:
			if(Check(&g_Config.m_ClVanillaSkinsOnly, Localize("Vanilla skins only"), Row))
			{
				g_Config.m_ClVanillaSkinsOnly ^= 1;
				Refresh = true;
			}
			break;
		case C::FAT:
			if(Check(&g_Config.m_ClFatSkins, Localize("Fat skins (DDFat)"), Row))
				g_Config.m_ClFatSkins ^= 1;
			break;
		case C::SOURCE_INFO:
		{
			const char *pSource = g_Config.m_ClDownloadCommunitySkins ? (g_Config.m_ClSkinCommunityDownloadUrl[0] ? g_Config.m_ClSkinCommunityDownloadUrl : Localize("Community source")) : Localize("Official DDNet");
			Label(Row, pSource, LucentStyle::Style::Text, true);
			static char s_SourceTooltipId;
			if(Ui()->MouseHovered(&Row))
				Ui()->SetHotItem(&s_SourceTooltipId);
			Ui()->DoToolTip(&s_SourceTooltipId, &Row, pSource);
			break;
		}
		case C::QUALITY:
		{
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClLucentSkinTextureLimit)
				Ui()->SetActiveItem(nullptr);
			Body.HSplitTop(20, &Caption, &Rail);
			Caption.VSplitRight(std::min(64.f, Caption.w * .3f), &Caption, &Value);
			Label(Caption, Localize("Maximum skin texture size"));
			char aValue[40];
			str_format(aValue, sizeof(aValue), "%d px", g_Config.m_ClLucentSkinTextureLimit);
			Label(Value, aValue);
			Rail.HSplitBottom(10, &Rail, &Body);
			const auto &Limits = LucentSkins::TextureLimits;
			const auto It = std::lower_bound(Limits.begin(), Limits.end(), g_Config.m_ClLucentSkinTextureLimit);
			const int Index = std::min(int(It - Limits.begin()), int(Limits.size()) - 1);
			const float Position = Ui()->DoScrollbarH(&g_Config.m_ClLucentSkinTextureLimit, &Rail, Index / float(Limits.size() - 1), nullptr, true, true);
			const int Next = std::clamp(int(std::round(Position * (Limits.size() - 1))), 0, int(Limits.size()) - 1);
			if(Ui()->Enabled() && Next != Index)
			{
				g_Config.m_ClLucentSkinTextureLimit = Limits[Next];
				RefreshBoth = true;
			}
			Ui()->DoToolTip(&g_Config.m_ClLucentSkinTextureLimit, &Row, Localize("Higher limits increase VRAM use. Changing the limit reloads skin textures."));
			break;
		}
		case C::LOADED_MAX:
			if(!Ui()->Enabled() && Ui()->ActiveItem() == &g_Config.m_ClSkinsLoadedMax)
				Ui()->SetActiveItem(nullptr);
			Body.HSplitTop(20, &Caption, &Rail);
			Caption.VSplitRight(std::min(64.f, Caption.w * .3f), &Caption, &Value);
			Label(Caption, Localize("Maximum loaded skins"));
			{
				int Inactive = g_Config.m_ClSkinsLoadedMax;
				Ui()->DoScrollbarOption(&g_Config.m_ClSkinsLoadedMax, Ui()->Enabled() ? &g_Config.m_ClSkinsLoadedMax : &Inactive, &Rail, Localize("Maximum loaded skins"), 256, 8192, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT, "", &Value);
			}
			break;

		case C::COMMUNITY_URL:
			if(Field(Localize("Community download URL"), s_CommunityUrl))
			{
				s_SourceDirty = true;
				GameClient()->m_Skins.RequestSettingsRefresh();
			}
			break;
		case C::RESET_SOURCE:
			if(LucentUi::Button(Ui(), TextRender(), &s_Buttons[3], Localize("Reset community URL"), Row))
			{
				str_copy(g_Config.m_ClSkinCommunityDownloadUrl, "https://skins.ddnet.org/skin/community/");
				Refresh = true;
				s_SourceDirty = false;
			}
			break;
		}
	};
	const auto Gallery = [&](CUIRect View) {
		static bool s_FavoritesOnly = false, s_LocalOnly = false, s_LastDummy = false;
		static CLineInput s_Filter(g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString));
		CUIRect Search, Filters, Footer, Database, Directory, Reload;
		View.HSplitTop(std::min(28.f, std::max(0.f, View.h)), &Search, &View);
		View.HSplitTop(std::min(32.f, std::max(0.f, View.h)), &Filters, &View);
		Footer = LucentSettings::CSkinGalleryGeometry::SplitFooter(View);
		Footer.VSplitLeft(Footer.w * .38f, &Database, &Footer);
		Footer.VSplitRight(32, &Directory, &Reload);
		if(Ui()->DoEditBox_Search(&s_Filter, &Search, 11, Ui()->Enabled() && !GameClient()->m_GameConsole.IsActive()))
			GameClient()->m_Skins.SkinList().ForceRefresh();
		CUIRect Fav, Local;
		Filters.VSplitMid(&Fav, &Local, 8);
		if(LucentUi::FilledCheckBox(Ui(), &s_FavoritesOnly, Localize("Favorites only"), s_FavoritesOnly, Fav, Ui()->Enabled(), true))
			s_FavoritesOnly = !s_FavoritesOnly;
		if(LucentUi::FilledCheckBox(Ui(), &s_LocalOnly, Localize("Local skins"), s_LocalOnly, Local, Ui()->Enabled(), true))
			s_LocalOnly = !s_LocalOnly;
		auto &List = GameClient()->m_Skins.SkinList();
		auto &Entries = List.Skins();
		std::vector<size_t> Indices;
		for(size_t i = 0; i < Entries.size(); ++i)
			if((!s_FavoritesOnly || Entries[i].IsFavorite()) && (!s_LocalOnly || Entries[i].SkinContainer()->Type() == CSkins::CSkinContainer::EType::LOCAL))
				Indices.push_back(i);
		const bool Scroll = m_SkinListScrollToSelected || s_LastDummy != m_Dummy;
		s_LastDummy = m_Dummy;
		m_SkinListScrollToSelected = false;
		const auto GetItem = [&](int Index) {
			const auto &Entry = Entries[Indices[Index]];
			return LucentSettings::CSkinGalleryItem{Entry.SkinContainer()->Name(), Entry.ListItemId(), Entry.FavoriteButtonId(), m_Dummy ? Entry.IsSelectedDummy() : Entry.IsSelectedMain(), Entry.IsFavorite()};
		};
		const auto Paint = [&](int Index, CUIRect Rect) {
			auto &Entry = Entries[Indices[Index]];
			Entry.RequestLoad();
			const auto *pContainer = Entry.SkinContainer();
			auto Info = GetInfo();
			Info.Apply(pContainer->State() == CSkins::CSkinContainer::EState::LOADED ? pContainer->Skin().get() : GameClient()->m_Skins.Find("default"));
			LucentSettings::RenderSkinPreview(RenderTools(), Info, Rect, m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes);
			if(pContainer->State() == CSkins::CSkinContainer::EState::ERROR || pContainer->State() == CSkins::CSkinContainer::EState::NOT_FOUND)
				Ui()->DoToolTip(Entry.ErrorTooltipId(), &Rect, Localize("Skin could not be loaded due to an error. Check the local console for details."));
		};
		const auto Select = [&](int Index) {
			char *pName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
			str_copy(pName, Entries[Indices[Index]].SkinContainer()->Name(), m_Dummy ? sizeof(g_Config.m_ClDummySkin) : sizeof(g_Config.m_ClPlayerSkin));
			Notify();
		};
		const auto Favorite = [&](int Index) {
			const auto &Entry = Entries[Indices[Index]];
			if(Entry.IsFavorite())
				GameClient()->m_Skins.RemoveFavorite(Entry.SkinContainer()->Name());
			else
				GameClient()->m_Skins.AddFavorite(Entry.SkinContainer()->Name());
		};
		LucentSettings::RenderSkinGallery(Ui(), TextRender(), View, Indices.size(), Scroll, GetItem, Paint, Select, Favorite, LucentSettings::SkinGalleryKeyboardFocused());

		if(LucentUi::TextButton(Ui(), TextRender(), &s_Buttons[4], Localize("Skin Database"), Database))
			Client()->ViewLink("https://ddnet.org/skins/");
		if(LucentUi::TextButton(Ui(), TextRender(), &s_Buttons[5], Localize("Skins directory"), Directory))
		{
			char aPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins", aPath, sizeof(aPath));
			Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
			Client()->ViewFile(aPath);
		}
		if(LucentUi::IconButton(Ui(), TextRender(), &s_Buttons[6], FontIcon::ARROW_ROTATE_RIGHT, Localize("Refresh"), Reload))
			Refresh = true;
		if(Indices.empty() && (g_Config.m_ClSkinFilterString[0] || s_FavoritesOnly || s_LocalOnly))
		{
			CUIRect Reset = View;
			Reset.HSplitTop(28, &Reset, nullptr);
			if(LucentUi::TextButton(Ui(), TextRender(), &s_Buttons[7], Localize("Reset filter"), Reset))
			{
				s_Filter.Clear();
				s_FavoritesOnly = s_LocalOnly = false;
				List.ForceRefresh();
			}
		}
	};
	const bool PreviousDummy = m_Dummy;
	const bool PreviousPanel = m_LucentPanelRendering;
	m_LucentPanelRendering = true;
	LucentSettings::FSkinContent Seven;
	if(Client()->IsSixup())
		Seven = [&](CUIRect View) { RenderSettingsTee7(View); };
	LucentSettings::RenderSkinManager(Ui(), TextRender(), MainView, m_Dummy, m_Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor, Control, Gallery, Seven);
	m_LucentPanelRendering = PreviousPanel;
	if(PreviousDummy != m_Dummy && s_Names[int(PreviousDummy)].WasChanged())
	{
		const bool Current = m_Dummy;
		m_Dummy = PreviousDummy;
		Notify();
		m_Dummy = Current;
	}
	if(s_SourceDirty && !s_CommunityUrl.IsActive())
	{
		Refresh = true;
		s_SourceDirty = false;
	}
	if(Ui()->Enabled() && !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && (Input()->KeyPress(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyPress(KEY_R))))
		Refresh = true;
	if(RefreshBoth)
	{
		m_pSelectedSkin = nullptr;
		m_SkinList7LastRefreshTime = std::nullopt;
		m_SkinPartsList7LastRefreshTime = std::nullopt;
		GameClient()->RefreshSkins(CSkinDescriptor::FLAG_SIX | CSkinDescriptor::FLAG_SEVEN);
	}
	else if(Refresh)
		GameClient()->RefreshSkins(CSkinDescriptor::FLAG_SIX);
}
