#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_IDENTITY_TEXT_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_IDENTITY_TEXT_H
#include "layout.h"

#include <game/client/cached_text.h>
#include <game/client/lucent/hud/panel_geometry.h>

#include <limits>

namespace LucentScoreboard
{
	// Positive width preserves exactly the advances used by bounded containers.
	// Measure at the actual font size and current framebuffer mapping, not at 12
	// followed by scaling: the renderer quantizes font sizes to physical pixels.
	template<typename TRenderer>
	float IdentityTextWidth(TRenderer *pRenderer, float Font, const char *pText)
	{
		return pRenderer->TextWidth(Font, pText, -1, std::numeric_limits<float>::max(), TEXTFLAG_DISALLOW_NEWLINE | TEXTFLAG_STOP_AT_END);
	}

	template<typename TRenderer>
	CIdentity PrepareIdentityText(TRenderer *pRenderer, CCachedText &NameCache, CCachedText &ClanCache,
		const char *pName, const char *pClan, float Width, float Font, float Row, float Ready, vec2 Metrics)
	{
		const float Name = IdentityTextWidth(pRenderer, Font, pName);
		const float Pixel = 1 / std::max(.01f, Metrics.x);
		const float Padding = std::round(LucentStyle::Scoreboard::ClanPadding * Font / 12 / Pixel) * Pixel;
		CTextCursor Measure;
		Measure.m_FontSize = Font * LucentStyle::Scoreboard::ClanFontScale;
		Measure.m_LineWidth = std::numeric_limits<float>::max();
		Measure.m_Flags = TEXTFLAG_DISALLOW_NEWLINE | TEXTFLAG_STOP_AT_END;
		Measure.m_TrackInk = true;
		if(pClan[0])
			pRenderer->TextEx(&Measure, pClan);
		const float InkWidth = Measure.m_Ink.Width();
		const float FullWidth = Measure.m_LongestLineWidth;
		// One pixel accommodates snapping the pill's left edge outwards; both
		// visible text edges then share the same physically rounded padding.
		auto Identity = CIdentity::CalculateMeasured(std::max(0.f, Width - (pClan[0] ? Pixel : 0)), Font, Row, Name,
			std::ceil(InkWidth / Pixel) * Pixel, pClan[0] != 0, Ready, Padding);
		if(pClan[0])
		{
			const float Budget = std::max(0.f, Identity.m_ClanWidth - 2 * Padding);
			float Limit = InkWidth <= Budget ? std::max(.01f, FullWidth) : std::max(.01f, Budget);
			const auto Flags = pRenderer->GetRenderFlags();
			// Only this container permits fractional translation: re-snapping its
			// centered origin independently would reintroduce asymmetric padding.
			pRenderer->SetRenderFlags(Flags | TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);
			for(;;)
			{
				ClanCache.Update(pRenderer, pClan, Identity.m_ClanFont, Limit,
					InkWidth <= Budget ? SingleLineTextFlags & ~TEXTFLAG_ELLIPSIS_AT_END : SingleLineTextFlags, Metrics, true);
				const float Overflow = ClanCache.Ink().Width() - Budget;
				if(Overflow <= .00001f || Limit <= .01f)
					break;
				Limit = std::max(.01f, Limit - std::max(Pixel, Overflow));
			}
			pRenderer->SetRenderFlags(Flags);
			Identity.m_Padding = Padding;
			Identity.m_ClanWidth = ClanCache.Ink().m_Valid ? std::ceil(ClanCache.Ink().Width() / Pixel) * Pixel + 2 * Padding : 0;
		}
		const float NameLimit = std::max(.01f, Width - (pClan[0] ? Pixel : 0) - Identity.m_Gap - Identity.m_ReadyWidth - Identity.m_ClanWidth);
		Identity.m_NameWidth = std::min(Name, NameLimit);
		NameCache.Update(pRenderer, pName, Font, NameLimit,
			Name <= NameLimit ? SingleLineTextFlags & ~TEXTFLAG_ELLIPSIS_AT_END : SingleLineTextFlags, Metrics);
		return Identity;
	}

	struct CClanPillPlacement
	{
		LucentHud::CRect m_Rect{};
		vec2 m_Text{};
		static CClanPillPlacement Calculate(const CIdentity &Identity, const STextInkBounds &Ink,
			float NameX, float RenderedName, float RowY, float RowHeight, const LucentHud::CPixelGrid &Grid)
		{
			if(!Ink.m_Valid || Identity.m_ClanWidth <= 0)
				return {};
			const float X = Grid.m_Origin.x + std::ceil((NameX + Identity.ClanOffset(RenderedName) - Grid.m_Origin.x) / Grid.m_Step.x) * Grid.m_Step.x;
			const auto Rect = Grid.SnapBounds({X, RowY + (RowHeight - Identity.m_Height) / 2, Identity.m_ClanWidth, Identity.m_Height});
			return {Rect, {Rect.x + (Rect.w - Ink.Width()) / 2 - Ink.m_Left, RowY + (RowHeight - Identity.m_ClanFont) / 2}};
		}
	};
}
#endif
