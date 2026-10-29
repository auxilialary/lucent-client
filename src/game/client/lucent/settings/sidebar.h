#ifndef GAME_CLIENT_LUCENT_SETTINGS_SIDEBAR_H
#define GAME_CLIENT_LUCENT_SETTINGS_SIDEBAR_H

#include "navigation.h"

#include <game/client/ui.h>

namespace LucentSettings
{
	class CSidebar
	{
		CNavigation m_Navigation;
		CButtonContainer m_aButtons[CategoryCount];
		CButtonContainer m_BackButton;
		CLayout m_Layout;
		bool m_Interactive = false;
		bool m_BackRequested = false;
		bool Owns(const void *pId) const;

	public:
		void Reset()
		{
			m_Navigation.Reset();
			m_BackRequested = false;
		}
		bool BackRequested() const { return m_BackRequested; }
		// Resolve navigation before the existing page; paint the overlay afterwards.
		CLayout Begin(CUi *pUi, CUIRect View, int &Selected, bool Blocked);
		bool BlocksContent(CUi *pUi) const;
		void Render(CUi *pUi, ITextRender *pTextRender, int Selected, const char *const *ppLabels);
	};
}
#endif
