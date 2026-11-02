#ifndef GAME_CLIENT_LUCENT_SETTINGS_FONTS_H
#define GAME_CLIENT_LUCENT_SETTINGS_FONTS_H
#include <game/client/ui_rect.h>
class CUi;
class ITextRender;
class IStorage;
namespace LucentSettings
{
	void RenderFonts(CUi *pUi, ITextRender *pTextRender, IStorage *pStorage, CUIRect View);
	void LeaveFonts(CUi *pUi);
}
#endif
