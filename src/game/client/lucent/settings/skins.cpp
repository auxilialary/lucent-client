#include "skins.h"

#include "mode_render.h"

#include <game/client/lineinput.h>
#include <game/client/lucent/ui.h>
#include <game/client/lucent/ui/color_picker.h>
#include <game/client/ui_scrollregion.h>
namespace LucentSettings
{
	namespace
	{
		struct CView
		{
			CModePresentation m_Mode, m_Profile;
			std::array<CButtonContainer, 2> m_Modes, m_Profiles;
			CScrollRegion m_Left, m_Advanced;
			bool m_GalleryFocused = false;
		};

		struct CColorPopup : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pTextRender = nullptr;
			unsigned *m_pColor = nullptr;
			bool m_Alpha = false;
			float m_Darkest = 0;
			LucentUi::CColorPickerState m_State;
			std::function<void()> m_Changed;
		};
		CColorPopup &ColorPopup()
		{
			static CColorPopup s_Popup;
			return s_Popup;
		}
		CUi::EPopupMenuFunctionResult PaintColor(void *pContext, CUIRect View, bool Active)
		{
			auto &Popup = *static_cast<CColorPopup *>(pContext);
			LucentUi::CScope Style(Popup.m_pUi);
			LucentUi::CInteractionScope Input(Popup.m_pUi, Active);
			if(LucentUi::ColorPicker(Popup.m_pUi, Popup.m_pTextRender, View, Popup.m_pColor, Popup.m_Alpha, Popup.m_Darkest, Popup.m_State) && Popup.m_Changed)
				Popup.m_Changed();
			return CUi::POPUP_KEEP_OPEN;
		}
		CView &ViewState()
		{
			static CView s_View;
			return s_View;
		}
		void Keyboard(CUi *pUi, CModePresentation &Mode)
		{
			if(!pUi->Enabled() || CLineInput::GetActiveInput())
				Mode.ClearKeyboardFocus();
			else
			{
				if(Mode.m_ModeFocused)
				{
					if(pUi->ConsumeHotkey(CUi::HOTKEY_LEFT))
						Mode.m_FocusedAdvanced = false;
					if(pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT))
						Mode.m_FocusedAdvanced = true;
				}
			}
			if(pUi->Enabled() && pUi->MouseButtonClicked(0))
				Mode.ClearKeyboardFocus();
		}
	}

	bool SkinGalleryKeyboardFocused() { return ViewState().m_GalleryFocused; }
	void OpenSkinColorPopup(CUi *pUi, ITextRender *pTextRender, CUIRect Anchor, unsigned *pColor, bool Alpha, float Darkest, const std::function<void()> &Changed)
	{
		if(!pUi->Enabled() || pUi->IsPopupOpen())
			return;
		auto &Popup = ColorPopup();
		Popup.m_pUi = pUi;
		Popup.m_pTextRender = pTextRender;
		Popup.m_pColor = pColor;
		Popup.m_Alpha = Alpha;
		Popup.m_Darkest = Darkest;
		Popup.m_State.Cancel(pUi);
		Popup.m_State = {};
		Popup.m_Changed = Changed;
		SPopupMenuProperties Props;
		Props.m_ClampToScreen = true;
		Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
		Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
		pUi->DoPopupMenu(&Popup, Anchor.x, Anchor.y + Anchor.h + 4, std::min(380.f, std::max(1.f, pUi->Screen()->w - 12)), std::min(202.f, std::max(1.f, pUi->Screen()->h - 12)), &Popup, PaintColor, Props);
	}
	void LeaveSkinManager(CUi *pUi)
	{
		auto &Page = ViewState();
		if(pUi->IsPopupOpen(&ColorPopup()))
			pUi->ClosePopupMenu(&ColorPopup(), true);
		ColorPopup().m_State.Cancel(pUi);
		ColorPopup().m_Changed = {};
		ColorPopup().m_pColor = nullptr;
		Page.m_Mode.Leave();
		Page.m_Profile.Leave();
		Page.m_GalleryFocused = false;
		for(auto *pButtons : {&Page.m_Modes, &Page.m_Profiles})
			for(auto &Button : *pButtons)
			{
				if(pUi->CheckActiveItem(&Button))
					pUi->SetActiveItem(nullptr);
				if(pUi->HotItem() == &Button)
					pUi->SetHotItem(nullptr);
			}
	}
	void RenderSkinManager(CUi *pUi, ITextRender *pTextRender, CUIRect View, bool &Dummy, bool CustomColors, const FSkinControl &Draw, const FSkinContent &Gallery, const FSkinContent &Seven)
	{
		using C = ESkinControl;
		LucentUi::CScope Style(pUi);
		LucentUi::CInteractionScope PopupInput(pUi, !pUi->IsPopupOpen());
		auto &Page = ViewState();
		auto &Mode = Page.m_Mode;
		const auto &Clock = LucentStyle::Motion::Clock();
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		if(pUi->Enabled() && !CLineInput::GetActiveInput() && pUi->ConsumeHotkey(CUi::HOTKEY_TAB))
		{
			if(Mode.m_ModeFocused && !Mode.m_ShownAdvanced)
			{
				Mode.ClearKeyboardFocus();
				Page.m_Profile.FocusMode();
			}
			else if(Page.m_Profile.m_ModeFocused && !Mode.m_ShownAdvanced)
			{
				Page.m_Profile.ClearKeyboardFocus();
				Page.m_GalleryFocused = true;
			}
			else
			{
				Page.m_GalleryFocused = false;
				Mode.FocusMode();
			}
		}
		if(pUi->MouseButtonClicked(0))
			Page.m_GalleryFocused = false;
		Keyboard(pUi, Mode);
		const auto Header = RenderModeHeader(pUi, pTextRender, SettingsContentRegion(View), Mode, Page.m_Modes, pUi->Enabled() && !pUi->ActiveItem() && !CLineInput::GetActiveInput(), Localize("Skins"), Localize("Advanced settings", "General settings"));
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		View.y = Header.m_Content.y;
		View.h = std::max(0.f, Header.m_Content.h);
		LucentUi::COpacityScope Fade(pTextRender, Mode.m_ModeReveal.m_Value);
		LucentUi::CInteractionScope ModeInput(pUi, Mode.ContentInteractive());
		if(Mode.m_ShownAdvanced)
		{
			Page.m_Profile.ClearKeyboardFocus();
			Page.m_GalleryFocused = false;
			CScrollRegionParams Params;
			Params.m_ScrollbarThickness = 8;
			Params.m_ScrollbarMargin = 1;
			Page.m_Advanced.Begin(&View, &Params);
			const auto Layout = CSkinManagerLayout::Advanced(View);
			float Y = View.y;
			const auto Item = [&](C Control, float Height) {
				CUIRect R{Layout.m_Profile.x, Y, Layout.m_Profile.w, Height};
				Y += Height + 4;
				Page.m_Advanced.AddRect(R);
				LucentUi::CInteractionScope Input(pUi, !Page.m_Advanced.RectClipped(R));
				Draw(Control, R);
			};
			Item(C::DOWNLOAD, 36);
			Item(C::COMMUNITY, 36);
			Item(C::SOURCE_INFO, 24);
			Item(C::VANILLA, 36);
			Item(C::FAT, 36);
			const float LeftBottom = Y;
			Y = Layout.m_TwoColumns ? View.y : Y + 14;
			CUIRect Right = Layout.m_TwoColumns ? Layout.m_Gallery : Layout.m_Profile;
			const auto RightItem = [&](C Control, float Height) {
				CUIRect R{Right.x, Y, Right.w, Height};
				Y += Height + 6;
				Page.m_Advanced.AddRect(R);
				LucentUi::CInteractionScope Input(pUi, !Page.m_Advanced.RectClipped(R));
				Draw(Control, R);
			};
			RightItem(C::QUALITY, 54);
			RightItem(C::LOADED_MAX, 54);
			RightItem(C::COMMUNITY_URL, 54);
			RightItem(C::RESET_SOURCE, 28);
			Page.m_Advanced.AddRect({View.x, std::max(Y, LeftBottom), View.w, 1});
			Page.m_Advanced.End();
			return;
		}
		auto &Profile = Page.m_Profile;
		if(Profile.m_LastFrame < 0)
			Profile.m_Advanced = Dummy;
		Profile.UpdateMode(Clock.m_Now, Clock.m_Animate);
		if(!pUi->Enabled() || CLineInput::GetActiveInput())
			Profile.ClearKeyboardFocus();
		if(Mode.m_ModeFocused && pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
		{
			Mode.ClearKeyboardFocus();
			Profile.FocusMode();
		}
		if(Profile.m_ModeFocused)
		{
			if(pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
			{
				Profile.ClearKeyboardFocus();
				Page.m_GalleryFocused = true;
			}
			if(pUi->ConsumeHotkey(CUi::HOTKEY_LEFT))
				Profile.m_FocusedAdvanced = false;
			if(pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT))
				Profile.m_FocusedAdvanced = true;
			if(pUi->ConsumeHotkey(CUi::HOTKEY_UP))
			{
				Profile.ClearKeyboardFocus();
				Mode.FocusMode();
			}
		}
		const bool OldProfile = Profile.m_Advanced;
		const auto ProfileHeader = RenderModeHeader(pUi, pTextRender, SettingsContentRegion(View), Profile, Page.m_Profiles, pUi->Enabled() && !pUi->ActiveItem() && !CLineInput::GetActiveInput(), Localize("Player"), Localize("Dummy"));
		if(OldProfile != Profile.m_Advanced)
		{
			if(auto *pInput = CLineInput::GetActiveInput())
				pInput->Deactivate();
		}
		Profile.UpdateMode(Clock.m_Now, Clock.m_Animate);
		Dummy = Profile.m_ShownAdvanced;
		View.y = ProfileHeader.m_Content.y;
		View.h = std::max(0.f, ProfileHeader.m_Content.h);
		LucentUi::COpacityScope ProfileFade(pTextRender, Profile.m_ModeReveal.m_Value);
		LucentUi::CInteractionScope ProfileInput(pUi, Profile.ContentInteractive());
		if(pUi->Enabled() && !CLineInput::GetActiveInput() && !Mode.m_ModeFocused && !Profile.m_ModeFocused)
			for(int Key : {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN, KEY_RETURN})
				if(pUi->Input()->KeyPress(Key))
					Page.m_GalleryFocused = true;

		if(Seven)
		{
			Seven(View);
			return;
		}
		const auto Layout = CSkinManagerLayout::Calculate(View);
		CUIRect Left = Layout.m_Profile;
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		Page.m_Left.Begin(&Left, &Params);
		float Y = Left.y;
		const auto Item = [&](C Control, float Height) {
			CUIRect R{Left.x, Y, Left.w, Height};
			Y += Height + 4;
			Page.m_Left.AddRect(R);
			LucentUi::CInteractionScope Input(pUi, !Page.m_Left.RectClipped(R));
			Draw(Control, R);
		};
		Item(C::PREVIEW, 60);
		Item(C::NAME, 28);
		Item(C::STATUS, 20);
		Item(C::CUSTOM_COLORS, 32);
		if(CustomColors)
		{
			Item(C::BODY, 28);
			Item(C::FEET, 28);
		}
		Item(C::EMOTIONS, 18);
		Item(C::EYES, 34);
		Item(C::RANDOM, 28);
		Item(C::PREFIX, 34);
		Item(C::COOLDOWN, 40);
		Page.m_Left.End();
		Gallery(Layout.m_Gallery);
	}
}
