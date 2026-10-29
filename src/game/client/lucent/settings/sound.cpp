#include "sound.h"

#include "mode_render.h"

#include <game/client/lineinput.h>
#include <game/client/ui_scrollregion.h>

#include <type_traits>
namespace LucentSettings
{
	namespace
	{
		struct CView
		{
			CModePresentation m_Mode;
			std::array<CButtonContainer, 2> m_Buttons;
			CScrollRegion m_Scroll;
		};
		CView &SoundView()
		{
			static CView s_View;
			return s_View;
		}
		void Text(CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pText, ColorRGBA Color)
		{
			const auto Previous = pTextRender->GetTextColor();
			pTextRender->TextColor(Color);
			SLabelProperties Props;
			Props.m_MaxWidth = std::max(0.f, Rect.w);
			Props.m_EnableWidthCheck = false;
			pUi->DoLabel(&Rect, pText, 11, TEXTALIGN_ML, Props);
			pTextRender->TextColor(Previous);
		}
	}
	const char *SoundLabel(ESoundControl Control)
	{
		static const char *const s_apLabels[] = {"Use sounds", "Enable game sounds", "Enable gun sound", "Play background music",
			"Sound volume", "Game sound volume", "Chat sound volume", "Map sound volume", "Background music volume",
			"Mute when not active", "Enable long pain sound (used when shooting in freeze)", "Enable server message sound",
			"Enable regular chat sound", "Enable team chat sound", "Enable highlighted chat sound"};
		static_assert(std::size(s_apLabels) == SoundControlCount);
		return Localize(s_apLabels[int(Control)]);
	}
	void LeaveSound(CUi *pUi)
	{
		auto &View = SoundView();
		for(auto &Button : View.m_Buttons)
		{
			if(pUi->CheckActiveItem(&Button))
				pUi->SetActiveItem(nullptr);
			if(pUi->HotItem() == &Button)
				pUi->SetHotItem(nullptr);
		}
		View.m_Mode.Leave();
	}
	void RenderSound(CUi *pUi, ITextRender *pTextRender, CUIRect View, const CSoundState &State, const FSoundControl &RenderControl)
	{
		LucentUi::CScope Style(pUi);
		LucentUi::CInteractionScope PopupInput(pUi, !pUi->IsPopupOpen());
		auto &Page = SoundView();
		auto &Mode = Page.m_Mode;
		const auto &Clock = LucentStyle::Motion::Clock();
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		const bool Keyboard = pUi->Enabled() && !pUi->ActiveItem() && !CLineInput::GetActiveInput();
		if(!pUi->Enabled() || CLineInput::GetActiveInput())
			Mode.ClearKeyboardFocus();
		else if(Keyboard)
		{
			if(pUi->Input()->KeyPress(KEY_TAB))
				Mode.FocusMode();
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
		const auto Header = RenderModeHeader(pUi, pTextRender, CGeneralLayout::ContentRegion(View), Mode, Page.m_Buttons, Keyboard);
		Mode.UpdateMode(Clock.m_Now, Clock.m_Animate);
		View.y = Header.m_Content.y;
		View.h = Header.m_Content.h;
		const auto Heading = [](int I) { const char *apLabels[] = {"Playback", "Volume", "Sound", "Chat"}; return Localize(apLabels[I]); };
		bool Paint = false;
		const auto Compose = [&]() {
			const auto Layout = CSoundLayout::Calculate(View);
			const auto Measure = [&](auto Item, float Width) {
				const char *pLabel;
				if constexpr(std::is_same_v<decltype(Item), ESoundControl>)
					pLabel = SoundLabel(Item);
				else
					pLabel = Heading(Item);
				return pTextRender->TextBoundingBox(11, pLabel, -1, std::max(1.f, Width)).m_H;
			};
			const float Height = Layout.Compose(Mode.m_ShownAdvanced, Measure, [&](ESoundControl Control, CUIRect Row) {
    if(!Paint) return;
    Page.m_Scroll.AddRect(Row);
    const bool Enabled = State.m_Enabled[int(Control)];
    const bool VisualEnabled = pUi->Enabled() && Enabled;
    LucentUi::CInteractionScope Input(pUi, Enabled && Mode.ContentInteractive() && !Page.m_Scroll.RectClipped(Row));
    CUIRect Caption, Value, Rail = Row;
    if(SoundSlider(Control)) {
     CGeneralLayout::SliderRegions(Row, Caption, Value, Rail);
     Text(pUi, pTextRender, Caption, SoundLabel(Control), VisualEnabled ? LucentStyle::Style::Text : LucentStyle::Style::MutedText);
    }
    RenderControl(Control, Rail, VisualEnabled, SoundSlider(Control) ? &Value : nullptr); }, [&](int Name, CUIRect Row) { if(Paint) Text(pUi, pTextRender, Row, Heading(Name), LucentStyle::Style::MutedText); });
			if(Paint)
				Page.m_Scroll.AddRect({View.x, View.y + Height, View.w, 1});
			return Height + 1;
		};
		CScrollRegionParams Params;
		Params.m_ScrollbarThickness = 8;
		Params.m_ScrollbarMargin = 1;
		Params.m_ScrollUnit = 54;
		const CUIRect Original = View;
		View.w = std::max(0.f, View.w - Params.m_ScrollbarThickness);
		const float Height = Compose();
		View = Original;
		Page.m_Scroll.Begin(&View, &Params, Height);
		Paint = true;
		{
			LucentUi::COpacityScope Fade(pTextRender, Mode.m_ModeReveal.m_Value);
			Compose();
		}
		Page.m_Scroll.End();
	}
}
