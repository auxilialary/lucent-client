#ifndef GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#define GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#include <game/client/ui_rect.h>

#include <algorithm>

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
	static constexpr float WideWidth = 620;
	static float WorkspaceWidth(float Width) { return std::min(820.f, Width); }
	static float ColumnGap(float Width) { return std::clamp(Width * .045f, 20.f, 36.f); }
	static CUIRect Column(const CUIRect &Workspace, int Index)
	{
		const float Gap = ColumnGap(Workspace.w);
		const float Width = (Workspace.w - Gap) / 2;
		return {Workspace.x + Index * (Width + Gap), Workspace.y, Width, Workspace.h};
	}
	static int QuickColumns(float Width) { return std::clamp(int((Width - 14) / 185), 1, 3); }
	static float QuickHeight(float Width) { return ((5 + QuickColumns(Width) - 1) / QuickColumns(Width)) * 25.f + 8; }
	static CLucentBrowserFilterLayout Measure(float Width, int Countries, int Types, bool CountriesTab, bool HasGrid, bool Ranks, bool Finished)
	{
		Width = WorkspaceWidth(Width);
		Width -= 12; // Forced 8-unit scrollbar and 4-unit content inset.
		const bool Wide = Width >= WideWidth;
		const int Columns = Countries > 8 ? 5 : 4;
		const float Grid = !HasGrid ? 0.f : CountriesTab ? ((Countries + Columns - 1) / Columns) * 22.f :
								   ((Types + 2) / 3) * 17.f;
		// Heights are the sum of consumed controls, not minimum section heights.
		constexpr float Heading = 8 + 16 + .5f;
		constexpr float Left = 5 * Heading + 18 + 7 * 21;
		const float Right = 2 * Heading + (Ranks ? 21 + (Finished ? 21 : 28) : 0) + (HasGrid ? 6 + 19 + Grid : 0);
		const float Height = QuickHeight(Width) + 10 + (Wide ? std::max(Left, Right) : Left + Right);
		return {Wide, Grid, Height};
	}
};
#endif
