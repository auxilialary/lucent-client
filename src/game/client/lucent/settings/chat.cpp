#include "chat.h"

#include "general.h"

#include <base/time.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/lucent/game/chat_policy.h>
#include <game/client/lucent/localization.h>
#include <game/client/lucent/ui.h>
#include <game/client/lucent/ui/controls.h>
#include <game/client/ui_scrollregion.h>

namespace LucentSettings
{
	namespace
	{
		struct CChatView : SPopupMenuId
		{
			CUi *m_pUi = nullptr;
			ITextRender *m_pText = nullptr;
			const char *m_pTitle = "";
			LucentChat::ECategory m_Category = LucentChat::ECategory::PLAYER;
			CScrollRegion m_PageScroll, m_PopupScroll;
			double m_PreviewStart = -1;
		};
		CChatView &State()
		{
			static CChatView s_State;
			return s_State;
		}
		void Label(CUi *pUi, ITextRender *pText, CUIRect Rect, const char *pTextValue, float Size, ColorRGBA Color)
		{
			const auto Previous = pText->GetTextColor();
			pText->TextColor(Color.WithMultipliedAlpha(CUIRect::DrawOpacity()));
			SLabelProperties Props;
			Props.m_MaxWidth = Rect.w;
			Props.m_StopAtEnd = false;
			pUi->DoLabel(&Rect, pTextValue, Size, TEXTALIGN_ML, Props);
			pText->TextColor(Previous);
		}
		CUi::EPopupMenuFunctionResult MessagePopup(void *pContext, CUIRect View, bool Active)
		{
			auto &S = *static_cast<CChatView *>(pContext);
			LucentUi::CScope Style(S.m_pUi);
			LucentUi::CInteractionScope Input(S.m_pUi, Active);
			View.Margin(6, &View);
			CUIRect Header;
			View.HSplitTop(42, &Header, &View);
			Label(S.m_pUi, S.m_pText, Header, LCNTLocalize(S.m_pTitle), 12, LucentStyle::Style::PrimaryText);
			CScrollRegionParams Params;
			Params.m_ScrollbarThickness = 6;
			Params.m_ScrollUnit = 32;
			S.m_PopupScroll.Begin(&View, &Params);
			const auto Row = [&](const char *pText, bool Muted = false) {
				const float Height = std::max(24.f, S.m_pText->TextBoundingBox(11, pText, -1, std::max(1.f, View.w)).m_H + 10);
				CUIRect Rect;
				View.HSplitTop(Height, &Rect, &View);
				S.m_PopupScroll.AddRect(Rect);
				Label(S.m_pUi, S.m_pText, Rect, pText, 11, Muted ? LucentStyle::Style::MutedText : LucentStyle::Style::Text);
			};
			using C = LucentChat::ECategory;
			if(S.m_Category == C::PLAYER)
			{
				Row(LCNTLocalize("Messages mentioning your player or dummy name."));
				Row(LCNTLocalize("Incoming private messages."));
				Row(LCNTLocalize("This checkbox controls the notification card only."), true);
			}
			else if(S.m_Category == C::SYSTEM)
			{
				Row(LCNTLocalize("All other server and client service messages."));
				Row(LCNTLocalize("Excludes welcome messages, server reminders and player joins, leaves and renames."), true);
				Row(LCNTLocalize("The exact messages depend on the server."), true);
			}
			else
			{
				Row(LCNTLocalize("Messages containing these phrases (case-insensitive):"), true);
				for(const auto &Rule : LucentChat::Rules)
					if(Rule.m_Category == S.m_Category)
					{
						const std::string Pattern = "... " + std::string(Rule.m_Fragment) + " ...";
						Row(Pattern.c_str());
					}
				Row(LCNTLocalize("Player chat is never hidden by these filters."), true);
			}
			S.m_PopupScroll.End();
			return CUi::POPUP_KEEP_OPEN;
		}
		void OpenMessagePopup(CUi *pUi, ITextRender *pText, const char *pTitle, LucentChat::ECategory Category)
		{
			auto &S = State();
			S.m_pUi = pUi;
			S.m_pText = pText;
			S.m_pTitle = pTitle;
			S.m_Category = Category;
			S.m_PopupScroll.Reset();
			SPopupMenuProperties Props;
			Props.m_ClampToScreen = true;
			Props.m_BackgroundColor = LucentStyle::Style::Surface.WithAlpha(1);
			Props.m_BorderColor = LucentUi::Accent(LucentStyle::Style::BorderActive);
			pUi->DoPopupMenu(&S, pUi->MouseX(), pUi->MouseY() + 8, std::min(380.f, pUi->Screen()->w - 16), std::min(330.f, pUi->Screen()->h - 16), &S, MessagePopup, Props);
		}
	}

