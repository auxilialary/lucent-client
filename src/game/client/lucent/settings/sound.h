#ifndef GAME_CLIENT_LUCENT_SETTINGS_SOUND_H
#define GAME_CLIENT_LUCENT_SETTINGS_SOUND_H
#include "general.h"
namespace LucentSettings
{
	enum class ESoundControl
	{
		ENABLE,
		GAME,
		GUN,
		MUSIC,
		VOLUME,
		GAME_VOLUME,
		CHAT_VOLUME,
		MAP_VOLUME,
		MUSIC_VOLUME,
		MUTE,
		LONG_PAIN,
		SERVER,
		CHAT,
		TEAM_CHAT,
		HIGHLIGHT,
		COUNT
	};
	constexpr int SoundControlCount = int(ESoundControl::COUNT);
	constexpr bool SoundSlider(ESoundControl C) { return C >= ESoundControl::VOLUME && C <= ESoundControl::MUSIC_VOLUME; }
	struct CSoundState
	{
		std::array<bool, SoundControlCount> m_Enabled;
		CSoundState(bool Sounds = true, bool Game = true, bool Music = true)
		{
			m_Enabled.fill(Sounds);
			m_Enabled[int(ESoundControl::ENABLE)] = true;
			for(auto Control : {ESoundControl::GUN, ESoundControl::LONG_PAIN, ESoundControl::GAME_VOLUME, ESoundControl::MAP_VOLUME})
				m_Enabled[int(Control)] = Sounds && Game;
			m_Enabled[int(ESoundControl::MUSIC_VOLUME)] = Sounds && Music;
		}
	};
	struct CSoundLayout
	{
		CUIRect m_Left, m_Right;
		bool m_TwoColumns;
		static CSoundLayout Calculate(CUIRect View)
		{
			const float Gap = std::min(24.f, View.w * .04f);
			const bool Wide = View.w >= 540;
			const float Left = Wide ? (View.w - Gap) * .44f : View.w;
			return {CGeneralLayout::ContentRegion({View.x, View.y, Left, View.h}),
				CGeneralLayout::ContentRegion({View.x + (Wide ? Left + Gap : 0), View.y, Wide ? View.w - Left - Gap : Left, View.h}), Wide};
		}
		template<typename FMeasure, typename FDraw, typename FHeading>
		float Compose(bool Advanced, FMeasure Measure, FDraw Draw, FHeading Heading) const
		{
			using C = ESoundControl;
			float Y = m_Left.y;
			const auto Section = [&](int Name, CUIRect Column) {
				const float H = std::max(18.f, Measure(Name, Column.w) + 2);
				Heading(Name, CUIRect{Column.x, Y, Column.w, H});
				Y += H + 4;
			};
			const auto Row = [&](C Control, CUIRect Column) {
				float Width = Column.w;
				if(!SoundSlider(Control))
					Width -= LucentStyle::Checkbox::Size + LucentStyle::Style::LabelGap;
				else
					Width -= std::min(CGeneralLayout::ValueWidth, Width * .4f) + CGeneralLayout::HeaderGap;
				const float H = SoundSlider(Control) ? 28 + std::max(18.f, Measure(Control, std::max(1.f, Width)) + 4) : std::max(28.f, Measure(Control, std::max(1.f, Width)) + 8);
				Draw(Control, CUIRect{Column.x, Y, Column.w, H});
				Y += H + 2;
			};
			Section(Advanced ? 2 : 0, m_Left);
			if(Advanced)
			{
				Row(C::MUTE, m_Left);
				Row(C::LONG_PAIN, m_Left);
			}
			else
			{
				Row(C::ENABLE, m_Left);
				Row(C::GAME, m_Left);
				Row(C::GUN, m_Left);
				Row(C::MUSIC, m_Left);
			}
			const float LeftBottom = Y;
			Y = m_TwoColumns ? m_Right.y : Y + 14;
			Section(Advanced ? 3 : 1, m_Right);
			if(Advanced)
			{
				Row(C::SERVER, m_Right);
				Row(C::CHAT, m_Right);
				Row(C::TEAM_CHAT, m_Right);
				Row(C::HIGHLIGHT, m_Right);
			}
			else
			{
				Row(C::VOLUME, m_Right);
				Row(C::GAME_VOLUME, m_Right);
				Row(C::CHAT_VOLUME, m_Right);
				Row(C::MAP_VOLUME, m_Right);
				Row(C::MUSIC_VOLUME, m_Right);
			}
			return std::max(LeftBottom, Y) - m_Left.y + 6;
		}
	};
	using FSoundControl = std::function<void(ESoundControl, CUIRect, bool, const CUIRect *)>;
	void RenderSound(CUi *pUi, ITextRender *pTextRender, CUIRect View, const CSoundState &State, const FSoundControl &RenderControl);
	void LeaveSound(CUi *pUi);
	const char *SoundLabel(ESoundControl Control);
}
#endif
