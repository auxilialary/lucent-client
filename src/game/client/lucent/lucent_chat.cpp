#include "lucent_chat.h"

static vec2 s_MessagesOffset = vec2(0.0f, 0.0f);
static vec2 s_InputOffset = vec2(0.0f, 0.0f);
static CLucentChat::FChatPositionProvider s_pfnPositionProvider = nullptr;
static void *s_pPositionProviderUserData = nullptr;

void CLucentChat::SetOffset(vec2 Offset)
{
	s_MessagesOffset = Offset;
	s_InputOffset = Offset;
}

vec2 CLucentChat::GetOffset()
{
	return s_MessagesOffset;
}

void CLucentChat::SetMessagesOffset(vec2 Offset)
{
	s_MessagesOffset = Offset;
}

vec2 CLucentChat::GetMessagesOffset()
{
	return s_MessagesOffset;
}

void CLucentChat::SetInputOffset(vec2 Offset)
{
	s_InputOffset = Offset;
}

vec2 CLucentChat::GetInputOffset()
{
	return s_InputOffset;
}

void CLucentChat::SetPositionProvider(FChatPositionProvider pfnProvider, void *pUserData)
{
	s_pfnPositionProvider = pfnProvider;
	s_pPositionProviderUserData = pUserData;
}

vec2 CLucentChat::ComputeMessagesOffset()
{
	if(s_pfnPositionProvider)
		return s_pfnPositionProvider(s_MessagesOffset, s_pPositionProviderUserData);

	return s_MessagesOffset;
}

vec2 CLucentChat::ComputeInputOffset()
{
	if(s_pfnPositionProvider)
		return s_pfnPositionProvider(s_InputOffset, s_pPositionProviderUserData);

	return s_InputOffset;
}

void CLucentChat::Reset()
{
	s_MessagesOffset = vec2(0.0f, 0.0f);
	s_InputOffset = vec2(0.0f, 0.0f);
	s_pfnPositionProvider = nullptr;
	s_pPositionProviderUserData = nullptr;
}
