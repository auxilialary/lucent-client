#ifndef GAME_CLIENT_TOOLTIPS_H
#define GAME_CLIENT_TOOLTIPS_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

class CTooltip
{
public:
	static constexpr float DefaultFontSize = 14.f;
	CTooltip(const void *pId, const CUIRect &Rect, const char *pText, float WidthHint, float FontSize, bool Truncated, bool FollowCursor = false) :
		m_pId(pId), m_Rect(Rect), m_pText(pText), m_WidthHint(WidthHint), m_FontSize(FontSize), m_Truncated(Truncated), m_FollowCursor(FollowCursor), m_OnScreen(false) {}

	const void *m_pId;
	CUIRect m_Rect;
	const char *m_pText;
	float m_WidthHint;
	float m_FontSize;
	bool m_Truncated;
	bool m_LucentStyle = false;
	bool m_FollowCursor = false;
	static CUIRect AboveCursor(CUIRect Rect, vec2 Mouse, const CUIRect &Screen, float Margin)
	{
		const float Left = Screen.x + Margin, Top = Screen.y + Margin;
		Rect.x = std::clamp(Mouse.x - Rect.w / 2, Left, std::max(Left, Screen.x + Screen.w - Margin - Rect.w));
		Rect.y = std::clamp(Mouse.y - Rect.h - Margin, Top, std::max(Top, Screen.y + Screen.h - Margin - Rect.h));
		return Rect;
	}
	bool m_OnScreen; // used to know if the tooltip should be rendered.
};

class CUi;
class ITextRender;

/**
 * A component that manages and renders UI tooltips.
 *
 * Should be among the last components to render.
 */
class CTooltips
{
	std::unordered_map<uintptr_t, CTooltip> m_Tooltips;
	std::optional<std::reference_wrapper<CTooltip>> m_ActiveTooltip;
	std::optional<std::reference_wrapper<CTooltip>> m_PreviousTooltip;
	int64_t m_HoverTime;

	/**
	 * @param Tooltip A reference to the tooltip that should be active.
	 */
	void SetActiveTooltip(CTooltip &Tooltip);

	inline void ClearActiveTooltip();
	CUi *m_pUi;
	ITextRender *m_pTextRender;
	CUi *Ui() { return m_pUi; }
	ITextRender *TextRender() { return m_pTextRender; }

public:
	CTooltips() = default;
	void Init(CUi *pUi, ITextRender *pTextRender);

	void DoToolTip(const CTooltip &Tooltip);

	void OnReset();
	void OnRender();
};

#endif
