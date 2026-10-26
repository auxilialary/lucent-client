#ifndef GAME_CLIENT_LUCENT_CONSOLE_FILTER_UI_H
#define GAME_CLIENT_LUCENT_CONSOLE_FILTER_UI_H

#include "filters.h"
#include "layout.h"

#include <engine/font_icons.h>

#include <game/client/lucent/motion/transition.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <array>

namespace LucentConsole
{
	class CFilterPanel
	{
		enum class EItem
		{
			CATEGORY,
			SEVERITY,
			COLLECT,
			HEADING,
			DIVIDER,
			HINT
		};
		CPopoverState m_Popover;
		bool m_Compact = false, m_Remote = false, m_Capture = false, m_IgnoreRelease = false;
		bool m_Down = false, m_ShowPrimary = false;
		float m_MainPadding = LucentStyle::Console::FilterPadding, m_HintHeight = 0;
		int m_CollectedLevel = 0;
		float m_Scroll = 0, m_ContentHeight = 0;
		CUIRect m_Main{}, m_Bounds{}, m_Popup{};
		std::array<CUIRect, 13> m_aMain{};
		std::array<int, 24> m_Ids{};
		vec2 m_Mouse{}, m_Press{};
		LucentStyle::Motion::CTransition m_Fade;
		static constexpr const char *Names[] = {"Chat", "Server", "Network", "Graphics", "Browser", "Client", "Assets", "Record", "Editor", "Other"};

		float LabelWidth(ITextRender *pText, const char *pLabel) const
		{
			return pText->TextWidth(LucentStyle::Console::FilterFontSize, pLabel) + 2 * LucentStyle::Console::FilterPadding;
		}
		bool InsideContent(vec2 Point) const
		{
			CUIRect Inner = m_Popup;
			Inner.Margin(LucentStyle::Console::PopoverPadding, &Inner);
			return Inner.Inside(Point);
		}
		const char *Hint() const
		{
			return m_Remote ? Localize("RCON levels are unknown.") : m_CollectedLevel < 1 ? Localize("Debug/Trace collection is off.") :
									 m_CollectedLevel < 2         ? Localize("Trace collection is off.") :
													Localize("Display filters do not change collection.");
		}
		template<typename F>
		void Items(F Visit) const
		{
			using namespace LucentStyle;
			float Y = m_Popup.y + Console::PopoverPadding - m_Scroll;
			const float X = m_Popup.x + Console::PopoverPadding, W = m_Popup.w - 2 * Console::PopoverPadding;
			const auto Row = [&](EItem Type, int Value, const char *pLabel, float Height) { Visit(Type, Value, pLabel, CUIRect{X, Y, W, Height}); Y += Height; };
			if(m_Popover.m_Display == EPopover::MORE)
			{
				for(int i = m_ShowPrimary ? 0 : 5; i < (m_ShowPrimary ? 5 : 10); ++i)
					Row(EItem::CATEGORY, i, Localize(Names[i]), Console::PopoverRow);
			}
			else if(m_Popover.m_Display == EPopover::LEVELS)
			{
				Row(EItem::HEADING, 0, Localize("Display"), Console::PopoverHeader);
				constexpr const char *Levels[] = {"Error", "Warning", "Info", "Debug", "Trace", "Unknown"};
				const float Cell = (W - Console::FilterGap) / 2;
				for(int i = 0; i < 6; ++i)
					Visit(EItem::SEVERITY, i, Localize(Levels[i]), CUIRect{X + (i % 2) * (Cell + Console::FilterGap), Y + (i / 2) * Console::PopoverRow, Cell, Console::PopoverRow});
				Y += 3 * Console::PopoverRow;
				Row(EItem::DIVIDER, 0, "", Console::FilterGap);
				Row(EItem::HEADING, 0, Localize("Log collection"), Console::PopoverHeader);
				if(!m_Remote)
				{
					Visit(EItem::COLLECT, 1, "Debug", CUIRect{X, Y, Cell, Console::PopoverRow});
					Visit(EItem::COLLECT, 2, "Trace", CUIRect{X + Cell + Console::FilterGap, Y, Cell, Console::PopoverRow});
					Y += Console::PopoverRow;
				}
				Row(EItem::HINT, 0, Hint(), m_HintHeight);
			}
		}
		bool Selected(EItem Type, int Value, const CFilters &Filters) const
		{
			return Type == EItem::CATEGORY ? !Filters.CategoriesUnrestricted() && Filters.Category(Groups[Value]) : Type == EItem::SEVERITY ? Filters.Severity(Value == 5 ? -1 : Value) :
																			  Type == EItem::COLLECT && m_CollectedLevel >= Value;
		}
		void DrawControl(CUi *pUi, ITextRender *pText, int Id, const char *pLabel, CUIRect Rect, bool Selected, bool Toggle, float Alpha, bool Interactive);

	public:
		struct CResult
		{
			bool m_Changed = false, m_Consumed = false;
			int m_CollectLevel = -1;
		};
		static constexpr std::array<EConsoleLogGroup, 10> Groups = {EConsoleLogGroup::CHAT, EConsoleLogGroup::SERVER, EConsoleLogGroup::NETWORK, EConsoleLogGroup::GRAPHICS, EConsoleLogGroup::BROWSER, EConsoleLogGroup::CLIENT, EConsoleLogGroup::ASSETS, EConsoleLogGroup::RECORD, EConsoleLogGroup::EDITOR, EConsoleLogGroup::OTHER};
		float Height(float Width) const { return PanelHeight(Width, false, false); }
		bool IsOpen() const { return m_Popover.IsOpen(); }
		bool OverlayVisible() const { return IsOpen() || m_Fade.m_Value > .001f; }
		bool Close() { return m_Popover.Close(); }
		void ToggleMore(bool Primary = false);
		void ToggleLevels();

		bool CapturePress(vec2 Point);
		bool Scroll(int Direction);
		void Prepare(ITextRender *pText, const CUIRect &Rect, const CUIRect &Bounds, int CollectedLevel, bool Remote);
		CResult Render(CUi *pUi, ITextRender *pText, vec2 Mouse, vec2 Press, bool Down, bool Released, bool Enabled, CFilters &Filters);
		void RenderPopover(CUi *pUi, ITextRender *pText, const CFilters &Filters, bool Enabled);
	};
}
#endif
