#ifndef GAME_CLIENT_LUCENT_SETTINGS_SKIN_PREVIEW_H
#define GAME_CLIENT_LUCENT_SETTINGS_SKIN_PREVIEW_H
#include <game/client/ui_rect.h>
class CRenderTools;
class CTeeRenderInfo;
namespace LucentSettings
{
	void RenderSkinPreview(CRenderTools *pRenderTools, const CTeeRenderInfo &Info, CUIRect Rect, int Emote);
}
#endif
