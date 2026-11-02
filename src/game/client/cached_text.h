#ifndef GAME_CLIENT_CACHED_TEXT_H
#define GAME_CLIENT_CACHED_TEXT_H
#include <engine/textrender.h>

#include <string>

/**
 * Text that keeps its text container across frames and is only rebuilt when the text or
 * its layout changes, for text that is rendered every frame. The color is applied when
 * rendering, so changing it does not rebuild the container.
 *
 * The container must be released with @link Reset @endlink before the text render drops
 * its containers, which happens on window resize and language change.
 */
class CCachedText
{
	STextContainerIndex m_TextContainerIndex;
	// Pixels per UI unit; bounded glyph geometry depends on the active mapping.
	vec2 m_Metrics = vec2(0, 0);
	std::string m_Text;
	float m_FontSize = -1.0f;
	float m_LineWidth = -1.0f;
	int m_CursorFlags = 0;
	bool m_TrackInk = false;
	STextInkBounds m_Ink;
	STextBoundingBox m_BoundingBox = {0.0f, 0.0f, 0.0f, 0.0f};
	float m_MaxCharacterHeight = 0.0f;
	float m_AlignedFontSize = 0.0f;

public:
	CCachedText() = default;
	// Copying would leave two owners for the same text container.
	CCachedText(const CCachedText &) = delete;
	CCachedText &operator=(const CCachedText &) = delete;

	template<typename TRenderer>
	void Update(TRenderer *pTextRender, const char *pText, float FontSize, float LineWidth = -1.0f, int CursorFlags = TEXTFLAG_RENDER, vec2 Metrics = vec2(0, 0), bool TrackInk = false)
	{
		if(m_TrackInk == TrackInk && m_Metrics == Metrics && m_FontSize == FontSize && m_LineWidth == LineWidth && m_CursorFlags == CursorFlags && m_Text == pText)
			return;

		// The render flags of a text container are fixed when it is created and depend on the
		// line width, so only text and font size changes can reuse the existing container and
		// upload the new quads into its buffer instead of allocating a new one.
		const bool ReuseContainer = m_TrackInk == TrackInk && m_Metrics == Metrics && m_TextContainerIndex.Valid() && m_LineWidth == LineWidth && m_CursorFlags == CursorFlags;

		m_Metrics = Metrics;
		m_TrackInk = TrackInk;
		m_Text = pText;
		m_FontSize = FontSize;
		m_LineWidth = LineWidth;
		m_CursorFlags = CursorFlags;

		CTextCursor Cursor;
		Cursor.m_FontSize = FontSize;
		Cursor.m_LineWidth = LineWidth;
		Cursor.m_Flags = CursorFlags;
		Cursor.m_TrackInk = TrackInk;

		// The color is applied when rendering, so it must not be baked into the quads.
		const ColorRGBA OldColor = pTextRender->GetTextColor();
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		if(ReuseContainer)
			pTextRender->RecreateTextContainerSoft(m_TextContainerIndex, &Cursor, m_Text.c_str());
		else
			pTextRender->RecreateTextContainer(m_TextContainerIndex, &Cursor, m_Text.c_str());
		pTextRender->TextColor(OldColor);

		m_BoundingBox = Cursor.BoundingBox();
		m_Ink = Cursor.m_Ink;
		m_MaxCharacterHeight = Cursor.m_MaxCharacterHeight;
		m_AlignedFontSize = Cursor.m_AlignedFontSize;
	}
	template<typename TRenderer>
	void Render(TRenderer *pTextRender, vec2 Pos, ColorRGBA Color) const
	{
		if(!m_TextContainerIndex.Valid())
			return;
		// The quads are built with the default color, so the outline has to be faded here
		// instead of inheriting the alpha from the baked vertex color.
		pTextRender->RenderTextContainer(m_TextContainerIndex, Color, pTextRender->DefaultTextOutlineColor().WithMultipliedAlpha(Color.a), Pos.x, Pos.y);
	}
	template<typename TRenderer>
	void Reset(TRenderer *pTextRender)
	{
		pTextRender->DeleteTextContainer(m_TextContainerIndex);
		m_Text.clear();
		m_Ink = {};
		m_FontSize = -1.0f;
		m_LineWidth = -1.0f;
		m_CursorFlags = 0;
		m_BoundingBox = {0.0f, 0.0f, 0.0f, 0.0f};
		m_MaxCharacterHeight = 0.0f;
	}

	const STextInkBounds &Ink() const { return m_Ink; }
	float Width() const { return m_BoundingBox.m_W; }
	float MaxCharacterHeight() const { return m_MaxCharacterHeight; }
	float AlignedFontSize() const { return m_AlignedFontSize; }
};

#endif
