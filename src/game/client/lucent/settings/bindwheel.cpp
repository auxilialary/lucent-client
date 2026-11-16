#include "bindwheel.h"

#include <engine/font_icons.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/ui_listbox.h>

namespace LucentSettings
{
	namespace
	{
		void Text(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size = 11.f, bool Muted = false)
		{
			const auto Previous = pTextRender->GetTextColor();
			pTextRender->TextColor(Muted ? LucentStyle::Style::MutedText : LucentStyle::Style::Text);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EllipsisAtEnd = true;
			pUi->DoLabel(&Rect, pText, Size, TEXTALIGN_ML, Props);
			pTextRender->TextColor(Previous);
		}
		void Marquee(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size, bool Muted)
		{
			const float Width = pTextRender->TextWidth(Size, pText);
			if(Width <= Rect.w || !LucentStyle::Motion::Clock().m_Animate)
			{
				Text(pUi, pTextRender, Rect, pText, Size, Muted);
				return;
			}
			pUi->ClipEnable(&Rect);
			const float Travel = Width + 28;
			const float Offset = std::fmod(std::max(0.0, LucentStyle::Motion::Clock().m_Time - 1.0) * 24.0, double(Travel));
			CUIRect Label = Rect;
			Label.x -= Offset;
			Label.w = Width + 2;
			Text(pUi, pTextRender, Label, pText, Size, Muted);
			Label.x += Travel;
			Text(pUi, pTextRender, Label, pText, Size, Muted);
			pUi->ClipDisable();
		}
		struct CView : SPopupMenuId
		{
			CGameClient *m_pGame = nullptr;
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			CListBox m_List, m_SourceList, m_ImportList;
			int m_Page = 0, m_Selected = -1, m_Source = -1;
			std::array<int, LucentBindWheel::MaxBinds> m_Items{};
			std::array<int, 32> m_Buttons{};
			std::array<int, LucentBindWheel::MaxBinds> m_DragIds{};
			SPopupMenuId m_ActionPopup;
			int m_ActionPage = 0, m_ActionIndex = -1;
			LucentBindWheel::CBind m_ActionBind;
			int m_DragSource = -1, m_DragPage = 0;
			int m_RingSource = -1, m_RingPage = 0;
			bool m_RingDragging = false;
			vec2 m_RingOrigin = vec2(0, 0);
			int m_RingDragId = 0;
			bool m_Dragging = false;
			vec2 m_DragOrigin = vec2(0, 0);
			LucentBindWheel::CFlipPresentation m_PreviewFlip;
			CButtonContainer m_Key, m_ClearKey;
			CLineInputBuffered<64> m_Name;
			CLineInputBuffered<1024> m_Command;
			CLineInputBuffered<IO_MAX_PATH_LENGTH> m_Path;
			LucentBindWheel::CSource m_Import;
			bool m_ImportSettings = false, m_ImportKeys = false, m_Reviewed = false;
			bool m_ManualImport = false, m_ImportOptions = false, m_ImportDone = false;
			int m_ImportPage = 0;
			std::string m_ImportError;
			void ClosePathInput()
			{
				m_Path.Deactivate();
				if(m_pUi && m_pUi->ActiveItem() == &m_Path)
					m_pUi->SetActiveItem(nullptr);
			}
			void ChooseSource(const LucentBindWheel::CSource &Source, int Index)
			{
				ClosePathInput();
				m_Source = Index;
				m_Import = Source;
				m_Reviewed = !Source.m_Data.m_Binds.empty();
				m_Path.Set(Source.m_Path.c_str());
				m_ImportList.Reset();
				m_ImportError.clear();
				m_ImportDone = false;
			}
			void FindSources(CLucentBindWheel &Wheel)
			{
				ClosePathInput();
				Wheel.RefreshSources();
				m_SourceList.Reset();
				m_ImportList.Reset();
				m_Source = LucentBindWheel::SuggestedSource(Wheel.Sources());
				m_Reviewed = false;
				m_ImportError.clear();
				m_ImportDone = false;
				if(m_Source >= 0)
					ChooseSource(Wheel.Sources()[m_Source], m_Source);
			}

