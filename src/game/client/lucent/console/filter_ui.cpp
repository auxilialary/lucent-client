#include "filter_ui.h"

#include <game/client/lucent/ui/controls.h>
#include <game/client/lucent/ui/scopes.h>

namespace LucentConsole
{
	void CFilterPanel::DrawControl(CUi *pUi, ITextRender *pText, int Id, const char *pLabel, CUIRect Rect, bool Selected, bool Toggle, float Alpha, bool Interactive)
	{
		using namespace LucentStyle;
		const bool Hot = Interactive && Rect.Inside(m_Mouse), Pressed = Interactive && m_Down && Rect.Inside(m_Press);
		const float Hover = LucentUi::Highlight(&m_Ids[Id], Hot, LucentUi::EChannel::HOVER, Console::PopoverMotion);
		const float Active = LucentUi::Highlight(&m_Ids[Id], Selected, LucentUi::EChannel::ACTIVE, Console::PopoverMotion);
		Rect.Draw(Style::Accent.WithAlpha(Alpha * (Hover * Style::Hover + (Pressed ? Style::Pressed : 0))), IGraphics::CORNER_ALL, Toggle ? Console::PopoverRadius / 2 : Style::Radius);
		const float Padding = Toggle ? Console::FilterPadding : m_MainPadding;
		if(Toggle)
		{
			CUIRect Mark{Rect.x + Rect.w - Padding - Console::PopoverIndicator, Rect.y + (Rect.h - Console::PopoverIndicator) / 2, Console::PopoverIndicator, Console::PopoverIndicator};
			Mark.Draw(Style::MutedText.WithAlpha(Alpha * Console::FilterInactiveAlpha * (1 - Active)), IGraphics::CORNER_ALL, Console::PopoverIndicator / 2);
			LucentUi::GradientFill(Mark, Alpha * Active, IGraphics::CORNER_ALL, Console::PopoverIndicator / 2);
			Rect.w -= Console::PopoverIndicator + Console::FilterGap;
		}
		else
		{
			CUIRect Underline{Rect.x + Padding, Rect.y + Rect.h - Style::Hairline, std::max(0.f, Rect.w - 2 * Padding), Style::Hairline};
			LucentUi::GradientFill(Underline, Alpha * std::max(Active, Hover) * Style::BorderActive, 0, 0);
		}
		Rect.VMargin(Padding, &Rect);
		{
			LucentUi::CTextScope TextStyle(pText, std::max(1.f, Rect.w), Active, Hover, true, Alpha * Motion::Lerp(Console::FilterInactiveAlpha, 1.f, std::max(Active, Hover)));
			CTextCursor Text;
			Text.SetPosition(vec2(Rect.x, Rect.y + (Rect.h - Console::FilterFontSize) / 2));
			Text.m_FontSize = Console::FilterFontSize;
			Text.m_LineWidth = -1;
			Text.m_MaxLines = 1;
			pText->TextEx(&Text, pLabel);
		}
		pText->TextColor(pText->DefaultTextColor());
	}

	void CFilterPanel::ToggleMore(bool Primary)
	{
		if(m_Popover.m_Open == EPopover::MORE && m_ShowPrimary != Primary)
			m_Popover.Close();
		m_ShowPrimary = Primary;
		m_Popover.Toggle(EPopover::MORE);
		m_Scroll = 0;
	}

	void CFilterPanel::ToggleLevels()
	{
		m_Popover.Toggle(EPopover::LEVELS);
		m_Scroll = 0;
	}

	bool CFilterPanel::CapturePress(vec2 Point)
	{
		m_Capture = m_Main.Inside(Point) || (IsOpen() && m_Popup.Inside(Point));
		m_IgnoreRelease = false;
		if(m_Popover.DismissOutside(m_Popup.Inside(Point), m_aMain[10].Inside(Point) || m_aMain[11].Inside(Point) || m_aMain[12].Inside(Point)))
		{
			m_Capture = true;
			m_IgnoreRelease = !m_Main.Inside(Point);
		}
		return m_Capture;
	}

	bool CFilterPanel::Scroll(int Direction)
	{
		if(!IsOpen())
			return false;
		m_Scroll = std::clamp(m_Scroll + Direction * LucentStyle::Console::PopoverRow, 0.f, std::max(0.f, m_ContentHeight - m_Popup.h));
		return true;
	}

