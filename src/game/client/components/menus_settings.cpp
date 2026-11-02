/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/dbg.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/components/camera.h>
#include <game/client/components/menu_background.h>
#include <game/client/gameclient.h>
#include <game/client/lucent/gameview/presentation.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/settings/general.h>
#include <game/client/lucent/settings/player.h>
#include <game/client/lucent/settings/presentation.h>
#include <game/client/lucent/settings/skins.h>
#include <game/client/lucent/settings/sound.h>
#include <game/client/lucent/settings/workspace.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/panel.h>
#include <game/client/lucent/ui/scopes.h>
#include <game/client/lucent/ui/selection_corners.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

namespace
{
	void SettingsText(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, float Size = 11, ColorRGBA Color = LucentStyle::Style::Text, int Align = TEXTALIGN_ML, bool Wrap = false)
	{
		const auto Previous = pTextRender->GetTextColor();
		pTextRender->TextColor(Color);
		SLabelProperties Props;
		Props.m_MaxWidth = std::max(0.f, Rect.w);
		Props.m_StopAtEnd = !Wrap;
		pUi->DoLabel(&Rect, pText, Size, Align, Props);
		pTextRender->TextColor(Previous);
	}
}

void CMenus::SetNeedSendInfo()
{
	if(m_Dummy)
		m_NeedSendDummyinfo = true;
	else
		m_NeedSendinfo = true;
}

void CMenus::RenderSettings(CUIRect MainView)
{
	LucentSettings::NormalizeWorkspace(g_Config.m_ClLucentSettingsWorkspace, g_Config.m_ClLucentSettingsPage, g_Config.m_UiSettingsPage);
	MainView = *Ui()->Screen();
	m_LucentSettings.WorkspaceInput(Ui(), MainView, g_Config.m_ClLucentSettingsWorkspace, GameClient()->m_KeyBinder.IsActive());
	static_assert(SETTINGS_LENGTH == LucentSettings::CategoryCount);
	const auto &Clock = LucentStyle::Motion::Clock();
	m_LucentWorkspacePresentation.SelectMode(g_Config.m_ClLucentSettingsWorkspace == LucentSettings::WorkspaceLucent);
	m_LucentWorkspacePresentation.UpdateMode(Clock.m_Now, Clock.m_Animate);
	const bool Lucent = m_LucentWorkspacePresentation.m_ShownAdvanced;
	const bool WorkspaceInteractive = m_LucentWorkspacePresentation.ContentInteractive();
	const float WorkspaceOpacity = m_LucentWorkspacePresentation.m_ModeReveal.m_Value;
	int &Selected = Lucent ? g_Config.m_ClLucentSettingsPage : g_Config.m_UiSettingsPage;
	const auto Layout = m_LucentSettings.Begin(Ui(), MainView, Selected, GameClient()->m_KeyBinder.IsActive(), Lucent ? LucentSettings::PageCount : SETTINGS_LENGTH, !WorkspaceInteractive);
	CUIRect RestartBar;
	MainView = Layout.m_Content;
	MainView.Margin(20.0f, &MainView);

	if(Lucent || g_Config.m_UiSettingsPage != SETTINGS_GENERAL)
		LucentSettings::LeaveGeneral(Ui());

	if(Lucent || g_Config.m_UiSettingsPage != SETTINGS_SOUND)
		LucentSettings::LeaveSound(Ui());

	if(Lucent || g_Config.m_UiSettingsPage != SETTINGS_PLAYER)
		LucentSettings::LeavePlayer(Ui());

	if(Lucent || g_Config.m_UiSettingsPage != SETTINGS_TEE)
		LucentSettings::LeaveSkinManager(Ui());

	const bool NeedRestart = m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartUpdate;
	if(NeedRestart)
	{
		MainView.HSplitBottom(20.0f, &MainView, &RestartBar);
		MainView.HSplitBottom(10.0f, &MainView, nullptr);
	}

	const char *apTabs[SETTINGS_LENGTH] = {
		Localize("Language"),
		Localize("General"),
		Localize("Player"),
		Client()->IsSixup() ? "Tee 0.7" : Localize("Tee"),
		Localize("Appearance"),
		Localize("Controls"),
		Localize("Graphics"),
		Localize("Sound"),
		Localize("DDNet"),
		Localize("Assets"),
		Localize("Credits")};
	{
		LucentUi::CInteractionScope ContentInput(Ui(), WorkspaceInteractive && !m_LucentSettings.BlocksContent(Ui()));
		LucentUi::COpacityScope WorkspaceFade(TextRender(), WorkspaceOpacity);
		if(Lucent)
		{
			LucentUi::CScope Style(Ui());
			RenderLucentPresentationSettings(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_LANGUAGE)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
			RenderLanguageSettings(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_GENERAL)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
			RenderSettingsGeneral(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_PLAYER)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
			RenderSettingsPlayer(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_TEE)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_TEE);
			RenderSettingsTee(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_APPEARANCE)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_APPEARANCE);
			RenderSettingsAppearance(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_CONTROLS)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_CONTROLS);
			m_MenusSettingsControls.Render(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_GRAPHICS)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GRAPHICS);
			RenderSettingsGraphics(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_SOUND)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_SOUND);
			RenderSettingsSound(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_DDNET)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_DDNET);
			RenderSettingsDDNet(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_ASSETS)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_ASSETS);
			RenderSettingsAssets(MainView);
		}
		else if(g_Config.m_UiSettingsPage == SETTINGS_CREDITS)
		{
			GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_CREDITS);
			RenderSettingsCredits(MainView);
		}
		else
		{
			dbg_assert_failed("ui_settings_page invalid");
		}

		if(NeedRestart)
			RenderSettingsRestart(RestartBar, Lucent);
	}
	const char *apLucentTabs[LucentSettings::PageCount];
	for(int i = 0; i < LucentSettings::PageCount; ++i)
		apLucentTabs[i] = LCNTLocalize(LucentSettings::Pages[i].m_pLabel);
	const char *apLucentIcons[] = {FontIcon::EYE, FontIcon::BRUSH, FontIcon::CAMERA};
	m_LucentSettings.Render(Ui(), TextRender(), Selected, Lucent ? apLucentTabs : apTabs, Lucent ? apLucentIcons : nullptr, g_Config.m_ClLucentSettingsWorkspace, WorkspaceOpacity);
	if(m_LucentSettings.BackRequested())
	{
		LucentSettings::LeaveGeneral(Ui());
		LucentSettings::LeaveSound(Ui());
		LucentSettings::LeavePlayer(Ui());
		LucentSettings::LeaveSkinManager(Ui());
		m_LucentSettings.Reset();
		if(Client()->State() == IClient::STATE_OFFLINE)
			SetShowStart(true);
		else
			m_GamePage = PAGE_GAME;
	}
}

