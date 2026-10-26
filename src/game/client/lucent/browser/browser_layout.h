#ifndef GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#define GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#include <game/client/ui_rect.h>

#include <algorithm>

// Shared scene anchors: artwork, title and interaction geometry use one space.
struct CLucentBrowserSceneLayout
{
	CUIRect m_Artwork;
	float m_TitleY;
	static float ListWidth(const CUIRect &Screen) { return (Screen.w - 36) * .38f; }
	static CUIRect SceneBounds(const CUIRect &Screen)
	{
		const float Gap = std::clamp(Screen.w * .02f, 12.f, 24.f);
		const float Left = Screen.x + 18 + ListWidth(Screen) + Gap;
		return {Left, Screen.y, Screen.x + Screen.w - 24 - Left, Screen.h};
	}
	static CLucentBrowserSceneLayout Measure(const CUIRect &Screen, float /*Context*/, float /*Players*/)
	{
		const auto Scene = SceneBounds(Screen);
		// UI height tracks the viewport: 156 units is 281 pixels at 1080p.
		// Reserve the same header and contextual space in all three modes.
		const CUIRect Artwork = {Scene.x, Screen.y + Screen.h * .12f, Scene.w, Screen.h * .26f};
		return {Artwork, Artwork.y + Artwork.h + 12};
	}
};

// One measured scroll region, including complete country/type grids.
struct CLucentBrowserFilterLayout
{
	bool m_Wide;
	float m_GridHeight;
	float m_ContentHeight;
	static constexpr float WideWidth = 620;
	static constexpr float SectionDividerGap = 10, SectionBottomGap = 7, TabGridGap = 7;
	static float WorkspaceWidth(float Width) { return std::min(820.f, Width); }
	static float ColumnGap(float Width) { return std::clamp(Width * .045f, 20.f, 36.f); }
	static CUIRect Column(const CUIRect &Workspace, int Index)
	{
		const float Gap = ColumnGap(Workspace.w);
		const float Width = (Workspace.w - Gap) / 2;
		return {Workspace.x + Index * (Width + Gap), Workspace.y, Width, Workspace.h};
	}
	static int QuickColumns(float Width, float Minimum = 185) { return Width >= 2 * Minimum + 8 ? 2 : 1; }
	static float GroupHeight(float Width, int Count, float Minimum = 185)
	{
		return 28 + ((Count + QuickColumns(Width, Minimum) - 1) / QuickColumns(Width, Minimum)) * 25.f;
	}
	static float CheckboxHeight(float Width, float Minimum = 185)
	{
		if(Width >= WideWidth)
		{
			const float GroupWidth = (Width - ColumnGap(Width)) / 2;
			return std::max(GroupHeight(GroupWidth, 4, Minimum), GroupHeight(GroupWidth, 5, Minimum));
		}
		return GroupHeight(Width, 4, Minimum) + 8 + GroupHeight(Width, 5, Minimum);
	}
	static float InputHeight(float /*Width*/) { return 36.f; }
	static CUIRect ChipBounds(const CUIRect &Row, int Index)
	{
		const float Width = (Row.w - 12) / 3;
		return {Row.x + Index * (Width + 6), Row.y + 10, Width, 24};
	}
	static float GridWidth(float Width, int Countries, bool CountriesTab, float Minimum = 240)
	{
		const float ContentWidth = CountriesTab ? (Countries > 8 ? 5 : 4) * 44.f : 300.f;
		return std::min(Width, std::max(Minimum, ContentWidth));
	}
	static CLucentBrowserFilterLayout Measure(float Width, int Countries, int Types, bool CountriesTab, bool HasGrid, bool /*Ranks*/, bool /*Finished*/, float Minimum = 185)
	{
		Width = WorkspaceWidth(Width);
		const bool Wide = Width >= WideWidth;
		const int Columns = Countries > 8 ? 5 : 4;
		const float Grid = !HasGrid ? 0.f : CountriesTab ? ((Countries + Columns - 1) / Columns) * 22.f :
								   ((Types + 2) / 3) * 17.f;
		const float Height = CheckboxHeight(Width, Minimum) + 26 + InputHeight(Width) + 10 + 24.5f + SectionDividerGap + SectionBottomGap + 21 + (HasGrid ? 6 + 19 + TabGridGap + Grid : 0) + 8;
		return {Wide, Grid, Height};
	}
};
#endif
