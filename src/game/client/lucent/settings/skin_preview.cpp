#include "skin_preview.h"

#include <game/client/render.h>
namespace LucentSettings
{
	void RenderSkinPreview(CRenderTools *pRenderTools, const CTeeRenderInfo &Info, CUIRect Rect, int Emote)
	{
		CTeeRenderInfo Preview = Info;
		Preview.m_Size = std::min(48.f, std::min(Rect.h, Rect.w));
		if(Preview.m_Size > 0)
			pRenderTools->RenderIdleTeeAtCenter(&Preview, Rect.Center(), Emote, vec2(1, 0), CUIRect::DrawOpacity());
	}
}