			std::string m_Error;
			void Select(const CLucentBindWheel &Wheel, int Index)
			{
				m_Selected = Index;
				const auto &Page = Wheel.Page(m_Page);
				if(Index >= 0 && Index < int(Page.size()))
				{
					m_Name.Set(Page[Index].m_Name.c_str());
					m_Command.Set(Page[Index].m_Command.c_str());
				}
				else
				{
					m_Selected = -1;
					m_Name.Set("");
					m_Command.Set("");
				}
			}
		};
		CView &State()
		{
			static CView View;
			return View;
		}
		CUi::EPopupMenuFunctionResult ActionDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &S = *static_cast<CView *>(pContext);
			auto &Wheel = S.m_pGame->m_BindWheel;
			const auto &Page = Wheel.Page(S.m_ActionPage);
			if(S.m_ActionIndex < 0 || S.m_ActionIndex >= int(Page.size()) || !(Page[S.m_ActionIndex] == S.m_ActionBind))
				return CUi::POPUP_CLOSE_CURRENT;
			LucentUi::CScope Style(S.m_pUi);
			LucentUi::CInteractionScope Input(S.m_pUi, Active);
			View.Margin(8, &View);
			CUIRect Row;
			View.HSplitTop(26, &Row, &View);
			if(LucentUi::Button(S.m_pUi, S.m_pTextRender, &S.m_Buttons[20], LCNTLocalize("Duplicate action"), Row, false, Page.size() < LucentBindWheel::MaxBinds))
			{
				if(Wheel.Set(S.m_ActionPage, int(Page.size()), S.m_ActionBind))
				{
					S.m_Page = S.m_ActionPage;
					S.Select(Wheel, int(Page.size()) - 1);
				}
				return CUi::POPUP_CLOSE_CURRENT;
			}
			View.HSplitTop(26, &Row, &View);
			const int Target = 1 - S.m_ActionPage;
			if(LucentUi::Button(S.m_pUi, S.m_pTextRender, &S.m_Buttons[21], LCNTLocalize("Move to other page"), Row, false, Wheel.Page(Target).size() < LucentBindWheel::MaxBinds))
			{
				if(Wheel.Transfer(S.m_ActionPage, S.m_ActionIndex, Target, int(Wheel.Page(Target).size())))
				{
					S.m_Page = Target;
					S.Select(Wheel, int(Wheel.Page(Target).size()) - 1);
				}
				return CUi::POPUP_CLOSE_CURRENT;
			}
			View.HSplitTop(26, &Row, &View);
			if(LucentUi::Button(S.m_pUi, S.m_pTextRender, &S.m_Buttons[22], LCNTLocalize("Remove action"), Row))
			{
				Wheel.Remove(S.m_ActionPage, S.m_ActionIndex);
				S.Select(Wheel, -1);
				return CUi::POPUP_CLOSE_CURRENT;
			}
			return CUi::POPUP_KEEP_OPEN;
		}
		CUi::EPopupMenuFunctionResult ImportDialog(void *pContext, CUIRect View, bool Active)
		{
			auto &S = *static_cast<CView *>(pContext);
			auto *pUi = S.m_pUi;
			auto *pTextRender = S.m_pTextRender;
			auto &Wheel = S.m_pGame->m_BindWheel;
			LucentUi::CScope Style(pUi);
			LucentUi::CInteractionScope Input(pUi, Active);
			View.Margin(14, &View);
			const auto L = CBindWheelImportLayout::Calculate(View, S.m_ImportOptions);
			CUIRect Header = L.m_Header, Subtitle;
			Header.HSplitTop(24, &Header, &Subtitle);
			Text(pUi, pTextRender, Header, LCNTLocalize("Import wheel"), 16);
			if(S.m_ImportDone)
			{
				Text(pUi, pTextRender, Subtitle, LCNTLocalize("Your wheel is ready to use"), 10, true);
				CUIRect Message{View.x + 16, View.y + View.h * .3f, View.w - 32, 36};
				char aMessage[256];
				str_format(aMessage, sizeof(aMessage), LCNTLocalize("Imported %d actions"), int(S.m_Import.m_Data.m_Binds.size()));
				Text(pUi, pTextRender, Message, aMessage, 18);
				Message.y += 40;
				Text(pUi, pTextRender, Message, LCNTLocalize(S.m_ImportPage ? "Page 2 · Shift" : "Page 1"), 12, true);
				if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[13], LCNTLocalize("Done"), L.m_Footer, true))
					return CUi::POPUP_CLOSE_CURRENT;
				return CUi::POPUP_KEEP_OPEN;
			}
			Text(pUi, pTextRender, Subtitle, LCNTLocalize(Wheel.Sources().empty() ? "We look for saved wheels on this computer." : "Choose a wheel. We found your saved settings automatically."), 10, true);
			CUIRect Target = L.m_Target, TargetLabel, Page1, Page2;
			Target.VSplitLeft(std::min(120.f, Target.w * .3f), &TargetLabel, &Target);
			Target.w = std::min(Target.w, 290.f);
			Target.VSplitMid(&Page1, &Page2, 6);
			Text(pUi, pTextRender, TargetLabel, LCNTLocalize("Import into"), 10, true);
			if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[23], LCNTLocalize("Page 1"), Page1, S.m_ImportPage == 0))
				S.m_ImportPage = 0;
			if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[24], LCNTLocalize("Page 2 · Shift"), Page2, S.m_ImportPage == 1))
				S.m_ImportPage = 1;
			LucentUi::Surface(L.m_Sources);
			LucentUi::Surface(L.m_Review);
			CUIRect Sources = L.m_Sources;
			Sources.Margin(8, &Sources);
			CUIRect SourceHeading, Refresh;
			Sources.HSplitTop(26, &SourceHeading, &Sources);
			SourceHeading.VSplitRight(24, &SourceHeading, &Refresh);
			Text(pUi, pTextRender, SourceHeading, LCNTLocalize("Found automatically"), 10, true);
			if(LucentUi::IconButton(pUi, pTextRender, &S.m_Buttons[8], FontIcon::ARROWS_ROTATE, LCNTLocalize("Refresh"), Refresh))
			{
				S.FindSources(Wheel);
				S.m_ManualImport = false;
			}
			const auto &Found = Wheel.Sources();
			S.m_SourceList.SetActive(Active);
			S.m_SourceList.SetColors(LucentUi::Accent(.15f), LucentUi::Accent(.055f));
			S.m_SourceList.DoStart(44, int(Found.size()), 1, 3, S.m_Source, &Sources, false);
			for(int i = 0; i < int(Found.size()); ++i)
			{
				const auto Item = S.m_SourceList.DoNextItem(&Found[i], S.m_Source == i);
				if(!Item.m_Visible)
					continue;
				CUIRect Name = Item.m_Rect, Details;
				Name.VMargin(6, &Name);
				Name.HSplitTop(23, &Name, &Details);
				Text(pUi, pTextRender, Name, LCNTLocalize(LucentBindWheel::SourceClient(Found[i])), 12);
				char aDetails[160];
				str_format(aDetails, sizeof(aDetails), LCNTLocalize("%s · %d actions"), LCNTLocalize(LucentBindWheel::SourceLocation(Found[i])), int(Found[i].m_Data.m_Binds.size()));
				Text(pUi, pTextRender, Details, aDetails, 9, true);
			}
			const int SourceIndex = S.m_SourceList.DoEnd();
			if(SourceIndex >= 0 && SourceIndex < int(Found.size()) && (SourceIndex != S.m_Source || S.m_SourceList.WasItemSelected()))
			{
				S.ChooseSource(Found[SourceIndex], SourceIndex);
				S.m_ManualImport = false;
			}
			if(Found.empty())
				Text(pUi, pTextRender, Sources, LCNTLocalize("No saved wheels found"), 10, true);
			CUIRect Review = L.m_Review;
			Review.Margin(10, &Review);
			CUIRect Row;
			if(S.m_ManualImport)
			{
				const float Density = std::min(1.f, Review.h / 130.f);
				Review.HSplitTop(28 * Density, &Row, &Review);
				Text(pUi, pTextRender, Row, LCNTLocalize("Choose a file manually"), 12);
				Review.HSplitTop(34 * Density, &Row, &Review);
				Text(pUi, pTextRender, Row, LCNTLocalize("Use this only if your client was not found."), 10, true);
				Review.HSplitTop(26 * Density, &Row, &Review);
				S.m_Path.SetEmptyText(LCNTLocalize("Path to wheel settings file"));
				pUi->DoEditBox(&S.m_Path, &Row, 10);
				Review.HSplitTop(8 * Density, nullptr, &Review);
				Review.HSplitTop(28 * Density, &Row, &Review);
				if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[9], LCNTLocalize("Preview this file"), Row))
				{
					LucentBindWheel::CSource Source;
					if(LucentBindWheel::LoadSource(S.m_Path.GetString(), Source))
					{
						S.ChooseSource(Source, -1);
						S.m_ManualImport = false;
					}
					else
						S.m_ImportError = "Could not find a saved wheel in this file.";
				}
			}
			else if(S.m_Reviewed)
			{
				Review.HSplitTop(28, &Row, &Review);
				char aTitle[160];
				str_format(aTitle, sizeof(aTitle), LCNTLocalize("%s · %d actions"), LCNTLocalize(LucentBindWheel::SourceClient(S.m_Import)), int(S.m_Import.m_Data.m_Binds.size()));
				Text(pUi, pTextRender, Row, aTitle, 12);
				int Unavailable = 0;
				for(const auto &Bind : S.m_Import.m_Data.m_Binds)
					if(!Bind.m_Command.empty() && !S.m_pGame->Console()->LineIsValid(Bind.m_Command.c_str()))
						++Unavailable;
				if(S.m_Import.m_Data.m_Errors || Unavailable)
				{
					Review.HSplitTop(24, &Row, &Review);
					str_format(aTitle, sizeof(aTitle), LCNTLocalize("Actions needing attention: %d"), S.m_Import.m_Data.m_Errors ? S.m_Import.m_Data.m_Errors : Unavailable);
					Text(pUi, pTextRender, Row, aTitle, 10, true);
				}
				S.m_ImportList.SetActive(Active);
				S.m_ImportList.SetColors(LucentUi::Accent(.1f), LucentUi::Accent(.045f));
				S.m_ImportList.DoStart(34, int(S.m_Import.m_Data.m_Binds.size()), 1, 3, -1, &Review, false);
				for(const auto &Bind : S.m_Import.m_Data.m_Binds)
				{
					const auto Item = S.m_ImportList.DoNextItem(&Bind);
					if(Item.m_Visible)
					{
						CUIRect Name = Item.m_Rect, Command;
						Name.VMargin(4, &Name);
						Name.HSplitTop(19, &Name, &Command);
						Text(pUi, pTextRender, Name, Bind.m_Name.empty() ? "*" : Bind.m_Name.c_str(), 11);
						Text(pUi, pTextRender, Command, Bind.m_Command.c_str(), 9, true);
					}
				}
				S.m_ImportList.DoEnd();
			}
			else
			{
				Text(pUi, pTextRender, Review, LCNTLocalize("Save a wheel in another client, then refresh here."), 10, true);
			}
			CUIRect SecondaryLeft, SecondaryRight;
			L.m_Secondary.VSplitMid(&SecondaryLeft, &SecondaryRight, 12);
			if(LucentUi::TextButton(pUi, pTextRender, &S.m_Buttons[25], LCNTLocalize(S.m_ManualImport ? "Back to detected wheels" : "Choose a file manually"), SecondaryLeft, false, 10))
			{
				S.m_ManualImport = !S.m_ManualImport;
				if(!S.m_ManualImport)
					S.ClosePathInput();
				S.m_ImportError.clear();
			}
			if(LucentUi::TextButton(pUi, pTextRender, &S.m_Buttons[26], LCNTLocalize("Import options"), SecondaryRight, S.m_ImportOptions, 10))
				S.m_ImportOptions = !S.m_ImportOptions;
			// Layout was calculated before toggling; draw options on the next frame.
			if(L.m_Options.h > 0)
			{
				CUIRect Preferences, Keys;
				L.m_Options.HSplitMid(&Preferences, &Keys);
				if(LucentUi::FilledCheckBox(pUi, &S.m_Buttons[10], LCNTLocalize("Also copy wheel appearance and cursor settings"), S.m_ImportSettings, Preferences))
					S.m_ImportSettings = !S.m_ImportSettings;
				if(LucentUi::FilledCheckBox(pUi, &S.m_Buttons[11], LCNTLocalize("Also copy the wheel key if it is free"), S.m_ImportKeys, Keys))
					S.m_ImportKeys = !S.m_ImportKeys;
			}
			char aNotice[256];
			const int Existing = int(Wheel.Page(S.m_ImportPage).size());
			if(!S.m_ImportError.empty())
				str_copy(aNotice, LCNTLocalize(S.m_ImportError.c_str()));
			else if(S.m_Reviewed && S.m_Import.m_Data.m_Errors)
				str_copy(aNotice, LCNTLocalize("This wheel has damaged entries. Choose another source."));
			else if(!S.m_Reviewed)
				str_copy(aNotice, LCNTLocalize("Choose a detected wheel or select a file manually."));
			else if(Existing)
				str_format(aNotice, sizeof(aNotice), LCNTLocalize("This replaces %d actions on the selected page."), Existing);
			else
				str_copy(aNotice, LCNTLocalize("The other page and your original wheel stay unchanged."));
			Text(pUi, pTextRender, L.m_Notice, aNotice, 10, true);
			CUIRect Cancel, Apply;
			L.m_Footer.VSplitMid(&Cancel, &Apply, 12);
			if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[13], LCNTLocalize("Cancel"), Cancel))
			{
				S.ClosePathInput();
				return CUi::POPUP_CLOSE_CURRENT;
			}

			if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[12], LCNTLocalize("Import selected wheel"), Apply, true, S.m_Reviewed && !S.m_ManualImport && S.m_Import.m_Data.m_Errors == 0))
			{
				if(Wheel.Import(S.m_Import, S.m_ImportPage, S.m_ImportSettings, S.m_ImportKeys))
				{
					S.m_Page = S.m_ImportPage;
					S.Select(Wheel, -1);
					S.m_ImportDone = true;
				}
				else
					S.m_ImportError = "Could not import this wheel. Choose another source.";
			}
			return CUi::POPUP_KEEP_OPEN;
		}

	}
	void LeaveBindWheel(CUi *pUi)
	{
		auto &S = State();
		pUi->ClosePopupMenu(&S);
		pUi->ClosePopupMenu(&S.m_ActionPopup);
		if(S.m_DragSource >= 0 || S.m_RingSource >= 0)
			pUi->SetActiveItem(nullptr);
		S.m_RingSource = -1;
		S.m_RingDragging = false;
		S.m_DragSource = -1;
		S.m_Dragging = false;
		for(CLineInput *pInput : {static_cast<CLineInput *>(&S.m_Name), static_cast<CLineInput *>(&S.m_Command), static_cast<CLineInput *>(&S.m_Path)})
			if(pUi->ActiveItem() == pInput)
				pUi->SetActiveItem(nullptr);
	}
	void RenderBindWheel(CGameClient *pGame, CUi *pUi, ITextRender *pTextRender, CUIRect View)
	{
		auto &S = State();
		S.m_pGame = pGame;
		S.m_pUi = pUi;
		S.m_pTextRender = pTextRender;
		auto &Wheel = pGame->m_BindWheel;
		const auto L = CBindWheelLayout::Calculate(View);
		if(pUi->Enabled() && !pUi->IsPopupOpen() && !pGame->m_KeyBinder.IsActive() && !CLineInput::GetActiveInput() &&
			pGame->Input()->ModifierIsPressed() && !pGame->Input()->ShiftIsPressed() && pGame->Input()->KeyPress(KEY_Z))
		{
			S.m_DragSource = S.m_RingSource = -1;
			S.m_Dragging = S.m_RingDragging = false;
			pUi->SetActiveItem(nullptr);
			if(Wheel.Undo())
			{
				S.Select(Wheel, -1);
				S.m_Error.clear();
			}
		}
		CBindSlot Current = EMPTY_BIND_SLOT;
		for(int Mask = 0; Mask < KeyModifier::COMBINATION_COUNT && Current == EMPTY_BIND_SLOT; ++Mask)
			for(int KeyId = 1; KeyId < KEY_LAST; ++KeyId)
				if(str_comp(pGame->m_Binds.Get(KeyId, Mask), "+bindwheel") == 0)
				{
					Current = {KeyId, Mask};
					break;
				}
		CUIRect Title = L.m_Title;
		Text(pUi, pTextRender, Title, LCNTLocalize("Bind wheel"), 17);
		CUIRect Tabs = L.m_Tabs, First, Second, Import;
		Tabs.VSplitRight(std::min(180.f, Tabs.w * .38f), &Tabs, &Import);
		Tabs.w = std::min(Tabs.w - 8, 300.f);
		Tabs.VSplitMid(&First, &Second, 6);
		for(int PageIndex = 0; PageIndex < 2; ++PageIndex)
			if(LucentUi::FilterTab(pUi, pTextRender, &S.m_Buttons[PageIndex], LCNTLocalize(PageIndex ? "Page 2 · Shift" : "Page 1"), PageIndex ? Second : First, S.m_Page == PageIndex) && S.m_Page != PageIndex)
			{
				S.m_Page = PageIndex;
				S.Select(Wheel, -1);
			}
		if(LucentUi::TextButton(pUi, pTextRender, &S.m_Buttons[14], LCNTLocalize("Import wheel"), Import))
		{
			if(CLineInput::GetActiveInput())
				CLineInput::GetActiveInput()->Deactivate();
			pUi->SetActiveItem(nullptr);
			S.m_ImportPage = S.m_Page;
			S.m_ManualImport = false;
			S.m_ImportOptions = false;
			S.m_ImportSettings = false;
			S.m_ImportKeys = false;
			S.FindSources(Wheel);
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			const float W = std::min(620.f, pUi->Screen()->w - 32), H = std::min(440.f, pUi->Screen()->h - 32);
			pUi->DoPopupMenu(&S, (pUi->Screen()->w - W) / 2, (pUi->Screen()->h - H) / 2, W, H, &S, ImportDialog, Props);
		}
		const auto &Clock = LucentStyle::Motion::Clock();
		const bool Shift = pUi->Enabled() && !pUi->IsPopupOpen() && !pGame->m_KeyBinder.IsActive() && !CLineInput::GetActiveInput() && pGame->Input()->ShiftIsPressed();
		const bool DragActive = S.m_RingSource >= 0 || S.m_DragSource >= 0;
		const int DragPage = S.m_RingSource >= 0 ? S.m_RingPage : S.m_DragPage;
		const int PreviewTarget = DragActive ? (Shift ? 1 - DragPage : DragPage) : Shift ? 1 : S.m_Page;
		const auto Flip = S.m_PreviewFlip.Update(PreviewTarget, Clock.m_Now, Clock.m_Animate && g_Config.m_ClLucentBindwheelAnimation > 0);
		CUIRect Preview = L.m_Preview;
		Preview.VMargin(8, &Preview);
		CUIRect PreviewTitle;
		Preview.HSplitTop(24, &PreviewTitle, &Preview);
		Preview.Margin(8, &Preview);
		int RingHover = -1;
		const float Radius = std::min(Preview.w, Preview.h) / 2;
		if(pUi->Enabled() && !pUi->IsPopupOpen() && pUi->MouseHovered(&Preview))
			RingHover = LucentBindWheel::PreviewHit(pUi->MousePos() - Preview.Center(), Radius, Flip.m_Width, DragActive && Flip.m_Page != DragPage ? std::max(1, int(Wheel.Page(Flip.m_Page).size())) : int(Wheel.Page(Flip.m_Page).size()));
		if(RingHover >= 0 && pUi->MouseButtonClicked(0) && S.m_DragSource < 0)
		{
			S.m_Page = Flip.m_Page;
			S.Select(Wheel, RingHover);
			S.m_RingSource = RingHover;
			S.m_RingPage = Flip.m_Page;
			S.m_RingOrigin = pUi->MousePos();
			S.m_RingDragging = false;
			if(CLineInput::GetActiveInput())
				CLineInput::GetActiveInput()->Deactivate();
			pUi->SetActiveItem(&S.m_RingDragId);
		}
		if(S.m_RingSource >= 0)
		{
			const bool Cancel = !pUi->Enabled() || pUi->IsPopupOpen() || S.m_Page != S.m_RingPage || pGame->Input()->KeyPress(KEY_ESCAPE);
			S.m_RingDragging |= distance(S.m_RingOrigin, pUi->MousePos()) > 4;
			if(Cancel || !pUi->MouseButton(0))
			{
				if(!Cancel && S.m_RingDragging && RingHover >= 0)
				{
					if(Flip.m_Page == S.m_RingPage)
					{
						Wheel.Reorder(S.m_RingPage, S.m_RingSource, RingHover);
						S.Select(Wheel, RingHover);
					}
					else if(Wheel.Transfer(S.m_RingPage, S.m_RingSource, Flip.m_Page, RingHover))
					{
						S.m_Page = Flip.m_Page;
						S.Select(Wheel, RingHover);
					}
				}
				S.m_RingSource = -1;
				S.m_RingDragging = false;
				pUi->SetActiveItem(nullptr);
			}
		}
		CUIRect Editor = L.m_Editor;
		Editor.VMargin(8, &Editor);
		CUIRect List, Form;
		Editor.HSplitBottom(std::min(114.f, Editor.h * .48f), &List, &Form);
		List.h = std::max(0.f, List.h - 12.f);
		const auto &Page = Wheel.Page(S.m_Page);
		CUIRect Heading;
		List.HSplitTop(24, &Heading, &List);
		CUIRect NewAction, CountRect;
		Heading.VSplitRight(24, &Heading, &NewAction);
		Heading.VSplitRight(std::min(64.f, Heading.w * .3f), &Heading, &CountRect);
		Text(pUi, pTextRender, Heading, LCNTLocalize("Actions"), 11);
		Text(pUi, pTextRender, CountRect, (std::to_string(Page.size()) + " / 64").c_str(), 9, true);
		if(LucentUi::IconButton(pUi, pTextRender, &S.m_Buttons[15], FontIcon::PLUS, LCNTLocalize("New action"), NewAction))
		{
			S.Select(Wheel, -1);
			S.m_DragSource = -1;
		}
		int RowHover = -1, DropTarget = -1;
		S.m_List.SetColors(LucentUi::Accent(LucentStyle::Style::Selected), LucentUi::Accent(LucentStyle::Style::Hover));
		S.m_List.DoStart(40, int(Page.size()), 1, 3, S.m_Selected, &List, false);
		for(int i = 0; i < int(Page.size()); ++i)
		{
			const auto Item = S.m_List.DoNextItem(&S.m_Items[i], S.m_Selected == i);
			if(Item.m_Visible)
			{
				if(Flip.m_Page == S.m_Page && RingHover == i)
					Item.m_Rect.Draw(LucentUi::Accent(.07f), IGraphics::CORNER_ALL, 3);
				if(pUi->Enabled() && !pUi->IsPopupOpen() && pUi->MouseHovered(&List) && pUi->MouseHovered(&Item.m_Rect))
				{
					RowHover = i;
					DropTarget = i;
				}
				if(S.m_Selected == i)
				{
					CUIRect Indicator = Item.m_Rect;
					Indicator.HMargin(7, &Indicator);
					Indicator.w = 2;
					Indicator.Draw(LucentUi::Accent(1), IGraphics::CORNER_ALL, 1);
				}
				CUIRect Name = Item.m_Rect;
				Name.VMargin(8, &Name);
				CUIRect Handle;
				Name.VSplitLeft(16, &Handle, &Name);
				pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
				Text(pUi, pTextRender, Handle, FontIcon::ARROWS_UP_DOWN, 9, true);
				pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
				if(pUi->Enabled() && !pUi->IsPopupOpen() && pUi->MouseHovered(&List) && pUi->MouseHovered(&Handle) && pUi->MouseButtonClicked(0))
				{
					S.m_DragSource = i;
					S.m_DragPage = S.m_Page;
					if(CLineInput::GetActiveInput())
						CLineInput::GetActiveInput()->Deactivate();
					S.m_DragOrigin = pUi->MousePos();
					S.m_Dragging = false;
					pUi->SetActiveItem(&S.m_DragIds[i]);
					S.Select(Wheel, i);
				}
				if(S.m_DragSource >= 0 && S.m_Dragging && DropTarget == i)
				{
					CUIRect Marker = Item.m_Rect;
					Marker.h = 1;
					Marker.Draw(LucentUi::Accent(.8f), 0, 0);
				}
				CUIRect Number;
				Name.VSplitLeft(24, &Number, &Name);
				Text(pUi, pTextRender, Number, std::to_string(i + 1).c_str(), 9, true);
				CUIRect Command;
				Name.HSplitTop(19, &Name, &Command);
				Marquee(pUi, pTextRender, Name, Page[i].m_Name.empty() ? "*" : Page[i].m_Name.c_str(), 11, false);
				Marquee(pUi, pTextRender, Command, Page[i].m_Command.c_str(), 9, true);
			}
		}
		const int Selected = S.m_List.DoEnd();
		if(S.m_DragSource < 0 && S.m_RingSource < 0 && (Selected != S.m_Selected || S.m_List.WasItemSelected()))
			S.Select(Wheel, Selected);
		if(Page.empty())
			Text(pUi, pTextRender, List, LCNTLocalize("Add your first action below"), 11, true);
		if(S.m_DragSource >= 0)
		{
			if(!pUi->Enabled() || pUi->IsPopupOpen() || S.m_Page != S.m_DragPage || pGame->Input()->KeyPress(KEY_ESCAPE))
			{
				S.m_DragSource = -1;
				S.m_Dragging = false;
				pUi->SetActiveItem(nullptr);
			}
			else
			{
				if(distance(S.m_DragOrigin, pUi->MousePos()) > 4)
					S.m_Dragging = true;
				if(!pUi->MouseButton(0))
				{
					if(S.m_Dragging && RingHover >= 0)
					{
						if(Flip.m_Page == S.m_DragPage)
						{
							Wheel.Reorder(S.m_DragPage, S.m_DragSource, RingHover);
							S.Select(Wheel, RingHover);
						}
						else if(Wheel.Transfer(S.m_DragPage, S.m_DragSource, Flip.m_Page, RingHover))
						{
							S.m_Page = Flip.m_Page;
							S.Select(Wheel, RingHover);
						}
					}
					else if(S.m_Dragging && DropTarget >= 0)
					{
						Wheel.Reorder(S.m_DragPage, S.m_DragSource, DropTarget);
						S.Select(Wheel, DropTarget);
					}
					S.m_DragSource = -1;
					S.m_Dragging = false;
					pUi->SetActiveItem(nullptr);
				}
			}
		}
		const auto &EditorPage = Wheel.Page(S.m_Page);
		CUIRect Row;
		const float FormScale = std::min(1.f, Form.h / 114.f);
		const auto Field = [&](float Height) -> CUIRect & { Form.HSplitTop(Height * FormScale, &Row, &Form); return Row; };
		const bool HasSelection = S.m_Selected >= 0 && S.m_Selected < int(EditorPage.size());
		CUIRect Mode = Field(24), More;
		Mode.VSplitRight(24, &Mode, &More);
		const std::string ModeText = HasSelection ? std::string(LCNTLocalize("Editing")) + " · " + EditorPage[S.m_Selected].m_Name : LCNTLocalize("New action");
		Text(pUi, pTextRender, Mode, ModeText.c_str(), 11);
		if(HasSelection && LucentUi::IconButton(pUi, pTextRender, &S.m_Buttons[16], FontIcon::ELLIPSIS, LCNTLocalize("Action options"), More))
		{
			S.m_ActionPage = S.m_Page;
			S.m_ActionIndex = S.m_Selected;
			S.m_ActionBind = EditorPage[S.m_Selected];
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			pUi->DoPopupMenu(&S.m_ActionPopup, More.x, More.y + More.h, 190, 94, &S, ActionDialog, Props);
		}
		CUIRect NameLabel, CommandLabel, NameField, CommandField;
		Field(16).VSplitLeft(Form.w * .33f, &NameLabel, &CommandLabel);
		CommandLabel.VSplitLeft(8, nullptr, &CommandLabel);
		Text(pUi, pTextRender, NameLabel, LCNTLocalize("Action name"), 9, true);
		Text(pUi, pTextRender, CommandLabel, LCNTLocalize("Console command"), 9, true);
		Field(24).VSplitLeft(Form.w * .33f, &NameField, &CommandField);
		CommandField.VSplitLeft(8, nullptr, &CommandField);
		pUi->DoEditBox(&S.m_Name, &NameField, 11);
		pUi->DoEditBox(&S.m_Command, &CommandField, 10);
		Field(8);
		CUIRect Apply = Field(26), Up, Down;
		Apply.VSplitRight(26, &Apply, &Down);
		Apply.VSplitRight(26, &Apply, &Up);
		Apply.w = std::min(Apply.w - 6, 190.f);
		const LucentBindWheel::CBind Edited{S.m_Name.GetString(), S.m_Command.GetString()};
		const bool Changed = HasSelection ? !(Edited == EditorPage[S.m_Selected]) : !Edited.m_Name.empty() || !Edited.m_Command.empty();
		if(LucentUi::Button(pUi, pTextRender, &S.m_Buttons[2], LCNTLocalize(HasSelection ? "Save changes" : "Add action"), Apply, false, Changed && (HasSelection || EditorPage.size() < LucentBindWheel::MaxBinds)))
		{
			const int Index = HasSelection ? S.m_Selected : int(EditorPage.size());
			if(Wheel.Set(S.m_Page, Index, Edited))
			{
				S.Select(Wheel, Index);
				S.m_Error.clear();
			}
		}
		{
			LucentUi::CInteractionScope Enabled(pUi, S.m_Selected > 0);
			if(LucentUi::IconButton(pUi, pTextRender, &S.m_Buttons[5], FontIcon::CHEVRON_UP, LCNTLocalize("Move up"), Up))
			{
				Wheel.Move(S.m_Page, S.m_Selected, -1);
				S.Select(Wheel, S.m_Selected - 1);
			}
		}
		{
			LucentUi::CInteractionScope Enabled(pUi, S.m_Selected >= 0 && S.m_Selected + 1 < int(EditorPage.size()));
			if(LucentUi::IconButton(pUi, pTextRender, &S.m_Buttons[6], FontIcon::CHEVRON_DOWN, LCNTLocalize("Move down"), Down))
			{
				Wheel.Move(S.m_Page, S.m_Selected, 1);
				S.Select(Wheel, S.m_Selected + 1);
			}
		}
		Text(pUi, pTextRender, PreviewTitle, LCNTLocalize("Preview"), 12);
		const int Highlight = RingHover >= 0 ? RingHover : Flip.m_Page == S.m_Page ? RowHover :
											     -1;
		Wheel.Draw(Preview, Flip.m_Page, Flip.m_Page == S.m_Page ? S.m_Selected : -1, 1, Flip.m_Width, true, Highlight);
		const int CarriedIndex = S.m_RingSource >= 0 ? S.m_RingSource : S.m_DragSource;
		const int CarriedPage = S.m_RingSource >= 0 ? S.m_RingPage : S.m_DragPage;
		if((S.m_RingDragging || S.m_Dragging) && CarriedIndex >= 0 && CarriedIndex < int(Wheel.Page(CarriedPage).size()))
		{
			CUIRect Carried{pUi->MousePos().x + 12, pUi->MousePos().y + 12, 140, 24};
			Carried.x = std::min(Carried.x, pUi->Screen()->w - Carried.w);
			Carried.y = std::min(Carried.y, pUi->Screen()->h - Carried.h);
			LucentUi::Surface(Carried);
			Carried.VMargin(6, &Carried);
			Text(pUi, pTextRender, Carried, Wheel.Page(CarriedPage)[CarriedIndex].m_Name.c_str(), 10);
		}

		CUIRect Controls = L.m_Controls;
		const float ControlScale = std::min(1.f, Controls.h / 130.f);
		const auto Next = [&](float Height = 24.f) -> CUIRect & { Controls.HSplitTop(Height * ControlScale, &Row, &Controls); return Row; };
		CUIRect KeyLabel, Key;
		Next().VSplitLeft(std::min(120.f, Controls.w * .42f), &KeyLabel, &Key);
		Text(pUi, pTextRender, KeyLabel, LCNTLocalize("Wheel key"), 10);

		const auto KeyResult = pGame->m_KeyBinder.DoKeyReader(&S.m_Key, &S.m_ClearKey, &Key, Current, false);
		if(!KeyResult.m_Aborted && KeyResult.m_Bind != Current)
		{
			if(KeyResult.m_Bind.m_Key == KEY_LSHIFT || KeyResult.m_Bind.m_Key == KEY_RSHIFT)
				S.m_Error = "Shift is reserved for switching wheel pages.";
			else
			{
				if(Current != EMPTY_BIND_SLOT)
					pGame->m_Binds.Bind(Current.m_Key, "", false, Current.m_ModifierMask);
				if(KeyResult.m_Bind != EMPTY_BIND_SLOT)
					pGame->m_Binds.Bind(KeyResult.m_Bind.m_Key, "+bindwheel", false, KeyResult.m_Bind.m_ModifierMask);
			}
		}
		if(LucentUi::FilledCheckBox(pUi, &S.m_Buttons[7], LCNTLocalize("Reset cursor when opening"), g_Config.m_ClLucentBindwheelResetMouse != 0, Next()))
			g_Config.m_ClLucentBindwheelResetMouse ^= 1;
		char aValue[160];
		str_format(aValue, sizeof(aValue), "%s  %d%%", LCNTLocalize("Wheel size"), g_Config.m_ClLucentBindwheelScale);
		Text(pUi, pTextRender, Next(18), aValue, 10, true);
		g_Config.m_ClLucentBindwheelScale = int(pUi->DoScrollbarH(&g_Config.m_ClLucentBindwheelScale, &Next(12), (g_Config.m_ClLucentBindwheelScale - 50) / 50.f, nullptr, true, true) * 50 + 50);
		str_format(aValue, sizeof(aValue), "%s  %d ms", LCNTLocalize("Animation"), g_Config.m_ClLucentBindwheelAnimation);
		Text(pUi, pTextRender, Next(24), aValue, 10, true);
		g_Config.m_ClLucentBindwheelAnimation = int(pUi->DoScrollbarH(&g_Config.m_ClLucentBindwheelAnimation, &Next(12), g_Config.m_ClLucentBindwheelAnimation / 1000.f, nullptr, true, true) * 1000);
		if(!S.m_Error.empty())
			Text(pUi, pTextRender, L.m_Footer, LCNTLocalize(S.m_Error.c_str()), 9, true);
	}
}
