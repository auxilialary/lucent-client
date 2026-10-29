#ifndef GAME_CLIENT_LUCENT_SETTINGS_GENERAL_H
#define GAME_CLIENT_LUCENT_SETTINGS_GENERAL_H

#include "mode.h"

#include <game/client/lucent/motion/transition.h>
#include <game/client/lucent/ui/slider_geometry.h>
#include <game/client/ui_rect.h>

#include <array>
#include <functional>
#include <vector>

class CUi;
class ITextRender;
class CTheme;

namespace LucentSettings
{
	// Only General's existing controls, not a generic form schema or config model.
	enum class EGeneralControl
	{
		DYNAMIC_CAMERA,
		SMOOTH_CAMERA,
		WEAPON_PICKUP,
		WEAPON_EMPTY,
		SKIP_START,
		UPDATE_RATE,
		POWER_SAVE,
		SETTINGS_FILE,
		SAVES_FILE,
		CONFIG_DIRECTORY,
		AUTO_DEMO,
		MAX_DEMOS,
		AUTO_SCREENSHOT,
		MAX_SCREENSHOTS,
		AUTO_STATBOARD,
		MAX_STATBOARD,
		AUTO_CSV,
		MAX_CSV,
		DEFAULT_ZOOM,
		BEST_RACE_DEMO,
		REPLAYS,
		REPLAY_LENGTH,
		COUNT
	};
	constexpr int GeneralControlCount = int(EGeneralControl::COUNT);
	struct CGeneralState
	{
		std::array<bool, GeneralControlCount> m_Visible, m_Enabled;
		std::array<float, GeneralControlCount> m_Reveal;
		bool m_Advanced = false, m_ContentInteractive = true;
		float m_ContentAlpha = 1;
		static bool AdvancedControl(EGeneralControl Control)
		{
			using C = EGeneralControl;
			return Control != C::DYNAMIC_CAMERA && Control != C::WEAPON_PICKUP && Control != C::WEAPON_EMPTY &&
			       Control != C::DEFAULT_ZOOM && Control != C::AUTO_DEMO && Control != C::MAX_DEMOS && Control != C::BEST_RACE_DEMO;
		}
		CGeneralState()
		{
			m_Visible.fill(true);
			m_Enabled.fill(true);
			m_Reveal.fill(1);
		}
		bool Interactive(EGeneralControl Control) const
		{
			return m_ContentInteractive && m_ContentAlpha == 1.f && m_Visible[int(Control)] && m_Enabled[int(Control)] && m_Reveal[int(Control)] == 1.f;
		}
	};
	struct CGeneralPresentation : CModePresentation
	{
		int m_FileFocus = -1;
		void FocusMode()
		{
			CModePresentation::FocusMode();
			m_FileFocus = -1;
		}
		void ClearKeyboardFocus()
		{
			CModePresentation::ClearKeyboardFocus();
			m_FileFocus = -1;
		}
		bool SelectMode(bool Advanced)
		{
			if(!CModePresentation::SelectMode(Advanced))
				return false;
			m_FileFocus = -1;
			return true;
		}
		void ToggleMode() { SelectMode(!m_Advanced); }
		void Leave()
		{
			CModePresentation::Leave();
			m_FileFocus = -1;
		}
		void MoveFileFocus(int Direction) { m_FileFocus = (m_FileFocus + Direction + 3) % 3; }

		static constexpr std::array Limits = {EGeneralControl::MAX_DEMOS, EGeneralControl::MAX_SCREENSHOTS,
			EGeneralControl::MAX_STATBOARD, EGeneralControl::MAX_CSV, EGeneralControl::REPLAY_LENGTH};
		std::array<LucentStyle::Motion::CTransition, Limits.size()> m_Limits;
		CGeneralState Update(const CGeneralState &Target, double Now, bool Animate)
		{
			const bool Fresh = UpdateMode(Now, Animate);
			if(Fresh)
				m_FileFocus = -1;
			auto State = Target;
			State.m_Advanced = m_ShownAdvanced;
			State.m_ContentAlpha = m_ModeReveal.m_Value;
			State.m_ContentInteractive = m_ShownAdvanced == m_Advanced;
			for(size_t i = 0; i < Limits.size(); ++i)
			{
				const int Index = int(Limits[i]);
				State.m_Reveal[Index] = m_Limits[i].Update(Target.m_Enabled[Index], Now, Animate && !Fresh, LucentStyle::Motion::Hover);
				State.m_Visible[Index] = State.m_Reveal[Index] > 0;
			}
			for(int i = 0; i < GeneralControlCount; ++i)
				State.m_Visible[i] = State.m_Visible[i] && (CGeneralState::AdvancedControl(EGeneralControl(i)) == m_ShownAdvanced);
			return State;
		}
	};

	using CGeneralHeaderLayout = CModeHeaderLayout;

	enum class EGeneralSection
	{
		GAME,
		CLIENT,
		CAPTURE,
		FILES
	};
	struct CGeneralLayout
	{
		CUIRect m_Left, m_Right;
		bool m_TwoColumns;
		static constexpr float RowHeight = 28, RowGap = 2, SectionGap = 14;
		static CGeneralLayout Calculate(CUIRect View)
		{
			const float Width = std::max(0.f, View.w);
			const bool Wide = Width >= 620;
			const float Left = Wide ? std::min(400.f, (Width - LucentStyle::Checkbox::Size - LucentStyle::Style::LabelGap) * .44f) : std::min(480.f, Width);
			return {{View.x, View.y, Left, View.h}, {View.x + (Wide ? Left + 24 : 0), View.y, Wide ? std::min(500.f, Width - Left - 24) : Left, View.h}, Wide};
		}
		static constexpr float SliderHeight = 28, ValueWidth = 64, HeaderGap = 8;
		static CUIRect ContentRegion(CUIRect Column) { return SettingsContentRegion(Column); }

