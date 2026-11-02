#ifndef GAME_CLIENT_LUCENT_SETTINGS_WORKSPACE_H
#define GAME_CLIENT_LUCENT_SETTINGS_WORKSPACE_H

namespace LucentSettings
{
	inline constexpr int WorkspaceDDNet = 0, WorkspaceLucent = 1;
	enum EPage
	{
		PAGE_CURSOR,
		PAGE_WEAPONS,
		PAGE_CAMERA,
		PAGE_FONTS
	};
	struct CPageDescription
	{
		int m_Id;
		const char *m_pLabel;
	};
	// Stable persisted IDs; append sections without reinterpreting old configs.
	inline constexpr CPageDescription Pages[] = {{PAGE_CURSOR, "Cursor"}, {PAGE_WEAPONS, "Weapons"}, {PAGE_CAMERA, "Camera"}, {PAGE_FONTS, "Fonts"}};
	inline constexpr int PageCount = sizeof(Pages) / sizeof(Pages[0]);
	inline int PageIndex(int Id)
	{
		for(int i = 0; i < PageCount; ++i)
			if(Pages[i].m_Id == Id)
				return i;
		return 0;
	}
	inline void NormalizeWorkspace(int &Workspace, int &LucentPage, int &DDNetPage)
	{
		if(Workspace != WorkspaceDDNet && Workspace != WorkspaceLucent)
			Workspace = WorkspaceDDNet;
		LucentPage = Pages[PageIndex(LucentPage)].m_Id;
		if(DDNetPage < 0 || DDNetPage > 10)
			DDNetPage = 0;
	}
}
#endif
