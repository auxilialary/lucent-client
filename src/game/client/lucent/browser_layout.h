#ifndef GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#define GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#include <algorithm>
#include <game/client/ui_rect.h>

// Shared scene anchors: artwork, title and interaction geometry use one space.
struct CLucentBrowserSceneLayout
{
	CUIRect m_Artwork;
	float m_TitleY;
	static CLucentBrowserSceneLayout Measure(const CUIRect &Screen, float Context, float Players)
	{
		const float Left = Screen.x + 18 + (Screen.w - 36) * .38f + Screen.w * .05f;
		const float Width = Screen.x + Screen.w - 24 - Left;
		const CUIRect Preview = {Left, Screen.y + Screen.h * .15f, Width, Screen.h * .51f};
		const CUIRect Supporting = {Left + Width * (.56f + .08f * Players), Screen.y + Screen.h * .13f, Width * (.44f - .08f * Players), Screen.h * .19f};
		const auto Blend = [Context](float A, float B) { return A + (B - A) * Context; };
		return {{Blend(Preview.x, Supporting.x), Blend(Preview.y, Supporting.y), Blend(Preview.w, Supporting.w), Blend(Preview.h, Supporting.h)}, Blend(Screen.y + Screen.h * .62f, Screen.y + Screen.h * .32f)};
	}
};

// One measured scroll region, including complete country/type grids.
struct CLucentBrowserFilterLayout
{
	bool m_Wide;
	float m_GridHeight;
	float m_ContentHeight;
	static int QuickColumns(float Width) { return std::clamp(int((Width - 14) / 185), 1, 3); }
	static float QuickHeight(float Width) { return ((5 + QuickColumns(Width) - 1) / QuickColumns(Width)) * 25.f + 8; }
	static CLucentBrowserFilterLayout Measure(float Width, int Countries, int Types, bool CountriesTab, bool HasGrid, bool Ranks, bool Finished)
	{
		const bool Wide = Width >= 720;
		const int Columns = Countries > 8 ? 5 : 4;
		const float Grid = !HasGrid ? 0.f : CountriesTab ? ((Countries + Columns - 1) / Columns) * 22.f :
								   ((Types + 2) / 3) * 17.f;
		const float Height = Wide ? QuickHeight(Width) + std::max(350.f, 59.f + Grid) : QuickHeight(Width) + 392.f + (Ranks ? 21.f + (Finished ? 21.f : 28.f) : 0.f) + (HasGrid ? 25.f + Grid : 0.f);
		return {Wide, Grid, Height};
	}
};
#endif
