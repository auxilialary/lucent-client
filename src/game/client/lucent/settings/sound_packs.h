#ifndef GAME_CLIENT_LUCENT_SETTINGS_SOUND_PACKS_H
#define GAME_CLIENT_LUCENT_SETTINGS_SOUND_PACKS_H
#include <game/client/ui_rect.h>

#include <algorithm>
class CUi;
class ITextRender;
class IStorage;
class ISound;
class CSounds;
namespace LucentSettings
{
	struct CSoundPackLayout
	{
		CUIRect m_Left, m_Right;
		bool m_TwoColumns;
		static CSoundPackLayout Calculate(CUIRect View)
		{
			const bool Wide = View.w >= 620;
			const float Left = Wide ? std::clamp(View.w * .30f, 220.f, 290.f) : View.w;
			return {{View.x, View.y, Left, View.h}, {View.x + (Wide ? Left + 24 : 0), View.y, Wide ? View.w - Left - 24 : View.w, View.h}, Wide};
		}
		static constexpr float Gap = 8.f, RowHeight = 40.f;
		static bool ShowCategoryCount(int Count) { return Count > 0; }
		static float ReviewHeight(size_t Count) { return std::min(3.f, float(Count)) * 32.f; }
		static float CompareHeight(size_t Count) { return 232.f + (Count ? ReviewHeight(Count) + 12.f : 0.f); }
		static CUIRect CategoryButton(CUIRect Row, int Column, int Columns)
		{
			const float Width = std::max(0.f, (Row.w - Gap * (Columns - 1)) / Columns);
			return {Row.x + Column * (Width + Gap), Row.y, Width, Row.h - Gap};
		}
		static int CategoryColumns(float Width) { return std::clamp(int(Width / 112), 1, 5); }
	};
	void RenderSoundPacks(CUi *pUi, ITextRender *pTextRender, IStorage *pStorage, ISound *pSound, CSounds *pSounds, CUIRect View);
	void LeaveSoundPacks(CUi *pUi);
}
#endif
