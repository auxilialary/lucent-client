#include "sound_packs.h"

#include "general.h"

#include <base/fs.h>
#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/shared/sound_pack.h>
#include <engine/storage.h>

#include <generated/client_data.h>

#include <game/client/components/sounds.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui_listbox.h>

#include <algorithm>
#include <string>
#include <vector>

namespace LucentSettings
{
	namespace
	{
		void Text(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size = 11, bool Muted = false, int Align = TEXTALIGN_ML)
		{
			const auto Previous = pTextRender->GetTextColor();
			pTextRender->TextColor(Muted ? LucentStyle::Style::MutedText : LucentStyle::Style::Text);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EllipsisAtEnd = true;
			pUi->DoLabel(&Rect, pText, Size, Align, Props);
			pTextRender->TextColor(Previous);
		}
		bool Chip(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pLabel, CUIRect Rect, bool Selected = false, int Count = -1, bool Enabled = true)
		{
			const bool Hover = Enabled && pUi->Enabled() && pUi->MouseHovered(&Rect);
			LucentUi::InputFrame(Rect, pId, Selected, pUi->CheckActiveItem(pId), Hover, Enabled, IGraphics::CORNER_ALL);
			CUIRect Inner;
			Rect.Margin(LucentStyle::Style::Hairline, &Inner);
			const float Hot = LucentUi::Highlight(pId, Hover);
			Inner.Draw(LucentUi::Accent(Selected ? LucentStyle::Style::Selected : LucentStyle::Style::Hover * Hot), IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
			CUIRect Label = Rect;
			Label.VMargin(8, &Label);
			if(CSoundPackLayout::ShowCategoryCount(Count))
			{
				char aCount[16];
				str_format(aCount, sizeof(aCount), "%d", Count);
				CUIRect Badge;
				Label.VSplitRight(std::max(18.f, pTextRender->TextWidth(9, aCount) + 10), &Label, &Badge);
				Label.w = std::max(0.f, Label.w - 6);
				Badge.HMargin(5, &Badge);
				Badge.Draw(LucentUi::Accent(Selected ? LucentStyle::Style::Selected : LucentStyle::Style::Hover), IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
				Text(pUi, pTextRender, Badge, aCount, 9, !Selected, TEXTALIGN_MC);
			}
			Text(pUi, pTextRender, Label, pLabel, 11, !Enabled || (!Selected && !Hover));
			return Enabled && pUi->Enabled() && pUi->DoButtonLogic(pId, Selected, &Rect, BUTTONFLAG_LEFT);
		}
		struct CEntry
		{
			std::string m_Name;
			bool m_Directory;
		};
		struct CSampleRow
		{
			size_t m_Index;
			const CDataSound *m_pSample;
			LucentSoundPacks::CSoundDescription m_Description;
			int m_Variant;
			int m_VariantCount;
		};
		std::vector<CSampleRow> CatalogRows()
		{
			std::vector<CSampleRow> Rows;
			for(int s = 0; s < g_pData->m_NumSounds; ++s)
				for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; ++i)
					Rows.push_back({Rows.size(), &g_pData->m_aSounds[s].m_aSounds[i], LucentSoundPacks::Describe(g_pData->m_aSounds[s].m_pName), i + 1, g_pData->m_aSounds[s].m_NumSounds});
			return Rows;
		}
		struct CSoundPackView : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			IStorage *m_pStorage = nullptr;
			CSounds *m_pSounds = nullptr;
			ISound *m_pSound = nullptr;
			CListBox m_Catalog, m_ReviewList;
			SPopupMenuId m_ComparePopup, m_SavePopup;
			CLineInputBuffered<96> m_PresetName;
			CLineInputBuffered<128> m_Search;
			std::string m_LastSearch;
			bool m_ReviewImport = false, m_OpenReview = false, m_ChangedOnly = false, m_CatalogInitialized = false;
			size_t m_CompareIndex = 0;
			int m_ReviewSelected = 0, m_Category = LucentSoundPacks::ALL;
			int m_CategoryButtons[LucentSoundPacks::CATEGORY_COUNT]{};
			int m_SaveButton = 0, m_SaveConfirm = 0, m_SaveCancel = 0, m_OriginalButton = 0, m_CustomButton = 0, m_ApplyButton = 0, m_CloseButton = 0, m_ChangedButton = 0;
			int m_SelectedSound = 0, m_StopButton = 0;
			ISound::CVoiceHandle m_Preview;
			CListBox m_PackList, m_FileList;
			CScrollRegion m_Scroll;
			std::vector<std::string> m_Packs;
			std::vector<CEntry> m_Entries;
			CLineInputBuffered<IO_MAX_PATH_LENGTH> m_Path;
			std::string m_Directory, m_Error, m_Requested;
			int m_Selected = -1;
			int m_ImportButton = 0, m_RefreshButton = 0, m_UpButton = 0, m_GoButton = 0, m_OpenButton = 0, m_CancelButton = 0;

			static int PackCallback(const char *pName, int IsDir, int Type, void *pUser)
			{
				auto &View = *static_cast<CSoundPackView *>(pUser);
				if(!IsDir && LucentSoundPacks::Managed(pName))
					View.m_Packs.emplace_back(pName);
				return 0;
			}
			void Refresh()
			{
				m_Packs = {""};
				m_pStorage->ListDirectory(IStorage::TYPE_SAVE, LucentSoundPacks::Directory, PackCallback, this);
				std::sort(m_Packs.begin() + 1, m_Packs.end());
				m_PackList.Reset();
			}
			static int FileCallback(const char *pName, int IsDir, int Type, void *pUser)
			{
				auto &View = *static_cast<CSoundPackView *>(pUser);
				if(str_comp(pName, ".") != 0 && str_comp(pName, "..") != 0 && (IsDir || LucentSoundPacks::ImportSupported(pName)))
					View.m_Entries.push_back({pName, IsDir != 0});
				return 0;
			}
			void Browse(const char *pDirectory)
			{
				char aAbsolute[IO_MAX_PATH_LENGTH];
				if(fs_is_relative_path(pDirectory))
				{
					char aCwd[IO_MAX_PATH_LENGTH];
					if(!fs_getcwd(aCwd, sizeof(aCwd)))
						return;
					str_format(aAbsolute, sizeof(aAbsolute), "%s/%s", aCwd, pDirectory);
					pDirectory = aAbsolute;
				}
				if(!fs_is_dir(pDirectory))
				{
					m_Error = "Folder is not accessible";
					return;
				}
				m_Directory = pDirectory;
				m_Path.Set(m_Directory.c_str());
				m_Entries.clear();
				fs_listdir(m_Directory.c_str(), FileCallback, IStorage::TYPE_ABSOLUTE, this);
				std::sort(m_Entries.begin(), m_Entries.end(), [](const CEntry &A, const CEntry &B) {
					return A.m_Directory != B.m_Directory ? A.m_Directory : A.m_Name < B.m_Name;
				});
				m_Selected = -1;
				m_FileList.Reset();
				m_Error.clear();
			}
			bool Import(const char *pAbsolutePath)
			{
				if(!m_pSounds->ImportSoundPack(pAbsolutePath, true))
				{
					m_Error = m_pSounds->SoundPackError();
					return false;
				}
				m_ReviewImport = true;
				m_OpenReview = true;
				m_ReviewSelected = 0;
				m_Error.clear();
				return true;
			}
		};
		CSoundPackView &State()
		{
			static CSoundPackView s_View;
			return s_View;
		}
		CUi::EPopupMenuFunctionResult ImportDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &State = *static_cast<CSoundPackView *>(pContext);
			auto *pUi = State.m_pUi;
			auto *pTextRender = State.m_pTextRender;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			CUIRect Title, Path, Go, Up, Footer, Open, Cancel, Error;
			View.Margin(6, &View);
			View.HSplitTop(25, &Title, &View);
			Text(pUi, pTextRender, Title, LCNTLocalize("Import sound pack"), 14);
			View.HSplitTop(8, nullptr, &View);
			View.HSplitTop(28, &Path, &View);
			Path.VSplitRight(64, &Path, &Go);
			Path.VSplitRight(8, &Path, nullptr);
			Path.VSplitLeft(44, &Up, &Path);
			Path.VSplitLeft(8, nullptr, &Path);
			pUi->DoEditBox(&State.m_Path, &Path, 10);
			if(Chip(pUi, pTextRender, &State.m_UpButton, LCNTLocalize("Up"), Up))
			{
				char aParent[IO_MAX_PATH_LENGTH];
				str_copy(aParent, State.m_Directory.c_str());
				if(fs_parent_dir(aParent) == 0)
					State.Browse(aParent);
			}
			if(Chip(pUi, pTextRender, &State.m_GoButton, LCNTLocalize("Open"), Go))
			{
				const std::string Path = State.m_Path.GetString();
				if(fs_is_dir(Path.c_str()))
					State.Browse(Path.c_str());
				else if(State.Import(Path.c_str()))
					return CUi::POPUP_CLOSE_CURRENT;
			}
			View.HSplitBottom(28, &View, &Footer);
			View.HSplitBottom(40, &View, &Error);
			Text(pUi, pTextRender, Error, LCNTLocalize(State.m_Error.empty() ? "Select ZIP or WV (standard DDNet filename), or enter an absolute path" : State.m_Error.c_str()), 10, true);
			Footer.VSplitMid(&Cancel, &Open, CSoundPackLayout::Gap);
			const bool Cancelled = Chip(pUi, pTextRender, &State.m_CancelButton, LCNTLocalize("Cancel"), Cancel);
			const bool Opened = Chip(pUi, pTextRender, &State.m_OpenButton, LCNTLocalize("Open"), Open, true, -1, State.m_Selected >= 0);
			State.m_FileList.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.08f));
			State.m_FileList.SetScrollbarWidth(8);
			State.m_FileList.SetActive(Active);
			State.m_FileList.DoStart(25, State.m_Entries.size(), 1, 3, State.m_Selected, &View, false);
			for(const auto &Entry : State.m_Entries)
			{
				const auto Item = State.m_FileList.DoNextItem(&Entry);
				if(Item.m_Visible)
					Text(pUi, pTextRender, Item.m_Rect, (Entry.m_Directory ? "[ " + Entry.m_Name + " ]" : Entry.m_Name).c_str());
			}
			State.m_Selected = State.m_FileList.DoEnd();
			if(Cancelled)
				return CUi::POPUP_CLOSE_CURRENT;
			if((Opened || State.m_FileList.WasItemActivated()) && State.m_Selected >= 0 && State.m_Selected < int(State.m_Entries.size()))
			{
				const auto Entry = State.m_Entries[State.m_Selected];
				const std::string Path = State.m_Directory + "/" + Entry.m_Name;
				if(Entry.m_Directory)
					State.Browse(Path.c_str());
				else if(State.Import(Path.c_str()))
					return CUi::POPUP_CLOSE_CURRENT;
			}
			return CUi::POPUP_KEEP_OPEN;
		}
		void StopPreview(CSoundPackView &State)
		{
			if(State.m_pSound)
				State.m_pSound->StopVoice(State.m_Preview);
			State.m_Preview = {};
		}
		void PlayPreview(CSoundPackView &State, bool Original)
		{
			StopPreview(State);
			const int Id = State.m_pSounds->SoundPackSample(State.m_CompareIndex, Original, State.m_ReviewImport);
			if(Id >= 0)
				State.m_Preview = State.m_pSound->Play(CSounds::CHN_GUI, Id, ISound::FLAG_PREVIEW, 1.0f);
		}
		void DiscardReview(CSoundPackView &State)
		{
			StopPreview(State);
			State.m_pSounds->SetSoundPackComparing(false);
			State.m_pSounds->CancelSoundImport();
			State.m_ReviewImport = false;
			State.m_OpenReview = false;
		}
		CUi::EPopupMenuFunctionResult CompareDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &State = *static_cast<CSoundPackView *>(pContext);
			auto *pUi = State.m_pUi;
			auto *pTextRender = State.m_pTextRender;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			View.Margin(12, &View);
			CUIRect Row, Original, Custom, Stop, Close, Apply, Footer;
			View.HSplitTop(26, &Row, &View);
			Text(pUi, pTextRender, Row, LCNTLocalize(State.m_ReviewImport ? "Review imported sounds" : "Compare sounds"), 15);
			View.HSplitTop(8, nullptr, &View);
			std::vector<CSampleRow> Rows;
			for(const auto &Entry : CatalogRows())
				if(!State.m_ReviewImport || State.m_pSounds->SoundReplaced(Entry.m_Index, true))
					Rows.push_back(Entry);
			if(State.m_ReviewImport)
			{
				View.HSplitTop(std::min(CSoundPackLayout::ReviewHeight(Rows.size()), std::max(32.f, View.h - 170)), &Row, &View);
				State.m_ReviewList.SetActive(Active);
				State.m_ReviewList.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.08f));
				State.m_ReviewList.DoStart(32, Rows.size(), 1, 3, State.m_ReviewSelected, &Row, false);
				for(const auto &Entry : Rows)
				{
					const auto Item = State.m_ReviewList.DoNextItem(Entry.m_pSample);
					if(Item.m_Visible)
					{
						CUIRect Name = Item.m_Rect;
						Name.VMargin(6, &Name);
						CUIRect Filename;
						Name.HSplitTop(18, &Name, &Filename);
						std::string Label = LCNTLocalize(Entry.m_Description.m_pLabel);
						if(Entry.m_VariantCount > 1)
							Label += " " + std::to_string(Entry.m_Variant);
						Text(pUi, pTextRender, Name, Label.c_str(), 10);
						Text(pUi, pTextRender, Filename, fs_filename(Entry.m_pSample->m_pFilename), 8, true);
					}
				}
				State.m_ReviewSelected = State.m_ReviewList.DoEnd();
				View.HSplitTop(12, nullptr, &View);
				if(State.m_ReviewSelected >= 0 && State.m_ReviewSelected < int(Rows.size()))
				{
					const auto Index = Rows[State.m_ReviewSelected].m_Index;
					if(Index != State.m_CompareIndex)
						StopPreview(State);
					State.m_CompareIndex = Index;
				}
			}
			const auto Full = CatalogRows();
			if(State.m_CompareIndex >= Full.size())
				return CUi::POPUP_CLOSE_CURRENT;
			const auto &Entry = Full[State.m_CompareIndex];
			View.HSplitTop(22, &Row, &View);
			Text(pUi, pTextRender, Row, LCNTLocalize(Entry.m_Description.m_pLabel), 12);
			View.HSplitTop(18, &Row, &View);
			Text(pUi, pTextRender, Row, fs_filename(Entry.m_pSample->m_pFilename), 10, true);
			View.HSplitTop(10, nullptr, &View);
			View.HSplitTop(32, &Row, &View);
			Row.VSplitMid(&Original, &Custom, CSoundPackLayout::Gap);
			if(Chip(pUi, pTextRender, &State.m_OriginalButton, LCNTLocalize("Play original"), Original))
				PlayPreview(State, true);
			const bool Replaced = State.m_pSounds->SoundReplaced(State.m_CompareIndex, State.m_ReviewImport);
			if(Chip(pUi, pTextRender, &State.m_CustomButton, LCNTLocalize("Play replacement"), Custom, false, -1, Replaced))
				PlayPreview(State, false);
			View.HSplitTop(10, nullptr, &View);
			View.HSplitTop(28, &Row, &View);
			const char *pError = State.m_pSounds->SoundPackError();
			Text(pUi, pTextRender, Row, LCNTLocalize(*pError ? pError : Replaced ? "Custom sound replaces this DDNet file" :
											       "Unchanged: using the original DDNet sound"),
				10, true);
			View.HSplitBottom(30, &View, &Footer);
			Footer.VSplitLeft(64, &Stop, &Footer);
			Footer.VSplitLeft(CSoundPackLayout::Gap, nullptr, &Footer);
			if(Chip(pUi, pTextRender, &State.m_StopButton, LCNTLocalize("Stop"), Stop))
				StopPreview(State);
			if(State.m_ReviewImport)
				Footer.VSplitMid(&Close, &Apply, CSoundPackLayout::Gap);
			else
				Footer.VSplitRight(std::min(110.f, Footer.w), nullptr, &Close);
			if(Chip(pUi, pTextRender, &State.m_CloseButton, LCNTLocalize(State.m_ReviewImport ? "Cancel import" : "Close"), Close))
			{
				DiscardReview(State);
				return CUi::POPUP_CLOSE_CURRENT;
			}
			if(State.m_ReviewImport && Chip(pUi, pTextRender, &State.m_ApplyButton, LCNTLocalize("Apply sounds"), Apply, true))
			{
				StopPreview(State);
				if(State.m_pSounds->CommitSoundImport())
				{
					State.m_ReviewImport = false;
					State.m_ChangedOnly = true;
					State.m_Category = LucentSoundPacks::ALL;
					State.m_SelectedSound = -1;
					State.m_Catalog.Reset();
					State.m_pSounds->SetSoundPackComparing(false);
					State.Refresh();
					return CUi::POPUP_CLOSE_CURRENT;
				}
			}
			return CUi::POPUP_KEEP_OPEN;
		}
		CUi::EPopupMenuFunctionResult SaveDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &State = *static_cast<CSoundPackView *>(pContext);
			auto *pUi = State.m_pUi;
			auto *pTextRender = State.m_pTextRender;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			View.Margin(10, &View);
			CUIRect Row, Save, Cancel;
			View.HSplitTop(28, &Row, &View);
			Text(pUi, pTextRender, Row, LCNTLocalize("Save sound preset"), 14);
			View.HSplitTop(18, &Row, &View);
			Text(pUi, pTextRender, Row, LCNTLocalize("Preset name"), 10, true);
			View.HSplitTop(30, &Row, &View);
			pUi->DoEditBox(&State.m_PresetName, &Row, 12);
			View.HSplitTop(30, &Row, &View);
			Text(pUi, pTextRender, Row, LCNTLocalize("Stores all current replacements as a separate preset"), 10, true);
			View.HSplitBottom(28, &View, &Row);
			Row.VSplitMid(&Cancel, &Save, 8);
			if(Chip(pUi, pTextRender, &State.m_SaveCancel, LCNTLocalize("Cancel"), Cancel))
				return CUi::POPUP_CLOSE_CURRENT;
			if(Chip(pUi, pTextRender, &State.m_SaveConfirm, LCNTLocalize("Save preset"), Save, true, -1, State.m_PresetName.GetString()[0] != 0))
			{
				if(State.m_pSounds->SaveSoundPreset(State.m_PresetName.GetString()))
				{
					State.Refresh();
					return CUi::POPUP_CLOSE_CURRENT;
				}
			}
			Text(pUi, pTextRender, View, State.m_pSounds->SoundPackError(), 10, true);
			return CUi::POPUP_KEEP_OPEN;
		}
		void OpenCentered(CSoundPackView &State, SPopupMenuId *pId, float Width, float Height, CUi::FPopupMenuFunction pfnFunction)
		{
			auto *pUi = State.m_pUi;
			if(pId == &State.m_ComparePopup)
				State.m_pSounds->SetSoundPackComparing(true);
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			Width = std::min(Width, pUi->Screen()->w - 16);
			Height = std::min(Height, pUi->Screen()->h - 16);
			pUi->DoPopupMenu(pId, (pUi->Screen()->w - Width) / 2, (pUi->Screen()->h - Height) / 2, Width, Height, &State, pfnFunction, Props);
		}

	}

	void LeaveSoundPacks(CUi *pUi)
	{
		auto &View = State();
		pUi->ClosePopupMenu(&View);
		pUi->ClosePopupMenu(&View.m_ComparePopup);
		pUi->ClosePopupMenu(&View.m_SavePopup);
		if(View.m_pSounds)
			DiscardReview(View);
		View.m_PresetName.Deactivate();
		View.m_Search.Deactivate();
		if(View.m_pSound)
			View.m_pSound->StopVoice(View.m_Preview);
		if(CLineInput::GetActiveInput() == &View.m_Path)
			View.m_Path.Deactivate();
	}

	void RenderSoundPacks(CUi *pUi, ITextRender *pTextRender, IStorage *pStorage, ISound *pSound, CSounds *pSounds, CUIRect View)
	{
		auto &State = LucentSettings::State();
		State.m_pUi = pUi;
		State.m_pTextRender = pTextRender;
		State.m_pStorage = pStorage;
		State.m_pSounds = pSounds;
		State.m_pSound = pSound;
		if(!State.m_CatalogInitialized && pSounds->SoundPacksReady())
		{
			State.m_ChangedOnly = pSounds->SoundReplacementCount() > 0;
			State.m_CatalogInitialized = true;
		}
		pSounds->SetSoundPackComparing(pUi->IsPopupOpen(&State.m_ComparePopup));
		if(!pUi->IsPopupOpen(&State.m_ComparePopup))
			StopPreview(State);
		if(!pUi->IsPopupOpen(&State) && CLineInput::GetActiveInput() == &State.m_Path)
			State.m_Path.Deactivate();
		if(!pUi->IsPopupOpen(&State.m_SavePopup) && CLineInput::GetActiveInput() == &State.m_PresetName)
			State.m_PresetName.Deactivate();
		if(State.m_ReviewImport && !State.m_OpenReview && !pUi->IsPopupOpen(&State.m_ComparePopup))
			DiscardReview(State);
		if(State.m_OpenReview && !pUi->IsPopupOpen())
		{
			State.m_OpenReview = false;
			State.m_ReviewList.Reset();
			OpenCentered(State, &State.m_ComparePopup, 460, CSoundPackLayout::CompareHeight(pSounds->SoundReplacementCount(true)), CompareDialog);
		}
		if(State.m_Packs.empty())
			State.Refresh();
		CUIRect Title;
		View.HSplitTop(22, &Title, &View);
		Text(pUi, pTextRender, CGeneralLayout::ContentRegion(Title), LCNTLocalize("Sounds"), 15);
		View.HSplitTop(8, nullptr, &View);
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		State.m_Scroll.Begin(&View, &Params);
		const auto Layout = CSoundPackLayout::Calculate(View);
		CUIRect Left = CGeneralLayout::ContentRegion(Layout.m_Left), Row, Import, Refresh;
		Left.HSplitTop(28, &Row, &Left);
		Text(pUi, pTextRender, Row, LCNTLocalize("Packs and presets"), 12);
		State.m_Scroll.AddRect(Row);
		Left.HSplitTop(30, &Row, &Left);
		Row.VSplitRight(30, &Import, &Refresh);
		Import.w = std::max(0.f, Import.w - 6);
		if(Chip(pUi, pTextRender, &State.m_ImportButton, LCNTLocalize("Import sound pack"), Import, true))
		{
			char aDirectory[IO_MAX_PATH_LENGTH];
			if(State.m_Directory.empty() && fs_getcwd(aDirectory, sizeof(aDirectory)))
				State.Browse(aDirectory);
			else
				State.Browse(State.m_Directory.c_str());
			OpenCentered(State, &State, 500, 340, ImportDialog);
		}
		if(LucentUi::IconButton(pUi, pTextRender, &State.m_RefreshButton, FontIcon::ARROWS_ROTATE, LCNTLocalize("Refresh"), Refresh))
		{
			State.Refresh();
		}
		State.m_Scroll.AddRect(Row);
		Left.HSplitTop(CSoundPackLayout::Gap, nullptr, &Left);
		Left.HSplitTop(30, &Row, &Left);
		State.m_Scroll.AddRect(Row);
		if(Chip(pUi, pTextRender, &State.m_SaveButton, LCNTLocalize("Save as preset"), Row, false, -1, pSounds->SoundReplacementCount() > 0))
		{
			State.m_PresetName.Set("");
			OpenCentered(State, &State.m_SavePopup, 420, 180, SaveDialog);
		}
		Left.HSplitTop(CSoundPackLayout::Gap, nullptr, &Left);
		Left.HSplitTop(24, &Row, &Left);
		State.m_Scroll.AddRect(Row);
		char aSummary[128];
		const auto FullCatalog = CatalogRows();
		str_format(aSummary, sizeof(aSummary), LCNTLocalize("Replaced %d of %d sounds"), int(pSounds->SoundReplacementCount()), int(FullCatalog.size()));
		Text(pUi, pTextRender, Row, aSummary, 11, true);
		Left.HSplitTop(CSoundPackLayout::Gap, nullptr, &Left);
		CUIRect List;
		Left.HSplitTop(std::min(260.f, std::max(100.f, View.h - 90)), &List, &Left);
		State.m_Scroll.AddRect(List);
		int Selected = 0;
		for(int i = 0; i < int(State.m_Packs.size()); ++i)
			if(State.m_Packs[i] == g_Config.m_ClLucentSoundPack)
				Selected = i;
		State.m_PackList.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.08f));
		State.m_PackList.SetScrollbarWidth(8);
		State.m_PackList.SetActive(pUi->Enabled() && !pUi->IsPopupOpen());
		State.m_PackList.DoStart(34, State.m_Packs.size(), 1, 3, Selected, &List, false);
		for(const auto &Name : State.m_Packs)
		{
			const auto Item = State.m_PackList.DoNextItem(&Name, Name == g_Config.m_ClLucentSoundPack);
			if(Item.m_Visible)
			{
				const bool Active = Name == g_Config.m_ClLucentSoundPack;
				CUIRect Label = Item.m_Rect, Check;
				Label.VSplitLeft(10, nullptr, &Label);
				Label.VSplitRight(24, &Label, &Check);
				if(Active)
				{
					CUIRect Stripe = {Item.m_Rect.x + 2, Item.m_Rect.y + 7, 2, Item.m_Rect.h - 14};
					Stripe.Draw(LucentUi::Accent(1), IGraphics::CORNER_ALL, 1);
					pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
					Text(pUi, pTextRender, Check, FontIcon::CHECK, 11);
					pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
				Text(pUi, pTextRender, Label, Name.empty() ? LCNTLocalize("Default") : LucentSoundPacks::DisplayName(Name).c_str(), 11, !Active);
			}
		}
		const int NewSelected = State.m_PackList.DoEnd();
		if(NewSelected >= 0 && NewSelected < int(State.m_Packs.size()) && (NewSelected != Selected || State.m_PackList.WasItemSelected() || State.m_PackList.WasItemActivated()))
		{
			State.m_Requested = State.m_Packs[NewSelected];
			if(pSounds->SetSoundPack(State.m_Requested.c_str()))
			{
				State.m_ChangedOnly = pSounds->SoundReplacementCount() > 0;
				State.m_Category = LucentSoundPacks::ALL;
				State.m_SelectedSound = -1;
				State.m_Catalog.Reset();
			}
			State.m_Error.clear();
		}
		CUIRect Right = CGeneralLayout::ContentRegion(Layout.m_Right);
		if(!Layout.m_TwoColumns)
			Right.y = Left.y + CGeneralLayout::SectionGap;
		CUIRect Caption, Catalog, Filter;
		Right.HSplitTop(28, &Caption, &Right);
		Text(pUi, pTextRender, Caption, LCNTLocalize("Game sounds"), 12);
		State.m_Scroll.AddRect(Caption);
		const int Columns = CSoundPackLayout::CategoryColumns(Right.w);
		for(int Start = 0; Start < LucentSoundPacks::CATEGORY_COUNT; Start += Columns)
		{
			Right.HSplitTop(36, &Row, &Right);
			State.m_Scroll.AddRect(Row);
			for(int Col = 0; Col < Columns && Start + Col < LucentSoundPacks::CATEGORY_COUNT; ++Col)
			{
				const int Category = Start + Col;
				int Changed = 0;
				for(const auto &Entry : FullCatalog)
					if(Category == LucentSoundPacks::ALL || Entry.m_Description.m_Category == Category)
					{
						Changed += pSounds->SoundReplaced(Entry.m_Index);
					}
				CUIRect Button = CSoundPackLayout::CategoryButton(Row, Col, Columns);
				if(Chip(pUi, pTextRender, &State.m_CategoryButtons[Category], LCNTLocalize(LucentSoundPacks::CategoryNames[Category]), Button, State.m_Category == Category, Changed))
				{
					State.m_Category = Category;
					State.m_SelectedSound = -1;
					State.m_Catalog.Reset();
				}
			}
		}
		Right.HSplitTop(6, nullptr, &Right);
		Right.HSplitTop(30, &Filter, &Right);
		State.m_Scroll.AddRect(Filter);
		CUIRect Search, Toggle;
		const bool Compact = Filter.w < 360;
		Filter.VSplitRight(Compact ? Filter.w : 175, &Search, &Toggle);
		if(Compact)
		{
			Right.HSplitTop(CSoundPackLayout::Gap, nullptr, &Right);
			Right.HSplitTop(30, &Search, &Right);
			State.m_Scroll.AddRect(Search);
		}
		else
			Search.w = std::max(0.f, Search.w - 10);
		pUi->DoEditBox(&State.m_Search, &Search, 10);
		if(State.m_Search.GetString()[0] == 0 && CLineInput::GetActiveInput() != &State.m_Search)
		{
			CUIRect Hint = Search;
			Hint.VMargin(6, &Hint);
			Text(pUi, pTextRender, Hint, LCNTLocalize("Search sounds"), 10, true);
		}
		if(State.m_LastSearch != State.m_Search.GetString())
		{
			State.m_LastSearch = State.m_Search.GetString();
			State.m_SelectedSound = -1;
			State.m_Catalog.Reset();
		}
		if(LucentUi::FilledCheckBox(pUi, &State.m_ChangedButton, LCNTLocalize("Replacements only"), State.m_ChangedOnly, Toggle))
		{
			State.m_ChangedOnly = !State.m_ChangedOnly;
			State.m_SelectedSound = -1;
			State.m_Catalog.Reset();
		}
		Right.HSplitTop(12, nullptr, &Right);
		Right.HSplitTop(220, &Catalog, &Right);
		State.m_Scroll.AddRect(Catalog);
		State.m_Catalog.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.08f));
		State.m_Catalog.SetScrollbarWidth(8);
		State.m_Catalog.SetActive(pUi->Enabled() && !pUi->IsPopupOpen());
		std::vector<CSampleRow> Samples;
		for(const auto &Entry : FullCatalog)
			if((State.m_Category == LucentSoundPacks::ALL || Entry.m_Description.m_Category == State.m_Category) && (!State.m_ChangedOnly || pSounds->SoundReplaced(Entry.m_Index)) &&
				(State.m_Search.GetString()[0] == 0 || str_utf8_find_nocase(LCNTLocalize(Entry.m_Description.m_pLabel), State.m_Search.GetString()) || str_utf8_find_nocase(Entry.m_pSample->m_pFilename, State.m_Search.GetString())))
				Samples.push_back(Entry);
		State.m_Catalog.DoStart(CSoundPackLayout::RowHeight, Samples.size(), 1, 3, State.m_SelectedSound, &Catalog, false);
		for(const auto &Entry : Samples)
		{
			const auto Item = State.m_Catalog.DoNextItem(Entry.m_pSample);
			if(Item.m_Visible)
			{
				CUIRect Label, Filename, Badge, Compare, Actions;
				CUIRect Content = Item.m_Rect;
				Content.VMargin(8, &Content);
				const bool Changed = pSounds->SoundReplaced(Entry.m_Index);
				const char *pStatus = LCNTLocalize(Changed ? "Replaced" : "Default");
				const float BadgeWidth = pTextRender->TextWidth(9, pStatus) + 12;
				const float CompareWidth = pTextRender->TextWidth(10, LCNTLocalize("Compare")) + 16;
				Content.VSplitRight(std::min(BadgeWidth + CompareWidth + 16, Content.w * .55f), &Content, &Actions);
				Actions.HMargin((Actions.h - 22) / 2, &Actions);
				Actions.VSplitRight(CompareWidth, &Badge, &Compare);
				Badge.VSplitRight(10, &Badge, nullptr);
				Badge.Draw(LucentUi::Accent(Changed ? LucentStyle::Style::Selected : LucentStyle::Style::Hover), IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
				Text(pUi, pTextRender, Badge, pStatus, 9, !Changed, TEXTALIGN_MC);
				LucentUi::Surface(Compare, false, pUi->Enabled() && pUi->MouseHovered(&Item.m_Rect));
				Text(pUi, pTextRender, Compare, LCNTLocalize("Compare"), 10, false, TEXTALIGN_MC);
				Content.w = std::max(0.f, Content.w - 10);
				Content.HMargin(3, &Content);
				Content.HSplitTop(20, &Label, &Filename);
				std::string LabelText = LCNTLocalize(Entry.m_Description.m_pLabel);
				if(Entry.m_VariantCount > 1)
					LabelText += " " + std::to_string(Entry.m_Variant);
				Text(pUi, pTextRender, Label, LabelText.c_str(), 11);
				Text(pUi, pTextRender, Filename, fs_filename(Entry.m_pSample->m_pFilename), 9, true);
			}
		}
		State.m_SelectedSound = State.m_Catalog.DoEnd();
		if((State.m_Catalog.WasItemSelected() || State.m_Catalog.WasItemActivated()) && State.m_SelectedSound >= 0 && State.m_SelectedSound < int(Samples.size()) && pSounds->SoundPacksReady())
		{
			StopPreview(State);
			State.m_CompareIndex = Samples[State.m_SelectedSound].m_Index;
			OpenCentered(State, &State.m_ComparePopup, 460, CSoundPackLayout::CompareHeight(0), CompareDialog);
		}
		if(Samples.empty())
			Text(pUi, pTextRender, Catalog, LCNTLocalize("No sounds match your filters"), 11, true);
		CUIRect Error;
		Right.HSplitTop(44, &Error, &Right);
		State.m_Scroll.AddRect(Error);
		Text(pUi, pTextRender, Error, LCNTLocalize(State.m_Error.empty() ? pSounds->SoundPackError() : State.m_Error.c_str()), 10, true);
		State.m_Scroll.End();
	}
}
