#include "sidebar.h"

#include <engine/font_icons.h>
#include <engine/keys.h>
#include <engine/storage.h>

#include <game/client/lineinput.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui.h>
#include <game/localization.h>

namespace LucentSettings
{
	void CSidebar::Init(IGraphics *pGraphics)
	{
		m_aWorkspaceTextures[0] = pGraphics->LoadTexture("lucent/ddnet.png", IStorage::TYPE_ALL);
		m_aWorkspaceTextures[1] = pGraphics->LoadTexture("lucent/client.png", IStorage::TYPE_ALL);
	}
	void CSidebar::Shutdown(IGraphics *pGraphics)
	{
		for(auto &Texture : m_aWorkspaceTextures)
			pGraphics->UnloadTexture(&Texture);
	}
	void CSidebar::WorkspaceInput(CUi *pUi, CUIRect View, int &Workspace, bool Blocked)
	{
		if(!pUi->Enabled() || Blocked || pUi->IsPopupOpen())
			return;
		const auto Layout = CLayout::Calculate(View, m_Navigation.m_Expansion.m_Value, m_Count);
		int Next = Workspace;
		if(!pUi->ActiveItem() && !CLineInput::GetActiveInput() && pUi->Input()->KeyPress(KEY_F7))
			Next ^= 1;
		if(!pUi->ActiveItem() || Owns(pUi->ActiveItem()) || CLineInput::GetActiveInput())
			for(int i = 0; i < 2; ++i)
			{
				if(!Layout.m_WorkspacesExpanded && !pUi->CheckActiveItem(&m_aWorkspaceButtons[i]))
					continue;
				// Expansion moves the right button. Keep the press identity until
				// release within the sidebar, so collapsed icon clicks still work.
				const bool CapturedRelease = pUi->CheckActiveItem(&m_aWorkspaceButtons[i]) && !pUi->MouseButton(0) && pUi->MouseHovered(&Layout.m_Sidebar);
				if(pUi->DoButtonLogic(&m_aWorkspaceButtons[i], Workspace == i, &Layout.m_aWorkspaces[i], BUTTONFLAG_LEFT) || CapturedRelease)
					Next = i;
			}
		if(Next != Workspace)
		{
			Workspace = Next;
			if(auto *pInput = CLineInput::GetActiveInput())
				pInput->Deactivate();
			pUi->SetActiveItem(nullptr);
			pUi->ClearHotkeys();
		}
	}

	bool CSidebar::Owns(const void *pId) const
	{
		if(pId == this || pId == &m_BackButton)
			return true;
		for(const auto &Button : m_aWorkspaceButtons)
			if(pId == &Button)
				return true;
		for(const auto &Button : m_aButtons)
			if(pId == &Button)
				return true;
		return false;
	}

