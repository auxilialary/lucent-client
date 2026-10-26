#ifndef GAME_CLIENT_LUCENT_CONSOLE_PRESENTATION_H
#define GAME_CLIENT_LUCENT_CONSOLE_PRESENTATION_H

#include "filters.h"
#include "layout.h"

#include <base/str.h>

#include <game/client/lucent/style/tokens.h>
#include <game/client/ui.h>
#include <game/localization.h>

namespace LucentConsole
{
	void RenderEmptyHistory(ITextRender *pText, EEmptyHistory State, const CUIRect &Bounds);
	ColorRGBA SourceColor(EConsoleLogGroup Group);
	struct CMetadataTooltip
	{
		char m_aSource[32] = {};
		CUIRect m_Rect{};
		void Reset() { m_aSource[0] = 0; }
		void Render(CUi *pUi, ITextRender *pText, const CUIRect &Bounds) const;
	};

	void RenderMetadata(ITextRender *pText, CMetadataTooltip &Tooltip, const char *pTimestamp, const char *pSource, EConsoleLogGroup Group, int Severity, float TimestampWidth, float SourceWidth, float Y, float Height, int SearchMatch, vec2 Mouse);
}
#endif