bool CMenus::RenderHslaScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha, float DarkestLight, bool Lucent)
{
	if(Lucent && pRect->w < 80)
		return false;
	if(Lucent && !Ui()->Enabled())
	{
		for(int i = 0; i < 4; ++i)
			if(Ui()->ActiveItem() == &reinterpret_cast<char *>(pColor)[i])
				Ui()->SetActiveItem(nullptr);
	}

	const unsigned PrevPackedColor = *pColor;
	ColorHSLA Color(*pColor, Alpha);
	const ColorHSLA OriginalColor = Color;
	const char *apLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};
	const float SizePerEntry = 20.0f;
	const float MarginPerEntry = 5.0f;
	const float PreviewMargin = 2.5f;
	const float PreviewHeight = 40.0f + 2 * PreviewMargin;
	const float OffY = (SizePerEntry + MarginPerEntry) * (3 + (Alpha ? 1 : 0)) - PreviewHeight;

	CUIRect Preview;
	pRect->VSplitLeft(PreviewHeight, &Preview, pRect);
	Preview.HSplitTop(OffY / 2.0f, nullptr, &Preview);
	Preview.HSplitTop(PreviewHeight, &Preview, nullptr);

	Preview.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);
	Preview.Margin(PreviewMargin, &Preview);
	Preview.Draw(color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight)), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);

	auto &&RenderHueRect = [&](CUIRect *pColorRect) {
		float CurXOff = pColorRect->x;
		const float SizeColor = pColorRect->w / 6;

		// red to yellow
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(1, 0, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		// yellow to green
		CurXOff += SizeColor;
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(1, 1, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// green to turquoise
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 1, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// turquoise to blue
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(0, 1, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// blue to purple
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(0, 0, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// purple to red
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 0, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	};

	auto &&RenderSaturationRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.s = 0.0f;
		RightColor.s = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderLightingRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.l = DarkestLight;
		RightColor.l = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderAlphaRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColorFull) {
		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(0.0f));
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(1.0f));

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	for(int i = 0; i < 3 + Alpha; i++)
	{
		CUIRect Button, Label;
		pRect->HSplitTop(SizePerEntry, &Button, pRect);
		pRect->HSplitTop(MarginPerEntry, nullptr, pRect);
		Button.VSplitLeft(140.0f, &Label, &Button);
		Label.VMargin(10.0f, &Label);

		Button.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

		CUIRect Rail;
		Button.Margin(2.0f, &Rail);

		char aBuf[32];

		// Hue
		if(i == 0)
			str_format(aBuf, sizeof(aBuf), "%s: %.1f° (%03d)", apLabels[i], Color[i] * 360.0f, round_to_int(Color[i] * 255.0f));
		// Lht
		else if(i == 2)
		{
			// handle internal light clamping, see `UnclampLighting`
			float Lht = DarkestLight + Color[i] * (1.0f - DarkestLight);
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Lht * 100.0f, round_to_int(Color[i] * 255.0f));
		}
		// Sat and Alpha
		else
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Color[i] * 100.0f, round_to_int(Color[i] * 255.0f));
		Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML);

		ColorRGBA HandleColor;
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();
		if(i == 0)
		{
			RenderHueRect(&Rail);
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f));
		}
		else if(i == 1)
		{
			RenderSaturationRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f));
		}
		else if(i == 2)
		{
			RenderLightingRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight));
		}
		else if(i == 3)
		{
			RenderAlphaRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight)));
			HandleColor = color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight));
		}
		Graphics()->TrianglesEnd();

		Color[i] = Ui()->DoScrollbarH(&((char *)pColor)[i], &Button, Color[i], &HandleColor, Lucent);
	}

	if(OriginalColor != Color)
	{
		*pColor = Color.Pack(Alpha);
	}
	return PrevPackedColor != *pColor;
}

