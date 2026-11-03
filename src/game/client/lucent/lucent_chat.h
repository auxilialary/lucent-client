#ifndef GAME_CLIENT_LUCENT_LUCENT_CHAT_H
#define GAME_CLIENT_LUCENT_LUCENT_CHAT_H

#include <base/vmath.h>

class CLucentChat
{
public:
	typedef vec2 (*FChatPositionProvider)(vec2 DefaultOffset, void *pUserData);

	static void SetOffset(vec2 Offset);
	static vec2 GetOffset();

	static void SetMessagesOffset(vec2 Offset);
	static vec2 GetMessagesOffset();

	static void SetInputOffset(vec2 Offset);
	static vec2 GetInputOffset();

	static void SetPositionProvider(FChatPositionProvider pfnProvider, void *pUserData = nullptr);

	static vec2 ComputeMessagesOffset();
	static vec2 ComputeInputOffset();

	static void Reset();
};

#endif
