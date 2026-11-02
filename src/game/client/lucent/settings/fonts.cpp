#include "fonts.h"

#include "general.h"

#include <base/fs.h>
#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/shared/font_manager.h>
#include <engine/storage.h>

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
		void Text(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size = 11, bool Muted = false)
		{
			const auto Previous = pTextRender->GetTextColor();
			pTextRender->TextColor(Muted ? LucentStyle::Style::MutedText : LucentStyle::Style::Text);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EllipsisAtEnd = true;
			pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_ML, Props);
			pTextRender->TextColor(Previous);
		}
		struct CEntry
		{
			std::string m_Name;
			bool m_Directory;
		};
		struct CFontView : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			IStorage *m_pStorage = nullptr;
			CListBox m_FontList, m_FileList;
			CScrollRegion m_Scroll;
			std::vector<std::string> m_Fonts;
			std::vector<CEntry> m_Entries;
			CLineInputBuffered<IO_MAX_PATH_LENGTH> m_Path;
			std::string m_Directory, m_Error, m_Requested;
			int m_Selected = -1;
			int m_ImportButton = 0, m_RefreshButton = 0, m_UpButton = 0, m_GoButton = 0, m_OpenButton = 0, m_CancelButton = 0;

			static int FontCallback(const char *pName, int IsDir, int Type, void *pUser)
			{
				auto &View = *static_cast<CFontView *>(pUser);
				if(!IsDir && LucentFonts::ManagedFilename(pName))
					View.m_Fonts.emplace_back(pName);
				return 0;
			}
			void Refresh()
			{
				m_Fonts = {""};
				m_pStorage->ListDirectory(IStorage::TYPE_SAVE, LucentFonts::Directory, FontCallback, this);
				std::sort(m_Fonts.begin() + 1, m_Fonts.end());
				m_FontList.Reset();
			}
			static int FileCallback(const char *pName, int IsDir, int Type, void *pUser)
			{
				auto &View = *static_cast<CFontView *>(pUser);
				if(str_comp(pName, ".") != 0 && str_comp(pName, "..") != 0 && (IsDir || LucentFonts::Supported(pName)))
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
				char aFilename[256];
				if(!m_pTextRender->ImportCustomFont(pAbsolutePath, aFilename, sizeof(aFilename)))
				{
					m_Error = "Could not import font. Choose a valid TTF or OTF file up to 32 MiB.";
					return false;
				}
				Refresh();
				str_copy(g_Config.m_ClLucentFont, aFilename);
				m_Requested = aFilename;
				m_Error.clear();
				return true;
			}
		};
		CFontView &State()
		{
			static CFontView s_View;
			return s_View;
		}
		CUi::EPopupMenuFunctionResult ImportDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &State = *static_cast<CFontView *>(pContext);
			auto *pUi = State.m_pUi;
			auto *pTextRender = State.m_pTextRender;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			CUIRect Title, Path, Go, Up, Footer, Open, Cancel, Error;
			View.Margin(6, &View);
			View.HSplitTop(25, &Title, &View);
			Text(pUi, pTextRender, Title, LCNTLocalize("Import font"), 14);
			View.HSplitTop(28, &Path, &View);
			Path.VSplitRight(55, &Path, &Go);
			Path.VSplitLeft(36, &Up, &Path);
			pUi->DoEditBox(&State.m_Path, &Path, 10);
			if(LucentUi::Button(pUi, pTextRender, &State.m_UpButton, LCNTLocalize("Up"), Up))
			{
				char aParent[IO_MAX_PATH_LENGTH];
				str_copy(aParent, State.m_Directory.c_str());
				if(fs_parent_dir(aParent) == 0)
					State.Browse(aParent);
			}
			if(LucentUi::Button(pUi, pTextRender, &State.m_GoButton, LCNTLocalize("Open"), Go))
			{
				const std::string Path = State.m_Path.GetString();
				if(fs_is_dir(Path.c_str()))
					State.Browse(Path.c_str());
				else if(State.Import(Path.c_str()))
					return CUi::POPUP_CLOSE_CURRENT;
			}
			View.HSplitBottom(28, &View, &Footer);
			View.HSplitBottom(40, &View, &Error);
			Text(pUi, pTextRender, Error, LCNTLocalize(State.m_Error.empty() ? "TTF / OTF - select a file or enter an absolute path" : State.m_Error.c_str()), 10, true);
			Footer.VSplitMid(&Cancel, &Open, 6);
			const bool Cancelled = LucentUi::Button(pUi, pTextRender, &State.m_CancelButton, LCNTLocalize("Cancel"), Cancel);
			const bool Opened = LucentUi::Button(pUi, pTextRender, &State.m_OpenButton, LCNTLocalize("Open"), Open, false, State.m_Selected >= 0);
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
	}

	void LeaveFonts(CUi *pUi)
	{
		auto &View = State();
		pUi->ClosePopupMenu(&View);
		if(CLineInput::GetActiveInput() == &View.m_Path)
			View.m_Path.Deactivate();
	}

	void RenderFonts(CUi *pUi, ITextRender *pTextRender, IStorage *pStorage, CUIRect View)
	{
		auto &State = LucentSettings::State();
		State.m_pUi = pUi;
		State.m_pTextRender = pTextRender;
		State.m_pStorage = pStorage;
		if(!pUi->IsPopupOpen(&State) && CLineInput::GetActiveInput() == &State.m_Path)
			State.m_Path.Deactivate();
		if(State.m_Fonts.empty())
			State.Refresh();
		if(!State.m_Requested.empty() && str_comp(pTextRender->CustomFont(), State.m_Requested.c_str()) != 0 && !g_Config.m_ClLucentFont[0])
		{
			State.m_Error = "Font is unavailable. Using standard DDNet.";
			State.m_Requested.clear();
		}
		CUIRect Title;
		View.HSplitTop(22, &Title, &View);
		Text(pUi, pTextRender, CGeneralLayout::ContentRegion(Title), LCNTLocalize("Fonts"), 15);
		View.HSplitTop(8, nullptr, &View);
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		State.m_Scroll.Begin(&View, &Params);
		const auto Layout = CGeneralLayout::Calculate(View);
		CUIRect Left = CGeneralLayout::ContentRegion(Layout.m_Left), Row, Import, Refresh;
		Left.HSplitTop(28, &Row, &Left);
		Row.VSplitMid(&Import, &Refresh, 6);
		if(LucentUi::Button(pUi, pTextRender, &State.m_ImportButton, LCNTLocalize("Import font"), Import))
		{
			char aDirectory[IO_MAX_PATH_LENGTH];
			if(State.m_Directory.empty() && fs_getcwd(aDirectory, sizeof(aDirectory)))
				State.Browse(aDirectory);
			else
				State.Browse(State.m_Directory.c_str());
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			const float Width = std::min(500.f, pUi->Screen()->w - 16);
			const float Height = std::min(340.f, pUi->Screen()->h - 16);
			pUi->DoPopupMenu(&State, (pUi->Screen()->w - Width) / 2, (pUi->Screen()->h - Height) / 2, Width, Height, &State, ImportDialog, Props);
		}
		if(LucentUi::Button(pUi, pTextRender, &State.m_RefreshButton, LCNTLocalize("Refresh"), Refresh))
		{
			pTextRender->InitializeCustomFonts();
			State.Refresh();
		}
		State.m_Scroll.AddRect(Row);
		Left.HSplitTop(6, nullptr, &Left);
		CUIRect List;
		Left.HSplitTop(std::min(260.f, std::max(100.f, View.h - 90)), &List, &Left);
		State.m_Scroll.AddRect(List);
		int Selected = 0;
		for(int i = 0; i < int(State.m_Fonts.size()); ++i)
			if(State.m_Fonts[i] == g_Config.m_ClLucentFont)
				Selected = i;
		State.m_FontList.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.08f));
		State.m_FontList.SetScrollbarWidth(8);
		State.m_FontList.SetActive(pUi->Enabled() && !pUi->IsPopupOpen());
		State.m_FontList.DoStart(28, State.m_Fonts.size(), 1, 3, Selected, &List, false);
		for(const auto &Name : State.m_Fonts)
		{
			const auto Item = State.m_FontList.DoNextItem(&Name, Name == g_Config.m_ClLucentFont);
			if(Item.m_Visible)
				Text(pUi, pTextRender, Item.m_Rect, Name.empty() ? LCNTLocalize("Standard DDNet") : LucentFonts::DisplayName(Name.c_str()).c_str());
		}
		const int NewSelected = State.m_FontList.DoEnd();
		if(NewSelected >= 0 && NewSelected < int(State.m_Fonts.size()) && (NewSelected != Selected || State.m_FontList.WasItemSelected() || State.m_FontList.WasItemActivated()))
		{
			State.m_Requested = State.m_Fonts[NewSelected];
			str_copy(g_Config.m_ClLucentFont, State.m_Requested.c_str());
			State.m_Error.clear();
		}
		CUIRect Right = CGeneralLayout::ContentRegion(Layout.m_Right);
		if(!Layout.m_TwoColumns)
			Right.y = Left.y + CGeneralLayout::SectionGap;
		const auto PreviewRow = [&](const char *pText, float Size, float Height, bool Muted = false) {
			CUIRect Rect;
			Right.HSplitTop(Height, &Rect, &Right);
			State.m_Scroll.AddRect(Rect);
			Text(pUi, pTextRender, Rect, pText, Size, Muted);
		};
		PreviewRow(LCNTLocalize("Preview"), 12, 28);
		PreviewRow("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 14, 30);
		PreviewRow("abcdefghijklmnopqrstuvwxyz", 12, 28);
		PreviewRow("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ", 12, 30);
		PreviewRow("Съешь ещё этих мягких французских булок", 12, 30);
		PreviewRow("0123456789  !? @#% +-= /  00:12.345", 12, 30);
		PreviewRow(LCNTLocalize("Player joined the game"), 12, 28);
		PreviewRow("Lucent | DDNet | /practice | 1920x1080", 11, 28);
		PreviewRow(LCNTLocalize("Selection applies immediately. Missing glyphs use DDNet fallbacks."), 10, 36, true);
		if(!State.m_Error.empty())
			PreviewRow(LCNTLocalize(State.m_Error.c_str()), 10, 40, true);
		State.m_Scroll.End();
	}
}
