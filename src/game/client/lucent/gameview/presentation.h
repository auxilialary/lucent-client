#ifndef GAME_CLIENT_LUCENT_GAMEVIEW_PRESENTATION_H
#define GAME_CLIENT_LUCENT_GAMEVIEW_PRESENTATION_H
#include <base/vmath.h>

#include <engine/shared/config.h>
namespace LucentGameView
{
	inline float CursorScale(int Percent) { return std::clamp(Percent, 5, 500) / 100.f; }
	// Fixed stack tables: no allocations in player rendering. Protocol weapon ordering
	// is shared by the existing DDNet 0.6 and 0.7 renderer.
	inline vec2 WeaponOffset(int Weapon)
	{
		const int aX[] = {g_Config.m_LucentWeaponHammerX, g_Config.m_LucentWeaponGunX, g_Config.m_LucentWeaponShotgunX, g_Config.m_LucentWeaponGrenadeX, g_Config.m_LucentWeaponLaserX, g_Config.m_LucentWeaponNinjaX};
		const int aY[] = {g_Config.m_LucentWeaponHammerY, g_Config.m_LucentWeaponGunY, g_Config.m_LucentWeaponShotgunY, g_Config.m_LucentWeaponGrenadeY, g_Config.m_LucentWeaponLaserY, g_Config.m_LucentWeaponNinjaY};
		return Weapon >= 0 && Weapon < 6 ? vec2(aX[Weapon], aY[Weapon]) : vec2(0, 0);
	}
}
#endif