void CMenus::RenderLucentPresentationSettings(CUIRect View)
{
	const int PageIndex = LucentSettings::PageIndex(g_Config.m_ClLucentSettingsPage);
	const int Page = LucentSettings::Pages[PageIndex].m_Id;
	static int s_Weapon = 0;
	static int s_aWeapons[6], s_Reset;
	static CScrollRegion s_aScroll[LucentSettings::PageCount];
	using Layout = LucentSettings::CGeneralLayout;
	CUIRect Title, Row;
	View.HSplitTop(22.f, &Title, &View);
	SettingsText(Ui(), TextRender(), LucentSettings::SettingsContentRegion(Title), LCNTLocalize(LucentSettings::Pages[PageIndex].m_pLabel), 15, LucentStyle::Style::PrimaryText);
	View.HSplitTop(8.f, nullptr, &View);
	auto &Scroll = s_aScroll[PageIndex];
	CScrollRegionParams Params;
	Params.m_ScrollbarThickness = 8;
	Params.m_ScrollbarMargin = 1;
	Params.m_ScrollUnit = 54;
	Params.m_ForceShowScrollbar = true;
	Scroll.Begin(&View, &Params);
	const auto Columns = Layout::Calculate(View);
	CUIRect Column = Layout::ContentRegion(Columns.m_Left);
	bool FirstSection = true;
	bool Paint = true;
	auto NextRow = [&](float Height = Layout::RowHeight) {
		CUIRect Rect;
		Column.HSplitTop(Height, &Rect, &Column);
		Column.HSplitTop(Layout::RowGap, nullptr, &Column);
		if(Paint)
			Scroll.AddRect(Rect);
		return Rect;
	};
	auto Section = [&](const char *pTitle) {
		if(!FirstSection)
			Column.HSplitTop(Layout::SectionGap, nullptr, &Column);
		FirstSection = false;
		const float Height = std::max(18.f, TextRender()->TextBoundingBox(11, LCNTLocalize(pTitle), -1, std::max(1.f, Column.w)).m_H + 2);
		const CUIRect Heading = NextRow(Height);
		if(Paint)
			SettingsText(Ui(), TextRender(), Heading, LCNTLocalize(pTitle), 11, LucentStyle::Style::MutedText);
		Column.HSplitTop(Layout::SectionHeadingGap - Layout::RowGap, nullptr, &Column);
	};
	auto RightColumn = [&]() {
		const float PreviousBottom = Column.y;
		Column = Layout::ContentRegion(Columns.m_Right);
		if(!Columns.m_TwoColumns)
			Column.y = PreviousBottom + Layout::SectionGap;
		FirstSection = true;
	};
	auto Slider = [&](int *pValue, const char *pName, int Min, int Max, const char *pSuffix = "") {
		CUIRect Caption, Value, Rail;
		Layout::SliderRegions({Column.x, Column.y, Column.w, Layout::SliderHeight}, Caption, Value, Rail);
		const float CaptionHeight = TextRender()->TextBoundingBox(11, LCNTLocalize(pName), -1, std::max(1.f, Caption.w)).m_H;
		const CUIRect Rect = NextRow(Layout::SliderRowHeight(CaptionHeight));
		Layout::SliderRegions(Rect, Caption, Value, Rail);
		if(!Paint)
			return false;
		SettingsText(Ui(), TextRender(), Caption, LCNTLocalize(pName), 11, LucentStyle::Style::Text, TEXTALIGN_ML, true);
		LucentUi::CInteractionScope Input(Ui(), !Scroll.RectClipped(Rect));
		return Ui()->DoScrollbarOption(pValue, pValue, &Rail, LCNTLocalize(pName), Min, Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_COMPACT | CUi::SCROLLBAR_OPTION_LUCENT, pSuffix, &Value);
	};
	auto Check = [&](int *pValue, const char *pName, bool Checked) {
		const float Width = std::max(1.f, Column.w - LucentStyle::Checkbox::Size - LucentStyle::Style::LabelGap);
		const float Height = std::max(Layout::RowHeight, TextRender()->TextBoundingBox(11, LCNTLocalize(pName), -1, Width).m_H + 8);
		CUIRect Rect = NextRow(Height);
		if(!Paint)
			return false;
		LucentUi::CInteractionScope Input(Ui(), !Scroll.RectClipped(Rect));
		return LucentSettings::GeneralCheckbox(Ui(), TextRender(), pValue, LCNTLocalize(pName), Checked, &Rect, Ui()->Enabled());
	};
	auto Label = [&](const char *pText) {
		const CUIRect Rect = NextRow(24);
		if(Paint)
			SettingsText(Ui(), TextRender(), Rect, pText, 10, LucentStyle::Style::MutedText);
	};
	auto Preview = [&](int Weapon, bool Cursor) {
		CUIRect Header = NextRow(26), Caption, Detail;
		Header.VSplitMid(&Caption, &Detail);
		CUIRect WheelHint;
		if(Cursor)
			WheelHint = NextRow(std::max(24.f, TextRender()->TextBoundingBox(10, LCNTLocalize("Use the mouse wheel to change cursor size"), -1, std::max(1.f, Column.w)).m_H + 4));
		CUIRect Rect = NextRow(LucentSettings::CPreviewLayout::Height);
		if(!Paint)
		{
			const bool Hover = Cursor && Ui()->MouseHovered(&Rect) && !Scroll.RectClipped(Rect) && !Ui()->IsPopupOpen() && !Ui()->ActiveItem() && !CLineInput::GetActiveInput();
			m_LucentCursorPreviewActive = Hover && GameClient()->m_GameSkin.m_aSpriteWeaponCursors[WEAPON_GUN].IsValid();
			if(Hover)
			{
				const bool Up = Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP);
				const bool Down = Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN);
				Scroll.StopScrolling();
				if(Up != Down)
				{
					char aCommand[64];
					str_format(aCommand, sizeof(aCommand), "lucent_cursor_size %d", LucentSettings::CPreviewLayout::WheelValue(g_Config.m_LucentCursorSize, Up ? 1 : -1));
					Console()->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED);
				}
			}
			return;
		}
		SettingsText(Ui(), TextRender(), Caption, LCNTLocalize("Preview"), 12, LucentStyle::Style::PrimaryText);
		if(!Cursor)
			SettingsText(Ui(), TextRender(), Detail, LCNTLocalize("Current size"), 10, LucentStyle::Style::MutedText, TEXTALIGN_MR);
		if(Cursor)
			SettingsText(Ui(), TextRender(), WheelHint, LCNTLocalize("Use the mouse wheel to change cursor size"), 10, LucentStyle::Style::MutedText, TEXTALIGN_ML, true);
		Rect.Draw(LucentStyle::Style::MenuBackground, IGraphics::CORNER_ALL, LucentStyle::Style::Radius);
		static LucentStyle::Motion::CTransition s_CursorPreviewCorners;
		const auto DrawBorder = [&]() {
			if(Cursor)
				LucentUi::SelectionCorners(Ui(), Rect, s_CursorPreviewCorners, true);
		};
		Ui()->ClipEnable(&Rect);
		float ScaleX, ScaleY;
		const auto *pSprite = Cursor ? g_pData->m_Weapons.m_aId[Weapon].m_pSpriteCursor : g_pData->m_Weapons.m_aId[Weapon].m_pSpriteBody;
		Graphics()->GetSpriteScale(pSprite, ScaleX, ScaleY);
		const float Size = Cursor ? 64.f * LucentGameView::CursorScale(g_Config.m_LucentCursorSize) : g_pData->m_Weapons.m_aId[Weapon].m_VisualSize;
		const vec2 Offset = Cursor ? vec2(0, 0) : LucentGameView::WeaponOffset(Weapon);
		if(Cursor && m_LucentCursorPreviewActive)
		{
			if(m_LucentCursorQuad < 0)
			{
				m_LucentCursorQuad = Graphics()->CreateQuadContainer(false);
				Graphics()->SetColor(1, 1, 1, 1);
				Graphics()->QuadsSetSubset(0, 0, 1, 1);
				Graphics()->QuadContainerAddSprite(m_LucentCursorQuad, 64.f * ScaleX, 64.f * ScaleY);
				Graphics()->QuadContainerUpload(m_LucentCursorQuad);
			}
			Graphics()->SetColor(1, 1, 1, CUIRect::DrawOpacity());
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponCursors[WEAPON_GUN]);
			const float Scale = LucentGameView::CursorScale(g_Config.m_LucentCursorSize);
			Graphics()->RenderQuadContainerAsSprite(m_LucentCursorQuad, 0, Ui()->MouseX(), Ui()->MouseY(), Scale, Scale);
			Ui()->ClipDisable();
			DrawBorder();
			return;
		}
		const auto Geometry = LucentSettings::CPreviewLayout::Calculate(Rect, {Size * ScaleX, Size * ScaleY}, Offset, Cursor);
		if(!Cursor)
		{
			const CUIRect Actual = Geometry.m_Actual;
			const vec2 Center(Actual.x + Actual.w / 2, Actual.y + Actual.h / 2);
			LucentUi::Divider({Actual.x + 12, Center.y, std::max(0.f, Actual.w - 24), 1}, .12f);
		}
		char aHint[128];
		if(Geometry.m_Fit < .999f)
		{
			str_format(aHint, sizeof(aHint), LCNTLocalize("Preview scale: %.0f%%"), Geometry.m_Fit * 100);
			SettingsText(Ui(), TextRender(), Geometry.m_Hint, aHint, 9, LucentStyle::Style::MutedText);
		}
		if(Geometry.m_Magnification > 1)
		{
			LucentUi::GradientFill(Geometry.m_Loupe, LucentStyle::Style::Hover);
			str_format(aHint, sizeof(aHint), LCNTLocalize("Magnified x%.1f"), Geometry.m_Magnification);
			SettingsText(Ui(), TextRender(), Geometry.m_LoupeLabel, aHint, 9, LucentStyle::Style::MutedText, TEXTALIGN_MC);
		}
		Graphics()->TextureSet(Cursor ? GameClient()->m_GameSkin.m_aSpriteWeaponCursors[Weapon] : GameClient()->m_GameSkin.m_aSpriteWeapons[Weapon]);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, CUIRect::DrawOpacity());
		const auto DrawSprite = [&](CUIRect Bounds) {
			IGraphics::CQuadItem Quad(Bounds.x + Bounds.w / 2, Bounds.y + Bounds.h / 2, Bounds.w, Bounds.h);
			Graphics()->QuadsDraw(&Quad, 1);
		};
		DrawSprite(Geometry.m_Sprite);
		if(Geometry.m_Magnification > 1)
			DrawSprite(Geometry.m_LoupeSprite);
		Graphics()->QuadsEnd();
		Ui()->ClipDisable();
		DrawBorder();
	};
	if(Page == LucentSettings::PAGE_CURSOR)
	{
		auto ComposeCursor = [&]() {
			Section("Cursor");
			Slider(&g_Config.m_LucentCursorSize, "Cursor size", 5, 500, "%");
			CUIRect Range = NextRow(20), Low, High;
			Range.VSplitMid(&Low, &High);
			if(Paint)
				SettingsText(Ui(), TextRender(), Low, "5%", 9, LucentStyle::Style::MutedText);
			if(Paint)
				SettingsText(Ui(), TextRender(), High, "500%", 9, LucentStyle::Style::MutedText, TEXTALIGN_MR);
			Row = NextRow(28);
			Row.w = std::min(100.f, Row.w);
			if(Paint && LucentUi::SceneButton(Ui(), TextRender(), &s_Reset, LCNTLocalize("Reset"), Row))
				g_Config.m_LucentCursorSize = 100;
			RightColumn();
			Preview(WEAPON_GUN, true);
		};
		const CUIRect Start = Column;
		Paint = false;
		ComposeCursor();
		Column = Start;
		FirstSection = true;
		Paint = true;
		ComposeCursor();
	}
	else if(Page == LucentSettings::PAGE_WEAPONS)
	{
		const int WeaponColumns = Column.w < 400 ? 3 : 6;
		const float ChoicesHeight = WeaponColumns == 3 ? 60.f : 30.f;
		Section("Visual offsets");
		const char *apWeapons[] = {"Hammer", "Gun", "Shotgun", "Grenade", "Laser", "Ninja"};
		CUIRect WeaponChoices = NextRow(ChoicesHeight);
		const float Width = WeaponChoices.w / WeaponColumns;
		for(int i = 0; i < 6; ++i)
		{
			CUIRect Button;
			Button = {WeaponChoices.x + (i % WeaponColumns) * Width, WeaponChoices.y + (i / WeaponColumns) * 30, Width, 28};
			Button.Margin(2, &Button);
			if(LucentUi::Button(Ui(), TextRender(), &s_aWeapons[i], LCNTLocalize(apWeapons[i]), Button, s_Weapon == i))
				s_Weapon = i;
		}
		int *apX[] = {&g_Config.m_LucentWeaponHammerX, &g_Config.m_LucentWeaponGunX, &g_Config.m_LucentWeaponShotgunX, &g_Config.m_LucentWeaponGrenadeX, &g_Config.m_LucentWeaponLaserX, &g_Config.m_LucentWeaponNinjaX};
		int *apY[] = {&g_Config.m_LucentWeaponHammerY, &g_Config.m_LucentWeaponGunY, &g_Config.m_LucentWeaponShotgunY, &g_Config.m_LucentWeaponGrenadeY, &g_Config.m_LucentWeaponLaserY, &g_Config.m_LucentWeaponNinjaY};
		Slider(apX[s_Weapon], "Visual offset X", -100, 100);
		Slider(apY[s_Weapon], "Visual offset Y", -100, 100);
		Row = NextRow(28);
		Row.w = std::min(132.f, Row.w);
		if(LucentUi::SceneButton(Ui(), TextRender(), &s_Reset, LCNTLocalize("Reset weapon"), Row))
			*apX[s_Weapon] = *apY[s_Weapon] = 0;
		Label(LCNTLocalize("Local weapon artwork only. Projectiles keep their actual positions."));
		RightColumn();
		Preview(s_Weapon, false);
	}
	else if(Page == LucentSettings::PAGE_CAMERA)
	{
		Section("Zoom");
		if(Slider(&g_Config.m_ClDefaultZoom, "Default zoom", 0, 20))
			GameClient()->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);
		Slider(&g_Config.m_ClSmoothZoomTime, "Zoom smoothness (ms)", 0, 5000);

		if(Check(&g_Config.m_ClLimitMaxZoomLevel, "Limit maximum zoom", g_Config.m_ClLimitMaxZoomLevel))
			g_Config.m_ClLimitMaxZoomLevel ^= 1;
		Section("Cursor movement");
		Slider(&g_Config.m_ClMouseDeadzone, "Camera deadzone", 0, 3000);
		Slider(&g_Config.m_ClMouseFollowfactor, "Camera follow factor", 0, 200);
		Slider(&g_Config.m_ClMouseMinDistance, "Minimum cursor distance", 0, 5000);
		Slider(&g_Config.m_ClMouseMaxDistance, "Maximum cursor distance", 0, 5000);
		RightColumn();
		Section("Dynamic Camera");
		const bool Dynamic = g_Config.m_ClDyncam || g_Config.m_ClMouseFollowfactor > 0;
		if(Check(&g_Config.m_ClDyncam, "Dynamic Camera", Dynamic))
		{
			g_Config.m_ClDyncam = !Dynamic;
			if(Dynamic)
				g_Config.m_ClMouseFollowfactor = 0;
		}
		Slider(&g_Config.m_ClDyncamDeadzone, "Dynamic camera deadzone", 1, 1300);
		Slider(&g_Config.m_ClDyncamFollowFactor, "Dynamic camera follow factor", 0, 200);
		Slider(&g_Config.m_ClDyncamMinDistance, "Dynamic minimum distance", 0, 2000);
		Slider(&g_Config.m_ClDyncamMaxDistance, "Dynamic maximum distance", 0, 2000);
		Slider(&g_Config.m_ClDyncamMousesens, "Dynamic camera mouse sensitivity", 0, 100000);
		Slider(&g_Config.m_ClDyncamSmoothness, "Dynamic camera smoothness", 0, 100);
		Slider(&g_Config.m_ClDyncamStabilizing, "Dynamic camera stabilizing", 0, 100);
		Section("Spectator camera");
		Slider(&g_Config.m_ClSmoothSpectatingTime, "Spectating smoothness (ms)", 0, 5000);
		if(Check(&g_Config.m_ClSpecAutoSync, "Synchronize spectator camera on zoom reset", g_Config.m_ClSpecAutoSync))
			g_Config.m_ClSpecAutoSync ^= 1;
	}
	Scroll.End();
}