	void LeaveChat(CUi *pUi)
	{
		auto &S = State();
		pUi->ClosePopupMenu(&S);
		S.m_PreviewStart = -1;
	}

	void RenderChat(CGameClient *pGame, CUi *pUi, ITextRender *pText, CUIRect View)
	{
		auto &S = State();
		const double Now = time_get() / (double)time_freq();
		if(S.m_PreviewStart < 0)
		{
			S.m_PreviewStart = Now;
			pGame->m_LucentChat.ResetPreview();
		}
		CUIRect Title;
		View.HSplitTop(22, &Title, &View);
		Label(pUi, pText, SettingsContentRegion(Title), LCNTLocalize("Chat"), 15, LucentStyle::Style::PrimaryText);
		View.HSplitTop(8, nullptr, &View);
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollUnit = 54;
		S.m_PageScroll.Begin(&View, &Params);
		const auto Columns = CGeneralLayout::Calculate(View);
		CUIRect Left = CGeneralLayout::ContentRegion(Columns.m_Left);
		const auto Row = [&](float Height) {
			CUIRect Rect;
			Left.HSplitTop(Height, &Rect, &Left);
			Left.HSplitTop(CGeneralLayout::RowGap, nullptr, &Left);
			S.m_PageScroll.AddRect(Rect);
			return Rect;
		};
		const auto Heading = [&](const char *pTextValue) {
			Label(pUi, pText, Row(24), LCNTLocalize(pTextValue), 11, LucentStyle::Style::MutedText);
		};
		const auto Check = [&](int *pValue, const char *pName, LucentChat::ECategory Category) {
			const float TextWidth = std::max(1.f, Left.w - LucentStyle::Checkbox::Size - LucentStyle::Style::LabelGap);
			const float Height = std::max(CGeneralLayout::RowHeight, pText->TextBoundingBox(11, LCNTLocalize(pName), -1, TextWidth).m_H + 8);
			const CUIRect Rect = Row(Height);
			LucentUi::CInteractionScope Input(pUi, !S.m_PageScroll.RectClipped(Rect));
			if(LucentUi::CheckboxContextRequested(pUi, Rect))
				OpenMessagePopup(pUi, pText, pName, Category);
			if(GeneralCheckbox(pUi, pText, pValue, LCNTLocalize(pName), *pValue != 0, &Rect, pUi->Enabled()))
				*pValue ^= 1;
		};
		using C = LucentChat::ECategory;
		Heading("Mentions");
		Check(&g_Config.m_ClLucentChatMentions, "Mention notifications", C::PLAYER);
		Left.HSplitTop(14, nullptr, &Left);
		Heading("Visible messages");
		Check(&g_Config.m_ClLucentChatSystem, "Other system messages", C::SYSTEM);
		Check(&g_Config.m_ClLucentChatWelcome, "Welcome and help messages", C::WELCOME);
		Check(&g_Config.m_ClLucentChatRace, "Server reminders (Good luck in your race)", C::RACE);
		Check(&g_Config.m_ClLucentChatConnections, "Player joins, leaves and renames", C::CONNECTION);
		Left.HSplitTop(12, nullptr, &Left);
		for(const char *pHint : {"Hidden messages are also muted.", "Right-click a checkbox to see affected messages."})
		{
			const float Height = std::max(24.f, pText->TextBoundingBox(10, LCNTLocalize(pHint), -1, Left.w).m_H + 8);
			Label(pUi, pText, Row(Height), LCNTLocalize(pHint), 10, LucentStyle::Style::MutedText);
		}
		CUIRect Preview = CGeneralLayout::ContentRegion(Columns.m_Right);
		if(!Columns.m_TwoColumns)
			Preview.y = Left.y + 18;
		CUIRect Caption;
		Preview.HSplitTop(24, &Caption, &Preview);
		Label(pUi, pText, Caption, LCNTLocalize("Preview"), 12, LucentStyle::Style::PrimaryText);
		Preview.h = 360;
		S.m_PageScroll.AddRect(Preview);
		pGame->m_LucentChat.RenderPreview(Preview, Now - S.m_PreviewStart);
		S.m_PageScroll.End();
	}
}
