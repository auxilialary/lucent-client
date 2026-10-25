#ifndef GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#define GAME_CLIENT_LUCENT_BROWSER_LAYOUT_H
#include <algorithm>

// One measured scroll region, including complete country/type grids.
struct CLucentBrowserFilterLayout
{
	bool m_Wide;
	float m_GridHeight;
	float m_ContentHeight;
	static CLucentBrowserFilterLayout Measure(float Width, int Countries, int Types, bool CountriesTab, bool HasGrid, bool Ranks, bool Finished)
	{
		const bool Wide = Width >= 720;
		const int Columns = Countries > 8 ? 5 : 4;
		const float Grid = !HasGrid ? 0.f : CountriesTab ? ((Countries + Columns - 1) / Columns) * 22.f :
								   ((Types + 2) / 3) * 17.f;
		const float Height = Wide ? 36.f + std::max(350.f, 35.f + Grid) : 21.f + 10.f + 11 * 21.f + 15.f + 48.f + 24.f + (Ranks && !Finished ? 28.f : 0.f) + (Ranks ? 21.f * (1 + Finished) : 0.f) + (HasGrid ? 25.f + Grid : 0.f) + 3.f + 21.f;
		return {Wide, Grid, Height};
	}
};
#endif
