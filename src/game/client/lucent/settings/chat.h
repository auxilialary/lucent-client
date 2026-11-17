#ifndef GAME_CLIENT_LUCENT_SETTINGS_CHAT_H
#define GAME_CLIENT_LUCENT_SETTINGS_CHAT_H

#include <game/client/ui_rect.h>

class CGameClient;
class CUi;
class ITextRender;

namespace LucentSettings
{
	void RenderChat(CGameClient *pGame, CUi *pUi, ITextRender *pTextRender, CUIRect View);
	void LeaveChat(CUi *pUi);
}
#endif
