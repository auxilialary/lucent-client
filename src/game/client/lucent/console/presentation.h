#ifndef GAME_CLIENT_LUCENT_CONSOLE_PRESENTATION_H
#define GAME_CLIENT_LUCENT_CONSOLE_PRESENTATION_H

#include "filters.h"
#include "layout.h"

#include <base/str.h>

#include <game/client/lucent/lucent_style.h>
#include <game/client/ui.h>
#include <game/localization.h>

namespace LucentConsole
{
	inline void RenderEmptyHistory(ITextRender *pText, EEmptyHistory State, const CUIRect &Bounds)
	{
		if(State == EEmptyHistory::NONE || Bounds.h < LucentStyle::Console::FilterFontSize)
			return;
		CTextCursor Text;
		Text.SetPosition(vec2(Bounds.x + LucentStyle::Console::FilterGap, Bounds.y + LucentStyle::Console::HistoryPadding));
		Text.m_FontSize = LucentStyle::Console::FilterFontSize;
		Text.m_LineWidth = std::max(1.f, Bounds.w - 2 * LucentStyle::Console::FilterGap);
		pText->TextColor(LucentStyle::Style::MutedText);
		pText->TextEx(&Text, State == EEmptyHistory::BACKLOG ? Localize("No messages yet") : Localize("No messages match the selected filters"));
		pText->TextColor(pText->DefaultTextColor());
	}
	inline ColorRGBA SourceColor(EConsoleLogGroup Group)
	{
		using namespace LucentStyle::Console;
		switch(Group)
		{
		case EConsoleLogGroup::GRAPHICS: return Graphics;
		case EConsoleLogGroup::NETWORK: return Network;
		case EConsoleLogGroup::CHAT: return Chat;
		case EConsoleLogGroup::SERVER: return Server;
		case EConsoleLogGroup::BROWSER: return Browser;
		case EConsoleLogGroup::CLIENT: return Client;
		case EConsoleLogGroup::ASSETS: return Assets;
		case EConsoleLogGroup::RECORD: return Record;
		case EConsoleLogGroup::EDITOR: return Editor;
		default: return Other;
		}
	}
	struct CMetadataTooltip
	{
		char m_aSource[32] = {};
		CUIRect m_Rect{};
		void Reset() { m_aSource[0] = 0; }
		void Render(CUi *pUi, ITextRender *pText, const CUIRect &Bounds) const
		{
			if(!m_aSource[0] || Bounds.h < LucentStyle::Console::FilterHeight)
				return;
			using namespace LucentStyle;
			const float Width = std::min(Bounds.w, pText->TextWidth(Console::MetadataSize, m_aSource) + 2 * Console::Gap);
			CUIRect Rect{std::clamp(m_Rect.x, Bounds.x, Bounds.x + Bounds.w - Width), std::clamp(m_Rect.y - Console::FilterHeight, Bounds.y, Bounds.y + Bounds.h - Console::FilterHeight), Width, Console::FilterHeight};
			Rect.Draw(Style::Surface, IGraphics::CORNER_ALL, Style::Radius);
			pText->TextColor(Style::Text);
			pUi->DoLabel(&Rect, m_aSource, Console::MetadataSize, TEXTALIGN_MC);
			pText->TextColor(pText->DefaultTextColor());
		}
	};

	inline void RenderMetadata(ITextRender *pText, CMetadataTooltip &Tooltip, const char *pTimestamp, const char *pSource, EConsoleLogGroup Group, int Severity, float TimestampWidth, float SourceWidth, float Y, float Height, int SearchMatch, vec2 Mouse)
	{
		if(!pTimestamp[0])
			return;
		using namespace LucentStyle;
		const auto Pill = [&](float X, float Width, const char *pLabel, ColorRGBA Background, bool Source) {
			CUIRect Rect{X, Y, Width, Height};
			Rect.Draw(Background, IGraphics::CORNER_ALL, Console::PillRadius);
			const float Indicator = Source ? Console::MetadataSize : 0;
			if(Source)
			{
				CUIRect Dot{X + Console::PillPadding, Y + (Height - Console::SeveritySize) / 2, Console::SeveritySize, Console::SeveritySize};
				Dot.Draw(Console::SeverityColors[CFilters::SeverityIndex(Severity)], IGraphics::CORNER_ALL, Console::PillRadius);
			}
			CTextCursor Text;
			Text.SetPosition(vec2(X + Console::PillPadding + Indicator, Y + (Height - Console::MetadataSize) / 2));
			Text.m_FontSize = Console::MetadataSize;
			Text.m_LineWidth = std::max(1.f, Width - 2 * Console::PillPadding - Indicator);
			Text.m_MaxLines = 1;
			Text.m_Flags |= TEXTFLAG_STOP_AT_END;
			// Metadata is deliberately excluded from visual selection, not from the logical range.
			Text.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
			pText->TextColor(SearchMatch == 2 ? Style::PrimaryText : Console::MetadataText);
			pText->TextEx(&Text, pLabel);
			if(SearchMatch)
			{
				CUIRect Underline{Rect.x + Console::PillPadding, Rect.y + Rect.h - Style::Hairline, Rect.w - 2 * Console::PillPadding, Style::Hairline};
				Underline.Draw(Style::Accent.WithAlpha(SearchMatch == 2 ? Style::ButtonUnderline : Style::BorderActive), 0, 0);
			}
			if(Source && Text.m_Truncated && Rect.Inside(Mouse))
			{
				str_copy(Tooltip.m_aSource, pSource);
				Tooltip.m_Rect = Rect;
			}
		};
		Pill(0, TimestampWidth, pTimestamp, Console::Timestamp, false);
		Pill(TimestampWidth + Console::Gap, SourceWidth, pSource, SourceColor(Group), true);
		pText->TextColor(pText->DefaultTextColor());
	}
}
#endif