		static void SliderRegions(CUIRect Block, CUIRect &Caption, CUIRect &Value, CUIRect &Rail)
		{
			CUIRect Header;
			Block.HSplitTop(std::max(0.f, Block.h - SliderHeight), &Header, &Rail);
			Header.VSplitRight(std::min(ValueWidth, std::max(0.f, Header.w) * .4f), &Caption, &Value);
			Caption.VSplitRight(std::min(HeaderGap, Caption.w), &Caption, nullptr);
		}
		static CUIRect Slider(CUIRect Row)
		{
			CUIRect Caption, Value, Rail;
			SliderRegions(Row, Caption, Value, Rail);
			return Rail;
		}
		// One composition for measurement, painting and geometry regression checks.
		// Measure returns wrapped text height; callbacks retain the real DDNet IDs.
		template<typename FMeasure, typename FDraw, typename FHeading>
		float Compose(const CGeneralState &State, FMeasure Measure, FDraw Draw, FHeading Heading) const
		{
			using C = EGeneralControl;
			const CUIRect LeftContent = ContentRegion(m_Left);
			const CUIRect RightContent = ContentRegion(m_Right);
			float Y = LeftContent.y;
			const auto Section = [&](EGeneralSection Name, CUIRect Column) {
				const float H = std::max(18.f, Measure(Name, Column.w) + 2);
				Heading(Name, CUIRect{Column.x, Y, Column.w, H});
				Y += H + 4;
			};
			const auto Check = [&](C Control, CUIRect Column) {
				if(!State.m_Visible[int(Control)])
					return;
				const float Width = Column.w;
				const float H = std::max(RowHeight, Measure(Control, std::max(1.f, Width - LucentStyle::Checkbox::Size - LucentStyle::Style::LabelGap)) + 8);
				Draw(Control, CUIRect{Column.x, Y, Width, H});
				Y += H + RowGap;
			};
			const auto SliderRow = [&](C Control, CUIRect Column) {
				if(!State.m_Visible[int(Control)])
					return;
				CUIRect Block = Column;
				Block.y = Y;
				Block.h = SliderHeight;
				CUIRect Caption, Value, Rail;
				SliderRegions(Block, Caption, Value, Rail);
				Block.h += std::max(18.f, Measure(Control, std::max(1.f, Caption.w)) + 4);
				Draw(Control, Block);
				Y += (Block.h + RowGap) * State.m_Reveal[int(Control)];
			};
			if(State.m_Advanced)
			{
				Section(EGeneralSection::CLIENT, LeftContent);
				Check(C::SMOOTH_CAMERA, LeftContent);
				Check(C::SKIP_START, LeftContent);
				SliderRow(C::UPDATE_RATE, LeftContent);
				Check(C::POWER_SAVE, LeftContent);
				Y += SectionGap;
				Section(EGeneralSection::FILES, LeftContent);
				Check(C::SETTINGS_FILE, LeftContent);
				Check(C::SAVES_FILE, LeftContent);
				Check(C::CONFIG_DIRECTORY, LeftContent);
			}
			else
			{
				Section(EGeneralSection::GAME, LeftContent);
				Check(C::DYNAMIC_CAMERA, LeftContent);
				Check(C::WEAPON_PICKUP, LeftContent);
				Check(C::WEAPON_EMPTY, LeftContent);
				SliderRow(C::DEFAULT_ZOOM, LeftContent);
			}
			const float LeftBottom = Y;
			Y = m_TwoColumns ? RightContent.y : Y + SectionGap;
			Section(EGeneralSection::CAPTURE, RightContent);
			const auto Capture = [&](C Toggle, C Limit) {
				Check(Toggle, RightContent);
				SliderRow(Limit, RightContent);
				Y += 4;
			};
			if(State.m_Advanced)
			{
				Capture(C::AUTO_SCREENSHOT, C::MAX_SCREENSHOTS);
				Capture(C::AUTO_STATBOARD, C::MAX_STATBOARD);
				Capture(C::AUTO_CSV, C::MAX_CSV);
				Capture(C::REPLAYS, C::REPLAY_LENGTH);
			}
			else
			{
				Capture(C::AUTO_DEMO, C::MAX_DEMOS);
				Check(C::BEST_RACE_DEMO, RightContent);
			}
			return std::max(LeftBottom, Y) - m_Left.y + 6;
		}
	};
	using FGeneralControl = std::function<void(EGeneralControl, CUIRect, bool, bool, bool, const CUIRect *)>;
	void LeaveGeneral(CUi *pUi);
	void RenderGeneral(CUi *pUi, ITextRender *pTextRender, CUIRect View, const CGeneralState &State, const FGeneralControl &RenderControl);
	bool GeneralCheckbox(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pLabel, bool Checked, const CUIRect *pRect, bool VisualEnabled);
	bool GeneralFileButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pLabel, const CUIRect *pRect, bool Focused, bool Activate);
	using FSelectGeneralTheme = std::function<void(const CTheme &)>;
	void GeneralThemes(CUi *pUi, ITextRender *pTextRender, CUIRect View, const std::vector<CTheme> &vThemes, int Selected, const FSelectGeneralTheme &SelectTheme);
}
#endif