	void CFilterPanel::Prepare(ITextRender *pText, const CUIRect &Rect, const CUIRect &Bounds, int CollectedLevel, bool Remote)
	{
		using namespace LucentStyle;
		m_Main = Rect;
		m_Bounds = Bounds;
		m_CollectedLevel = CollectedLevel;
		m_Remote = Remote;
		m_aMain.fill({});
		const auto NaturalWidth = [&](const char *pLabel) { return pText->TextWidth(Console::FilterFontSize, pLabel); };
		float FullWidth = NaturalWidth(Localize("All")) + NaturalWidth(Localize("More")) + NaturalWidth(Localize("Levels")) + 16 * Console::FilterPadding + 7 * Console::FilterGap;
		for(int i = 0; i < 5; ++i)
			FullWidth += NaturalWidth(Localize(Names[i]));
		m_Compact = FullWidth > Rect.w;
		m_MainPadding = Console::FilterPadding;
		if(m_Compact)
		{
			const float TextWidth = NaturalWidth(Localize("All")) + NaturalWidth(Localize("Categories")) + NaturalWidth(Localize("More")) + NaturalWidth(Localize("Levels"));
			m_MainPadding = std::min(Console::FilterPadding, std::max(0.f, (Rect.w - TextWidth - 3 * Console::FilterGap) / 8));
		}
		float X = Rect.x;
		const auto Place = [&](int Id, const char *pLabel) { const float Width = NaturalWidth(pLabel) + 2 * m_MainPadding; m_aMain[Id] = {X, Rect.y, Width, Rect.h}; X += Width + Console::FilterGap; };
		Place(0, Localize("All"));
		if(!m_Compact)
			for(int i = 0; i < 5; ++i)
				Place(1 + i, Localize(Names[i]));
		else
			Place(10, Localize("Categories"));
		Place(11, Localize("More"));
		const float LevelsWidth = NaturalWidth(Localize("Levels")) + 2 * m_MainPadding;
		m_aMain[12] = {Rect.x + Rect.w - LevelsWidth, Rect.y, LevelsWidth, Rect.h};
		const float PopupWidth = m_Popover.m_Display == EPopover::MORE ? Console::PopoverWidth : Console::LevelsWidth;
		m_HintHeight = pText->TextBoundingBox(Console::MetadataSize, Hint(), -1, std::max(1.f, std::min(PopupWidth, Bounds.w) - 2 * Console::PopoverPadding)).m_H;
		m_ContentHeight = PopoverContentHeight(m_Popover.m_Display == EPopover::LEVELS, Remote, m_HintHeight);
		const CUIRect &Anchor = m_aMain[m_Popover.m_Display == EPopover::MORE ? (m_ShowPrimary ? 10 : 11) : 12];
		const float AnchorX = m_Popover.m_Display == EPopover::LEVELS ? Anchor.x + Anchor.w - PopupWidth : Anchor.x;
		const auto &Clock = Motion::Clock();
		const float Fade = m_Fade.Update(IsOpen(), Clock.m_Now, Clock.m_Animate, Console::PopoverMotion);
		const auto Popup = AboveAnchor(AnchorX, Rect.y, PopupWidth, m_ContentHeight, Bounds.x, Bounds.y, Bounds.w, Bounds.h, Console::Gap);
		m_Popup = {Popup.m_X, std::min(Popup.m_Y + Console::PopoverTravel * (1 - Fade), Bounds.y + Bounds.h - Popup.m_H), Popup.m_W, Popup.m_H};
		m_Scroll = std::clamp(m_Scroll, 0.f, std::max(0.f, m_ContentHeight - m_Popup.h));
	}