void CMenus::RenderSettingsRestart(CUIRect RestartBar, bool Lucent)
{
	const auto Translate = [Lucent](const char *pText) { return Lucent ? LCNTLocalize(pText) : Localize(pText); };
	CUIRect RestartWarning, RestartButton;
	RestartBar.VSplitRight(125.0f, &RestartWarning, &RestartButton);
	RestartWarning.VSplitRight(10.0f, &RestartWarning, nullptr);
	if(m_NeedRestartUpdate)
	{
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		Ui()->DoLabel(&RestartWarning, Translate("Lucent Client needs to be restarted to complete update!"), 14.0f, TEXTALIGN_ML);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		Ui()->DoLabel(&RestartWarning, Translate("You must restart the game for all settings to take effect."), 14.0f, TEXTALIGN_ML);
	}

	static CButtonContainer s_RestartButton;
	if(Lucent ? LucentUi::Button(Ui(), TextRender(), &s_RestartButton, Translate("Restart"), RestartButton) : DoButton_Menu(&s_RestartButton, Translate("Restart"), 0, &RestartButton))
	{
		if(Client()->State() == IClient::STATE_ONLINE || GameClient()->Editor()->HasUnsavedData())
		{
			m_Popup = POPUP_RESTART;
		}
		else
		{
			Client()->Restart();
		}
	}
}
