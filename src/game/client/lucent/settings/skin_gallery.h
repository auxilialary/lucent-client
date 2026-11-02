#ifndef GAME_CLIENT_LUCENT_SETTINGS_SKIN_GALLERY_H
#define GAME_CLIENT_LUCENT_SETTINGS_SKIN_GALLERY_H
#include <game/client/ui_rect.h>

#include <algorithm>
#include <functional>
class CUi;
class ITextRender;
namespace LucentSettings
{
	struct CSkinGalleryItem
	{
		const char *m_pName;
		const void *m_pId;
		const void *m_pFavoriteId;
		bool m_Selected, m_Favorite;
	};
	using FSkinItem = std::function<CSkinGalleryItem(int)>;
	using FSkinPaint = std::function<void(int, CUIRect)>;
	using FSkinAction = std::function<void(int)>;
	struct CSkinGalleryGeometry
	{
		static constexpr float RowHeight = 80;
		static CUIRect Intersect(CUIRect A, CUIRect B)
		{
			const float X = std::max(A.x, B.x), Y = std::max(A.y, B.y);
			return {X, Y, std::max(0.f, std::min(A.x + A.w, B.x + B.w) - X), std::max(0.f, std::min(A.y + A.h, B.y + B.h) - Y)};
		}
		static CUIRect Clip(CUIRect Rect, float Pixel)
		{
			Pixel = std::max(.001f, Pixel);
			const float X = std::min(std::ceil(Rect.x / Pixel) * Pixel, Rect.x + std::max(0.f, Rect.w)), Y = std::min(std::ceil(Rect.y / Pixel) * Pixel, Rect.y + std::max(0.f, Rect.h));
			return {X, Y, std::max(0.f, std::floor((Rect.x + Rect.w) / Pixel) * Pixel - X), std::max(0.f, std::floor((Rect.y + Rect.h) / Pixel) * Pixel - Y)};
		}
		static CUIRect SplitFooter(CUIRect &View)
		{
			CUIRect Footer;
			View.HSplitBottom(std::min(28.f, std::max(0.f, View.h)), &View, &Footer);
			View.HSplitBottom(std::min(8.f, std::max(0.f, View.h)), &View, nullptr);
			return Footer;
		}
		static int Columns(float Width) { return std::max(1, int(std::max(0.f, Width - 8) / 150)); }
	};
	void RenderSkinGallery(CUi *pUi, ITextRender *pTextRender, CUIRect View, int Count, bool ScrollToSelected, const FSkinItem &Item, const FSkinPaint &Paint, const FSkinAction &Select, const FSkinAction &Favorite, bool KeyboardFocus = false);
}
#endif
