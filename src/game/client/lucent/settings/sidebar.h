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
		CButtonContainer m_aWorkspaceButtons[2];
		IGraphics::CTextureHandle m_aWorkspaceTextures[2];
		CLayout m_Layout;
		int m_Count = CategoryCount;
		bool m_Interactive = false;
		bool m_BackRequested = false;
		bool Owns(const void *pId) const;

	public:
		void Init(IGraphics *pGraphics);
		void Shutdown(IGraphics *pGraphics);
		void WorkspaceInput(CUi *pUi, CUIRect View, int &Workspace, bool Blocked);
		void Reset()
		{
			m_Navigation.Reset();
			m_BackRequested = false;
		}
		bool BackRequested() const { return m_BackRequested; }
		// Resolve navigation before the existing page; paint the overlay afterwards.
		CLayout Begin(CUi *pUi, CUIRect View, int &Selected, bool Blocked, int Count = CategoryCount, bool NavigationBlocked = false);
		bool BlocksContent(CUi *pUi) const;
		void Render(CUi *pUi, ITextRender *pTextRender, int Selected, const char *const *ppLabels, const char *const *ppIcons = nullptr, int Workspace = 0, float CategoryOpacity = 1);
	};
}
#endif
