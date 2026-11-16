#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_PANEL_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_PANEL_H
#include <game/client/lucent/hud/panel_geometry.h>
namespace LucentScoreboard
{
	// Adjacent strips share only side borders; top/bottom belong to the whole group.
	struct CGroupStrip
	{
		LucentHud::CRect m_Outer{}, m_Inner{};
		static CGroupStrip Calculate(LucentHud::CRect Rect, float Edge, bool First, bool Last)
		{
			Edge = std::clamp(Edge, 0.f, std::min(Rect.w, Rect.h) / 2);
			return {Rect, {Rect.x + Edge, Rect.y + (First ? Edge : 0.f), std::max(0.f, Rect.w - 2 * Edge), std::max(0.f, Rect.h - (First ? Edge : 0.f) - (Last ? Edge : 0.f))}};
		}
	};
	// Cache the physical-pixel coverage mesh until bounds or framebuffer mapping changes.
	struct CPanelCache
	{
		LucentHud::CPanelGeometry m_Geometry{};
		LucentHud::CRect m_Rect{};
		LucentHud::CPixelGrid m_Grid{{0, 0}, {1, 1}};
		bool m_Valid = false, m_Capsule = false;
		const LucentHud::CPanelGeometry &Get(const LucentHud::CRect &Rect, const LucentHud::CPixelGrid &Grid, bool Capsule = false)
		{
			if(!m_Valid || m_Capsule != Capsule || Rect.x != m_Rect.x || Rect.y != m_Rect.y || Rect.w != m_Rect.w || Rect.h != m_Rect.h ||
				Grid.m_Origin != m_Grid.m_Origin || Grid.m_Step != m_Grid.m_Step)
			{
				m_Rect = Rect;
				m_Grid = Grid;
				m_Capsule = Capsule;
				m_Geometry = LucentHud::CPanelGeometry::Calculate(Rect, Capsule ? std::max(1.f, Rect.h / (2 * LucentHud::Radius)) : 1.f, Grid);
				m_Valid = true;
			}
			return m_Geometry;
		}
	};
}
#endif
