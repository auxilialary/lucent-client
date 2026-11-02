#ifndef GAME_CLIENT_LUCENT_HUD_WEAPON_SCALE_H
#define GAME_CLIENT_LUCENT_HUD_WEAPON_SCALE_H

#include "layout.h"

#include <game/client/lucent/motion/transition.h>

namespace LucentHud
{
	inline constexpr float WeaponScaleDuration = .20f;
	struct CWeaponScale
	{
		std::array<LucentStyle::Motion::CTransition, 6> m_aTransitions{};
		unsigned m_Inventory = 0;
		int m_ClientId = -1;

		void Update(const CWeapons &Weapons, int Active, int ClientId, double Now)
		{
			unsigned Inventory = 0;
			for(int Slot = 0; Slot < Weapons.m_Count; ++Slot)
				Inventory |= 1u << Weapons.m_aIds[Slot];
			const bool Initialize = ClientId != m_ClientId || m_Inventory == 0;
			for(int Weapon = 0; Weapon < (int)m_aTransitions.size(); ++Weapon)
			{
				auto &Transition = m_aTransitions[Weapon];
				if(!(Inventory & (1u << Weapon)))
				{
					Transition = {};
					continue;
				}
				// New inventory entries start at inactive size; an initial view starts
				// settled. Removed entries cannot resurrect a stale ninja/weapon scale.
				if(Initialize || !(m_Inventory & (1u << Weapon)))
					Transition = {};
				Transition.Update(Weapon == Active, Now, !Initialize, WeaponScaleDuration);
			}
			m_ClientId = ClientId;
			m_Inventory = Inventory;
		}

		float Scale(int Weapon) const
		{
			return LucentStyle::Motion::Lerp(.75f, 1.f, m_aTransitions[Weapon].m_Value);
		}
	};
}
#endif
