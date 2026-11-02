#ifndef GAME_CLIENT_LUCENT_SETTINGS_SKINS_H
#define GAME_CLIENT_LUCENT_SETTINGS_SKINS_H
#include "mode.h"

#include <functional>
#include <string_view>
class CUi;
class ITextRender;
namespace LucentSettings
{
	inline constexpr std::array<const char *, 3> SkinPrefixes = {"", "kitty", "santa"};
	inline int SkinPrefixIndex(std::string_view Prefix)
	{
		for(size_t Index = 0; Index < SkinPrefixes.size(); ++Index)
			if(Prefix == SkinPrefixes[Index])
				return int(Index);
		return -1;
	}

	enum class ESkinControl
	{
		PREVIEW,
		NAME,
		RANDOM,
		CUSTOM_COLORS,
		BODY,
		FEET,
		EYES,
		PREFIX,
		EMOTIONS,
		STATUS,
		COOLDOWN,
		DOWNLOAD,
		COMMUNITY,
		VANILLA,
		FAT,
		QUALITY,
		LOADED_MAX,
		COMMUNITY_URL,
		SOURCE_INFO,
		RESET_SOURCE
	};
	struct CSkinManagerLayout
	{
		CUIRect m_Profile, m_Gallery;
		bool m_TwoColumns;
		static CSkinManagerLayout Calculate(CUIRect View)
		{
			View.w = std::max(0.f, View.w);
			View.h = std::max(0.f, View.h);
			const bool Wide = View.w >= 700;
			CUIRect Profile, Gallery;
			if(Wide)
			{
				View.VSplitLeft(View.w * .32f, &Profile, &Gallery);
				Gallery.VSplitLeft(std::min(24.f, Gallery.w), nullptr, &Gallery);
			}
			else
			{
				View.HSplitTop(std::min(300.f, View.h * .55f), &Profile, &Gallery);
				Gallery.HSplitTop(std::min(12.f, Gallery.h), nullptr, &Gallery);
			}
			return {SettingsContentRegion(Profile), SettingsContentRegion(Gallery), Wide};
		}
		static CSkinManagerLayout Advanced(CUIRect View)
		{
			View.w = std::min(std::max(0.f, View.w), std::max(700.f, std::max(0.f, View.h) * 1.8f));
			auto Layout = Calculate(View);
			if(Layout.m_TwoColumns)
				Layout.m_Gallery.w = std::min(Layout.m_Gallery.w, Layout.m_Profile.w * 1.5f);
			else
				Layout.m_Profile.w *= .9f;
			return Layout;
		}
	};
	using FSkinControl = std::function<void(ESkinControl, CUIRect)>;
	using FSkinContent = std::function<void(CUIRect)>;
	void RenderSkinManager(CUi *pUi, ITextRender *pTextRender, CUIRect View, bool &Dummy, bool CustomColors, const FSkinControl &Control, const FSkinContent &Gallery, const FSkinContent &Seven = {});
	void LeaveSkinManager(CUi *pUi);
	bool SkinGalleryKeyboardFocused();
	void OpenSkinColorPopup(CUi *pUi, ITextRender *pTextRender, CUIRect Anchor, unsigned *pColor, bool Alpha, float Darkest, const std::function<void()> &Changed);
}
#endif