	CLayout CSidebar::Begin(CUi *pUi, CUIRect View, int &Selected, bool Blocked, int Count, bool NavigationBlocked)
	{
		m_BackRequested = false;
		Count = std::clamp(Count, 1, CategoryCount);
		if(m_Count != Count)
		{
			// Preserve expansion and physical bounds when workspace items change.
			if(m_Navigation.m_Focus >= 0)
				m_Navigation.Focus(Selected, Count);
			m_Navigation.m_Scroll = 0;
			if(Owns(pUi->ActiveItem()))
				pUi->SetActiveItem(nullptr);
			m_Count = Count;
		}
		const auto &Clock = LucentStyle::Motion::Clock();
		m_Interactive = pUi->Enabled() && !Blocked && !pUi->IsPopupOpen();
		const bool ExternalActive = pUi->ActiveItem() && !Owns(pUi->ActiveItem());
		const bool Editing = CLineInput::GetActiveInput() != nullptr;
		if(!m_Interactive)
			Reset();
		m_Layout = CLayout::Calculate(View, m_Navigation.m_Expansion.m_Value, m_Count);
		if(m_Interactive && pUi->MouseButtonClicked(0) && !pUi->MouseHovered(&m_Layout.m_Sidebar))
			m_Navigation.m_Focus = -1;

		// F6 is an explicit escape from content editing into navigation; Tab enters
		// only when content does not own an input. Tab/Right/Escape return to content.
		if(m_Interactive && !NavigationBlocked && (!ExternalActive || Editing))
		{
			if(pUi->Input()->KeyPress(KEY_F6))
			{
				if(auto *pInput = CLineInput::GetActiveInput())
					pInput->Deactivate();
				pUi->SetActiveItem(nullptr);
				m_Navigation.Focus(Selected, m_Count);
			}
			else if(!Editing && pUi->ConsumeHotkey(CUi::HOTKEY_TAB))
			{
				if(m_Navigation.m_Focus < 0)
					m_Navigation.Focus(Selected, m_Count);
				else
					m_Navigation.m_Focus = -1;
			}
			if(m_Navigation.m_Focus >= 0)
			{
				if(pUi->ConsumeHotkey(CUi::HOTKEY_UP))
					m_Navigation.Move(-1, m_Count);
				if(pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
					m_Navigation.Move(1, m_Count);
				if(pUi->ConsumeHotkey(CUi::HOTKEY_HOME))
					m_Navigation.Focus(0, m_Count);
				if(pUi->ConsumeHotkey(CUi::HOTKEY_END))
					m_Navigation.Focus(m_Count - 1, m_Count);
				if(pUi->ConsumeHotkey(CUi::HOTKEY_ENTER))
					Selected = m_Navigation.m_Focus;
				if(pUi->ConsumeHotkey(CUi::HOTKEY_ESCAPE) || pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT))
					m_Navigation.m_Focus = -1;
			}
		}
		const bool CanExpand = m_Interactive && !ExternalActive && !CLineInput::GetActiveInput();
		const bool Hover = CanExpand && pUi->MouseHovered(&m_Layout.m_Sidebar);
		const float Reveal = m_Navigation.UpdateExpansion(CanExpand && (Hover || m_Navigation.m_Focus >= 0), Clock.m_Now, Clock.m_Animate);
		m_Layout = CLayout::Calculate(View, Reveal, m_Count);
		m_Navigation.Scroll(0, m_Layout);
		if(m_Navigation.m_Focus >= 0)
			m_Navigation.RevealFocus(m_Layout);

		if(m_Interactive && !ExternalActive && pUi->MouseHovered(&m_Layout.m_Sidebar))
		{
			// WorkspaceInput owns its button IDs; do not replace their hot item.
			if(!pUi->MouseHovered(&m_Layout.m_aWorkspaces[0]) && !pUi->MouseHovered(&m_Layout.m_aWorkspaces[1]))
				pUi->SetHotItem(this);
			const bool Up = pUi->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP);
			const bool Down = pUi->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN);
			if(pUi->MouseHovered(&m_Layout.m_Viewport))
			{
				if(Up)
					m_Navigation.Scroll(-RowHeight, m_Layout);
				if(Down)
					m_Navigation.Scroll(RowHeight, m_Layout);
			}
		}
		for(int i = 0; i < m_Count; ++i)
		{
			const auto Hit = m_Layout.Hitbox(i, m_Navigation.m_Scroll);
			if(!m_Interactive || NavigationBlocked || ExternalActive || Hit.h <= 0)
				continue;
			if(pUi->DoButtonLogic(&m_aButtons[i], Selected == i, &Hit, BUTTONFLAG_LEFT))
			{
				if(auto *pInput = CLineInput::GetActiveInput())
					pInput->Deactivate();
				Selected = i;
				m_Navigation.m_Focus = -1;
			}
		}
		if(m_Interactive && !ExternalActive && pUi->DoButtonLogic(&m_BackButton, 0, &m_Layout.m_Back, BUTTONFLAG_LEFT))
		{
			if(auto *pInput = CLineInput::GetActiveInput())
				pInput->Deactivate();
			m_BackRequested = true;
		}
		if(m_Navigation.m_Focus >= 0)
		{
			// Lists in the existing pages also consume these keys. Keep the whole
			// keyboard gesture in navigation, including unused PageUp/Left/etc.
			pUi->ClearHotkeys();
		}
		return m_Layout;
	}

	bool CSidebar::BlocksContent(CUi *pUi) const
	{
		return m_Interactive && (m_Navigation.m_Focus >= 0 || Owns(pUi->ActiveItem()) ||
						(pUi->MouseHovered(&m_Layout.m_Sidebar) && (!pUi->ActiveItem() || Owns(pUi->ActiveItem()))));
	}

	void CSidebar::Render(CUi *pUi, ITextRender *pTextRender, int Selected, const char *const *ppLabels, const char *const *ppIcons, int Workspace, float CategoryOpacity)
	{
		static const char *const s_apIcons[CategoryCount] = {
			FontIcon::EARTH_AMERICAS, FontIcon::GEAR, FontIcon::USER, FontIcon::BRUSH,
			FontIcon::EYE, FontIcon::KEYBOARD, FontIcon::VIDEO, FontIcon::MUSIC,
			FontIcon::FLAG_CHECKERED, FontIcon::IMAGE, FontIcon::INFO};
		LucentUi::Surface(m_Layout.m_Sidebar);
		// Opaque token color inside the shared surface border: expanded content
		// must not bleed through the navigation overlay.
		CUIRect Opaque;
		m_Layout.m_Sidebar.Margin(LucentStyle::Style::Hairline, &Opaque);
		Opaque.Draw(LucentStyle::Style::Surface.WithAlpha(1), IGraphics::CORNER_ALL, LucentUi::RADIUS);
		for(int i = 0; i < 2; ++i)
		{
			const CUIRect Button = m_Layout.WorkspaceVisual(i);
			const float Hot = LucentUi::Highlight(&m_aWorkspaceButtons[i], m_Interactive && m_Layout.m_WorkspacesExpanded && pUi->MouseHovered(&Button));
			const float Active = LucentUi::Highlight(&m_aWorkspaceButtons[i], Workspace == i, LucentUi::EChannel::ACTIVE);
			const float Pressed = pUi->CheckActiveItem(&m_aWorkspaceButtons[i]) ? LucentStyle::Style::Pressed : 0;
			const float Reveal = m_Layout.m_WorkspaceReveal;
			const float IconOpacity = LucentStyle::Motion::Lerp(Active, 1.f, Reveal);
			if(IconOpacity <= .001f)
				continue;
			Button.Draw(LucentUi::Accent((LucentStyle::Style::Selected * Active + LucentStyle::Style::Hover * Hot + Pressed) * Reveal), IGraphics::CORNER_ALL, LucentUi::RADIUS);
			pUi->ClipEnable(&Button);
			const CUIRect Icon = m_Layout.WorkspaceIcon(i);
			pUi->Graphics()->TextureSet(m_aWorkspaceTextures[i]);
			pUi->Graphics()->QuadsBegin();
			pUi->Graphics()->SetColor(1, 1, 1, IconOpacity);
			const IGraphics::CQuadItem Quad(Icon.x, Icon.y, Icon.w, Icon.h);
			pUi->Graphics()->QuadsDrawTL(&Quad, 1);
			pUi->Graphics()->QuadsEnd();
			const char *pLabel = i == 0 ? LCNTLocalize("DDNet") : LCNTLocalize("Lucent");
			CUIRect Label{Icon.x + Icon.w + 4, Button.y, std::max(0.f, Button.x + Button.w - Icon.x - Icon.w - 6), Button.h};
			if(Reveal > .001f && Label.w > 0)
			{
				LucentUi::CTextScope Text(pTextRender, Label.w, Active, Hot, m_Interactive, Reveal * IconOpacity);
				SLabelProperties Props;
				Props.m_MaxWidth = Label.w;
				Props.m_EnableWidthCheck = false;
				Props.m_StopAtEnd = true;
				pUi->DoLabel(&Label, pLabel, 10, TEXTALIGN_ML, Props);
			}
			pUi->ClipDisable();
			if(m_Interactive && (!m_Layout.m_WorkspacesExpanded || Label.w <= 18))
				pUi->DoToolTip(&m_aWorkspaceButtons[i], &Button, pLabel);
		}
		LucentUi::Divider(m_Layout.m_WorkspaceDivider, .2f);

		{
			LucentUi::COpacityScope Categories(pTextRender, CategoryOpacity);
			pUi->ClipEnable(&m_Layout.m_Viewport);
			for(int i = 0; i < m_Count; ++i)
			{
				const auto Hit = m_Layout.Hitbox(i, m_Navigation.m_Scroll);
				const bool Hover = m_Interactive && Hit.h > 0 && pUi->MouseHovered(&Hit);
				const bool Focus = m_Interactive && m_Navigation.m_Focus == i;
				const float Hot = LucentUi::Highlight(&m_aButtons[i], Hover);
				const auto Row = m_Layout.VisualRow(i, m_Navigation.m_Scroll, Hot);
				const float Active = LucentUi::Highlight(&m_aButtons[i], Selected == i, LucentUi::EChannel::ACTIVE);
				if(Hit.h <= 0)
					continue;
				if(Focus)
					LucentUi::InputFrame(Row, &m_aButtons[i], true, true, Hover, true, IGraphics::CORNER_ALL);
				Row.Draw(LucentUi::Accent(LucentStyle::Style::Hover * Hot + LucentStyle::Style::Selected * Active), IGraphics::CORNER_ALL, LucentUi::RADIUS);
				if(Active > .001f)
				{
					const CUIRect Indicator = {Row.x, Row.y + 6, 2, Row.h - 12};
					LucentUi::GradientFill(Indicator, Active);
				}
				CUIRect Icon = {Row.x, Row.y, std::min(CollapsedWidth - 2 * Padding, Row.w), Row.h};
				{
					LucentUi::CTextScope Text(pTextRender, Icon.w, Active, Hot, m_Interactive);
					pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
					pUi->DoLabel(&Icon, ppIcons ? ppIcons[i] : s_apIcons[i], 13, TEXTALIGN_MC);
					pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
				const float Slide = LucentStyle::Motion::Slide(6.f, m_Navigation.m_Expansion.m_Value);
				CUIRect Label = {Row.x + CollapsedWidth - Padding + Slide, Row.y, std::max(0.f, Row.w - CollapsedWidth - Slide), Row.h};
				if(Label.w > 0 && m_Navigation.m_Expansion.m_Value > .001f)
				{
					LucentUi::CTextScope Text(pTextRender, Label.w, Active, Hot, m_Interactive, m_Navigation.m_Expansion.m_Value);
					SLabelProperties Props;
					Props.m_MaxWidth = Label.w;
					Props.m_StopAtEnd = true;
					Props.m_EnableWidthCheck = false;
					pUi->DoLabel(&Label, ppLabels[i], 12, TEXTALIGN_ML, Props);
				}
				if(m_Interactive && m_Navigation.m_Expansion.m_Value < .1f)
					pUi->DoToolTip(&m_aButtons[i], &Hit, ppLabels[i]);
			}
			if(m_Layout.MaxScroll() > 0 && m_Layout.m_Viewport.h > 0)
			{
				const float Height = std::min(m_Layout.m_Viewport.h, std::max(12.f, m_Layout.m_Viewport.h * m_Layout.m_Viewport.h / (m_Count * RowHeight)));
				const CUIRect Thumb = {m_Layout.m_Sidebar.x + m_Layout.m_Sidebar.w - 3,
					m_Layout.m_Viewport.y + (m_Layout.m_Viewport.h - Height) * m_Navigation.m_Scroll / m_Layout.MaxScroll(), 1, Height};
				LucentUi::GradientFill(Thumb, .5f);
			}
			pUi->ClipDisable();
		}

		// A persistent exit replaces the removed legacy menubar. It never scrolls
		// with categories and shares their hover, label reveal and tooltip styling.
		const auto &Back = m_Layout.m_Back;
		const float Hot = LucentUi::Highlight(&m_BackButton, m_Interactive && pUi->MouseHovered(&Back));
		CUIRect Icon = {Back.x, Back.y, std::min(CollapsedWidth - 2 * Padding, Back.w), Back.h};
		Back.Draw(LucentUi::Accent(LucentStyle::Style::Hover * Hot), IGraphics::CORNER_ALL, LucentUi::RADIUS);
		LucentUi::CTextScope Text(pTextRender, Back.w, 0, Hot, m_Interactive);
		pUi->ClipEnable(&Back);
		pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
		pUi->DoLabel(&Icon, FontIcon::CHEVRON_LEFT, 12, TEXTALIGN_MC);
		pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
		if(Back.w > CollapsedWidth)
		{
			const float Slide = LucentStyle::Motion::Slide(6.f, m_Navigation.m_Expansion.m_Value);
			CUIRect Label = {Back.x + CollapsedWidth - Padding + Slide, Back.y, std::max(0.f, Back.w - CollapsedWidth - Slide), Back.h};
			LucentUi::COpacityScope Opacity(pTextRender, m_Navigation.m_Expansion.m_Value);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Label.w);
			Props.m_StopAtEnd = true;
			Props.m_EnableWidthCheck = false;
			pUi->DoLabel(&Label, (Workspace == 1 ? LCNTLocalize("Back") : Localize("Back")), 12, TEXTALIGN_ML, Props);
		}
		pUi->ClipDisable();
		if(m_Interactive)
			pUi->DoToolTip(&m_BackButton, &Back, (Workspace == 1 ? LCNTLocalize("Back") : Localize("Back")));
	}
}
