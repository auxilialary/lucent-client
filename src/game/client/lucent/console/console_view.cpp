#include <base/math.h>
#include <base/str.h>

#include <engine/font_icons.h>

#include <game/client/components/console.h>
#include <game/client/lucent/ui/controls.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

void CGameConsole::RenderLogMetadata(const CInstance::CBacklogEntry *pEntry, float Y, int SearchMatch, vec2 Mouse)
{
	LucentConsole::RenderMetadata(TextRender(), CurrentConsole()->m_MetadataTooltip, pEntry->m_aTimestamp, pEntry->m_aSource,
		pEntry->m_Group, pEntry->m_Severity,
		pEntry->m_TimestampWidth, pEntry->m_SourceWidth, Y, FONT_SIZE, SearchMatch, Mouse);
}

void CGameConsole::RenderSelectionCopy(CInstance *pConsole, bool Visible, const LucentConsole::COverlayPosition &Fragment, std::span<const LucentConsole::COverlayPosition> Occupied, vec2 Mouse, bool Released, const CUIRect &Bounds)
{
	using namespace LucentStyle;
	if(pConsole->m_Selecting || pConsole->m_FilterPanel.OverlayVisible() || (pConsole->m_MouseIsPress && !m_ButtonPressed))
	{
		pConsole->m_CopyVisible = false;
		pConsole->m_CopyFade = {};
		return;
	}
	const auto &Clock = Motion::Clock();
	const bool Copied = pConsole->m_CopyFeedback.IsCopied();
	const char *pLabel = Localize(Copied ? "Copied" : "Copy selection");
	const float Width = Console::CopySize + Console::Gap + std::max(TextRender()->TextWidth(Console::FilterFontSize, Localize("Copy selection")), TextRender()->TextWidth(Console::FilterFontSize, Localize("Copied")));
	const auto Placement = LucentConsole::CopyBeside(Fragment, Width, Console::CopySize, {Bounds.x, Bounds.y, Bounds.w, Bounds.h}, Occupied);
	if(Visible && Placement.m_W == 0)
	{
		if(Copied)
			pConsole->m_CopyFeedback.HideAnchor();
		pConsole->m_CopyVisible = false;
		pConsole->m_CopyFade = {};
		return;
	}
	const bool Fits = Bounds.w >= Width && Bounds.h >= Console::CopySize;
	const bool Target = m_ConsoleState == CONSOLE_OPEN && Fits && pConsole->m_CopyFeedback.Visible(Clock.m_Now, false, Visible, pConsole->m_Selection.HasSelection());
	const float Fade = pConsole->m_CopyFade.Update(Target, Clock.m_Now, Clock.m_Animate, Console::CopyFade);
	if(Visible && Target)
	{
		pConsole->m_CopyRect = {Placement.m_X, Placement.m_Y, Placement.m_W, Placement.m_H};
	}
	pConsole->m_CopyVisible = Fits && (Target || Fade > 0);
	if(!Fits || Fade <= 0)
		return;
	pConsole->m_CopyRect.x = std::clamp(pConsole->m_CopyRect.x, Bounds.x, Bounds.x + Bounds.w - Width);
	pConsole->m_CopyRect.y = std::clamp(pConsole->m_CopyRect.y, Bounds.y, Bounds.y + Bounds.h - Console::CopySize);
	pConsole->m_CopyRect.w = Width;
	const CUIRect &Rect = pConsole->m_CopyRect;
	const bool Hot = Target && Rect.Inside(Mouse);
	const bool Active = Target && Rect.Inside(m_ButtonPressPosition) && pConsole->m_MouseIsPress;
	if(Active)
		m_ButtonPressed = true;
	const float Hover = LucentUi::Highlight(&pConsole->m_CopyRect, Hot, LucentUi::EChannel::HOVER, Console::FilterTransition);
	TextRender()->TextColor(Style::Text.WithAlpha(Fade * (Active ? 1.f : Motion::Lerp(Console::CopyIdleAlpha, 1.f, Hover))));
	CUIRect Icon{Rect.x, Rect.y, Console::CopySize, Rect.h}, Label{Rect.x + Console::CopySize + Console::Gap, Rect.y, Rect.w - Console::CopySize - Console::Gap, Rect.h};
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	Ui()->DoLabel(&Icon, Copied ? FontIcon::CHECK : FontIcon::COPY, Console::FilterFontSize, TEXTALIGN_MC);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	Ui()->DoLabel(&Label, pLabel, Console::FilterFontSize, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	if(!Copied && Target && Released && Rect.Inside(m_ButtonPressPosition) && Hot)
		pConsole->CopySelection();
}

void CGameConsole::CInstance::MeasureLogMetadata(CBacklogEntry *pEntry, float Width) const
{
	using namespace LucentStyle::Console;
	pEntry->m_TimestampWidth = std::min(Width * MetadataWidthFraction, m_pGameConsole->TextRender()->TextWidth(MetadataSize, pEntry->m_aTimestamp) + 2 * PillPadding);
	pEntry->m_SourceWidth = std::min(Width * MetadataWidthFraction, m_pGameConsole->TextRender()->TextWidth(MetadataSize, pEntry->m_aSource) + 2 * PillPadding + MetadataSize);
	pEntry->m_MessageX = pEntry->m_TimestampWidth + pEntry->m_SourceWidth + 2 * Gap;
}

void CGameConsole::PrepareLogMetadataHit(const CInstance::CBacklogEntry *pEntry, vec2 Mouse, float EntryTop, CTextCursor &Hit, const char *&pHitText, int &CharacterOffset, int &HitCharacters)
{
	using namespace LucentStyle::Console;
	const bool Timestamp = Mouse.x < pEntry->m_TimestampWidth + Gap / 2;
	pHitText = Timestamp ? pEntry->m_aTimestamp : pEntry->m_aSource;
	CharacterOffset = Timestamp ? 0 : pEntry->m_SourceCharacterOffset + 2;
	HitCharacters = str_utf8_offset_bytes_to_chars(pHitText, str_length(pHitText));
	Hit.SetPosition(vec2((Timestamp ? 0.f : pEntry->m_TimestampWidth + Gap + MetadataSize) + PillPadding, EntryTop + (FONT_SIZE - MetadataSize) / 2));
	Hit.m_FontSize = MetadataSize;
	Hit.m_LineWidth = std::max(1.f, (Timestamp ? pEntry->m_TimestampWidth : pEntry->m_SourceWidth - MetadataSize) - 2 * PillPadding);
	Hit.m_MaxLines = 1;
	Hit.m_Flags |= TEXTFLAG_STOP_AT_END;
}

void CGameConsole::MeasureLogSelectionFragments(const CInstance::CBacklogEntry *pEntry, const CTextCursor &EntryCursor, int SelectionStart, int SelectionStop, int BodyStart, int BodyStop, float EntryTop, float LineHeight, float ClipTop, float BottomY, float Width, SSelectionOverlay &Overlay)
{
	if(BodyStart != BodyStop)
	{
		CTextCursor End;
		End.m_FontSize = FONT_SIZE;
		End.m_LineSpacing = LINE_SPACING;
		End.m_LineWidth = EntryCursor.m_LineWidth;
		End.SetPosition(vec2(pEntry->m_MessageX, EntryTop));
		End.m_Flags = 0;
		End.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
		End.m_CursorMode = TEXT_CURSOR_CURSOR_MODE_SET;
		End.m_CursorCharacter = BodyStop;
		End.m_RenderCursor = false;
		TextRender()->TextEx(&End, pEntry->Message());
		CTextCursor Start = End;
		Start.m_GlyphCount = Start.m_CharCount = 0;
		Start.m_LineCount = 1;
		Start.SetPosition(vec2(pEntry->m_MessageX, EntryTop));
		Start.m_CursorCharacter = BodyStart;
		TextRender()->TextEx(&Start, pEntry->Message());
		const auto A = Start.m_CursorRenderedPosition, B = End.m_CursorRenderedPosition;
		const bool TakeAnchor = !Overlay.m_Visible;
		const float FirstRow = A.y + std::max(0.f, std::ceil((ClipTop - A.y - LineHeight) / LineHeight)) * LineHeight;
		for(float RowY = FirstRow; RowY <= B.y + .5f && RowY < BottomY; RowY += LineHeight)
		{
			const float Top = std::max(RowY, ClipTop), Bottom = std::min(RowY + LineHeight, BottomY);
			const float Left = absolute(RowY - A.y) < LineHeight / 2 ? A.x : pEntry->m_MessageX;
			const float Right = absolute(RowY - B.y) < LineHeight / 2 ? B.x : Width - 10.f;
			if(Bottom <= Top || Right <= Left)
				continue;
			const LucentConsole::COverlayPosition Rect{Left, Top, Right - Left, Bottom - Top};
			if(Overlay.m_OccupiedCount < Overlay.m_aOccupied.size())
				Overlay.m_aOccupied[Overlay.m_OccupiedCount++] = Rect;
			if(TakeAnchor)
				Overlay.m_Fragment = Rect;
			Overlay.m_Visible = true;
		}
	}
	const bool TimestampSelected = SelectionStart < pEntry->m_SourceCharacterOffset - 1 && SelectionStop > SelectionStart;
	const bool SourceSelected = SelectionStart < pEntry->m_MessageCharacterOffset && SelectionStop > pEntry->m_SourceCharacterOffset;
	if((TimestampSelected || SourceSelected) && EntryTop >= ClipTop && EntryTop < BottomY)
	{
		const LucentConsole::COverlayPosition Rect{0, EntryTop, SourceSelected ? pEntry->m_MessageX : pEntry->m_TimestampWidth, FONT_SIZE};
		if(Overlay.m_OccupiedCount < Overlay.m_aOccupied.size())
			Overlay.m_aOccupied[Overlay.m_OccupiedCount++] = Rect;
		if(!Overlay.m_Visible)
			Overlay.m_Fragment = Rect;
		Overlay.m_Visible = true;
	}
}
