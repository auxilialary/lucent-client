#ifndef GAME_CLIENT_LUCENT_GAME_PING_RENDERER_H
#define GAME_CLIENT_LUCENT_GAME_PING_RENDERER_H
#include "ping.h"
#include "ping_sender.h"
class IGraphics;
class ITextRender;
namespace LucentPing
{
	void Render(IGraphics *pGraphics, ITextRender *pText, CRenderTools *pRenderTools, const CSenderAppearances &Senders, const CState &State, double Now, vec2 WorldMin, vec2 WorldSize, vec2 DistanceOrigin, bool Animate);
}
#endif