	CFilterPanel::CResult CFilterPanel::Render(CUi *pUi, ITextRender *pText, vec2 Mouse, vec2 Press, bool Down, bool Released, bool Enabled, CFilters &Filters)
	{
		CResult Result;
		m_Mouse = Mouse;
		m_Press = Press;
		m_Down = Down;
		const bool Click = Enabled && Released && m_Capture && !m_IgnoreRelease;
		const auto Hit = [&](const CUIRect &Box) { return Click && Box.Inside(Press) && Box.Inside(Mouse); };
		Result.m_Consumed = m_Capture && (Down || Released);
		if(Hit(m_aMain[0]))
		{
			Filters.ResetCategories();
			Result.m_Changed = true;
		}
		if(!m_Compact)
			for(int i = 0; i < 5; ++i)
				if(Hit(m_aMain[1 + i]))
				{
					Filters.ToggleCategory(Groups[i]);
					Result.m_Changed = true;
				}
		if(m_Compact && Hit(m_aMain[10]))
			ToggleMore(true);
		if(Hit(m_aMain[11]))
			ToggleMore(false);
		if(Hit(m_aMain[12]))
			ToggleLevels();
		Prepare(pText, m_Main, m_Bounds, m_CollectedLevel, m_Remote);
		if(IsOpen())
			Items([&](EItem Type, int Value, const char *pLabel, CUIRect Box) {
				if(!InsideContent(Mouse) || !InsideContent(Press) || !Hit(Box))
					return;
				if(Type == EItem::CATEGORY)
				{
					Filters.ToggleCategory(Groups[Value]);
					Result.m_Changed = true;
				}
				if(Type == EItem::SEVERITY)
				{
					Filters.ToggleSeverity(Value == 5 ? -1 : Value);
					Result.m_Changed = true;
				}
				if(Type == EItem::COLLECT)
					Result.m_CollectLevel = std::max(Value, m_CollectedLevel);
			});
		DrawControl(pUi, pText, 0, Localize("All"), m_aMain[0], Filters.CategoriesUnrestricted(), false, 1, Enabled);
		if(!m_Compact)
			for(int i = 0; i < 5; ++i)
				DrawControl(pUi, pText, 1 + i, Localize(Names[i]), m_aMain[1 + i], !Filters.CategoriesUnrestricted() && Filters.Category(Groups[i]), false, 1, Enabled);
		if(m_Compact)
			DrawControl(pUi, pText, 10, Localize("Categories"), m_aMain[10], m_Popover.m_Open == EPopover::MORE && m_ShowPrimary, false, 1, Enabled);
		DrawControl(pUi, pText, 11, Localize("More"), m_aMain[11], m_Popover.m_Open == EPopover::MORE && !m_ShowPrimary, false, 1, Enabled);
		DrawControl(pUi, pText, 12, Localize("Levels"), m_aMain[12], m_Popover.m_Open == EPopover::LEVELS, false, 1, Enabled);
		if(Filters.HasAdditionalCategories())
		{
			CUIRect Dot{m_aMain[11].x + m_aMain[11].w - LucentStyle::Console::SeveritySize, m_aMain[11].y + LucentStyle::Console::FilterPadding, LucentStyle::Console::SeveritySize, LucentStyle::Console::SeveritySize};
			Dot.Draw(LucentStyle::Style::Accent, IGraphics::CORNER_ALL, LucentStyle::Console::PillRadius);
		}
		if(Released)
		{
			m_Capture = false;
			m_IgnoreRelease = false;
		}
		return Result;
	}

	void CFilterPanel::RenderPopover(CUi *pUi, ITextRender *pText, const CFilters &Filters, bool Enabled)
	{
		using namespace LucentStyle;
		const auto &Clock = Motion::Clock();
		const float Fade = m_Fade.Update(IsOpen() && Enabled, Clock.m_Now, Clock.m_Animate, Console::PopoverMotion);
		if(Fade <= 0 || m_Popup.h <= 2 * Console::PopoverPadding)
			return;
		m_Popup.Draw(Style::Accent.WithAlpha(Fade * Style::BorderIdle), IGraphics::CORNER_ALL, Console::PopoverRadius);
		CUIRect Inner;
		m_Popup.Margin(Style::Hairline, &Inner);
		Inner.Draw(Console::PopoverSurface.WithAlpha(Console::MorePopoverOpacity * Fade), IGraphics::CORNER_ALL, Console::PopoverRadius);
		m_Popup.Margin(Console::PopoverPadding, &Inner);
		pUi->ClipEnable(&Inner);
		Items([&](EItem Type, int Value, const char *pLabel, CUIRect Box) {
			if(Box.y + Box.h <= Inner.y || Box.y >= Inner.y + Inner.h)
				return;
			if(Type == EItem::DIVIDER)
			{
				Box.y += Box.h / 2;
				Box.h = Style::Hairline;
				LucentUi::Divider(Box, Fade * Style::BorderActive, false, true);
				return;
			}
			if(Type == EItem::HEADING || Type == EItem::HINT)
			{
				pText->TextColor((Type == EItem::HEADING ? Style::Text : Style::MutedText).WithAlpha(Fade * (Type == EItem::HEADING ? 1.f : Console::CopyIdleAlpha)));
				CTextCursor Text;
				Text.SetPosition(vec2(Box.x, Box.y));
				Text.m_FontSize = Console::MetadataSize;
				Text.m_LineWidth = Box.w;
				Text.m_MaxLines = Type == EItem::HINT ? 4 : 1;
				pText->TextEx(&Text, pLabel);
				return;
			}
			const int Id = Type == EItem::CATEGORY ? 1 + Value : Type == EItem::SEVERITY ? 13 + Value :
												       18 + Value;
			DrawControl(pUi, pText, Id, pLabel, Box, Selected(Type, Value, Filters), true, Fade, Enabled && IsOpen() && InsideContent(m_Mouse));
		});
		pUi->ClipDisable();
		pText->TextColor(pText->DefaultTextColor());
	}
}
