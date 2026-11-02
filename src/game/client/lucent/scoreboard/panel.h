#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_PANEL_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_PANEL_H
#include <game/client/lucent/hud/panel_geometry.h>
namespace LucentScoreboard
{
	// Cache the physical-pixel coverage mesh until bounds or framebuffer mapping changes.
	struct CPanelCache
	{
		LucentHud::CPanelGeometry m_Geometry{};
		LucentHud::CRect m_Rect{};
		LucentHud::CPixelGrid m_Grid{{0, 0}, {1, 1}};
		bool m_Valid = false;
		const LucentHud::CPanelGeometry &Get(const LucentHud::CRect &Rect, const LucentHud::CPixelGrid &Grid)
		{
			if(!m_Valid || Rect.x != m_Rect.x || Rect.y != m_Rect.y || Rect.w != m_Rect.w || Rect.h != m_Rect.h ||
				Grid.m_Origin != m_Grid.m_Origin || Grid.m_Step != m_Grid.m_Step)
			{
				m_Rect = Rect;
				m_Grid = Grid;
				m_Geometry = LucentHud::CPanelGeometry::Calculate(Rect, 1.f, Grid);
				m_Valid = true;
			}
			return m_Geometry;
		}
	};
}
#endif
