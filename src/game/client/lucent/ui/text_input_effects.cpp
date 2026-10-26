#include "text_input_effects.h"

#include <base/str.h>
#include <base/time.h>
#include <engine/keys.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/motion/text_motion.h>
#include <game/client/lucent/style/gradient.h>

#include <cmath>

namespace
{
	// Only the focused editor owns visual motion, including chat and both consoles.
	static CLucentTextMotion s_TextMotion;
	static CLucentTextAppear s_TextAppear;

	static double s_CaretChanged = 0;
	static vec2 s_LastInputOrigin(0, 0);
	static float s_LastInputHeight = 0;
	static size_t s_LastInputChars = 0;
	static int s_LastCursorChars = 0;
	static bool s_LastSelection = false;
	static CLineInput *s_pMotionInput = nullptr;

}

namespace LucentUi
{
	float SInputGlyphOpacity::Sample(int Character, const void *pContext)
	{
		const auto &Context = *static_cast<const SInputGlyphOpacity *>(pContext);
		return s_TextAppear.Alpha(Character, Context.m_Now, Context.m_CompositionBegin, Context.m_CompositionLength);
	}

	void ResetTextInputAppearance()
	{
		s_TextAppear.Reset();
	}

	void ReplaceTextInputAppearance(int Begin, int Removed, int Added, double Now)
	{
		s_TextAppear.Replace(Begin, Removed, Added, Now);
	}

	void ApplyTextInputOpacity(CTextCursor &Cursor, const SInputGlyphOpacity &Opacity, bool HasText)
	{
		if(s_TextAppear.Active(Opacity.m_Now) && HasText)
			Cursor.m_GlyphOpacity = {&Opacity, SInputGlyphOpacity::Sample};
	}

	void ResetTextInputMotion(bool Activating)
	{
		s_TextAppear.Reset();
		s_TextMotion.Reset();
		if(Activating)
			s_CaretChanged = double(time_get()) / time_freq();
	}
}

void CLineInput::RenderCaretEffects(const CTextCursor &Cursor, vec2 CursorPos, const char *pDisplayStr, size_t DisplayCursorOffset, bool HasComposition, bool Changed, double Now, bool AnimateCaret, const CUIRect &Clip)
{
	auto &Clock = LucentStyle::Motion::Clock();
	Clock.BeginFrame(Now, AnimateCaret);
	const int CursorChars = str_utf8_offset_bytes_to_chars(pDisplayStr, DisplayCursorOffset);
	const int CharDelta = int(GetNumChars()) - int(s_LastInputChars);
	const bool Editing = CharDelta != 0;
	const bool Sequential = m_VisualEditCount != 0 && CharDelta == m_VisualEditCount && CursorChars - s_LastCursorChars == CharDelta;
	const bool Reposition = m_VisualReposition || HasComposition || s_pMotionInput != this || s_LastInputHeight != Cursor.m_AlignedFontSize ||
				HasSelection() || s_LastSelection || m_MouseSelection.m_Selecting ||
				(Editing ? !Sequential : CursorChars != s_LastCursorChars) ||
				(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_V));
	if(!Reposition)
		s_TextMotion.Translate(CursorPos - s_LastInputOrigin);
	s_TextMotion.Update(m_CaretPosition, Cursor.m_AlignedFontSize, Now, AnimateCaret, Reposition, std::abs(m_VisualEditCount));
	m_VisualReposition = false;
	m_VisualEditCount = 0;
	s_LastInputChars = GetNumChars();
	s_LastCursorChars = CursorChars;
	s_LastSelection = HasSelection();
	s_pMotionInput = this;
	s_LastInputOrigin = CursorPos;
	s_LastInputHeight = Cursor.m_AlignedFontSize;
	if(Changed || s_TextMotion.Moving(Now))
		s_CaretChanged = Now;
	if(AnimateCaret && (HasComposition || Now - s_CaretChanged < .5 || std::fmod(Now - s_CaretChanged, 1.0) < .5))
	{
		const float CaretWidth = std::max(LucentStyle::Motion::TextHeadMinWidth, Cursor.m_AlignedFontSize * LucentStyle::Motion::TextHeadWidth);
		vec2 Visual = m_CaretPosition;
		vec2 Tail = s_TextMotion.Tail(Now, Cursor.m_AlignedFontSize);
		Visual.x = std::clamp(Visual.x, Clip.x, std::max(Clip.x, Clip.x + Clip.w - CaretWidth));
		Tail.x = std::clamp(Tail.x, Clip.x, std::max(Clip.x, Clip.x + Clip.w - CaretWidth));
		const float Alpha = TextRender()->RenderOpacity();
		const float Fade = s_TextMotion.TrailAlpha(Now) * Alpha;
		const auto White = LucentStyle::Gradient::White.WithAlpha(Fade);
		const auto Rose = LucentStyle::Gradient::Pink.WithAlpha(Fade);
		CUIRect Trail = {std::min(Tail.x, Visual.x), Visual.y, std::abs(Visual.x - Tail.x), Cursor.m_AlignedFontSize};
		// A soft opacity ribbon, not a second cursor: both ends dissolve and
		// the sole solid caret stays exactly at the logical insertion point.
		CUIRect Left, Right;
		Trail.VSplitMid(&Left, &Right);
		Left.Draw4(White.WithAlpha(0), White, Rose.WithAlpha(0), Rose, 0, 0);
		Right.Draw4(White, White.WithAlpha(0), Rose, Rose.WithAlpha(0), 0, 0);
		CUIRect Caret = {m_CaretPosition.x, m_CaretPosition.y, CaretWidth, Cursor.m_AlignedFontSize};
		Caret.Draw(LucentStyle::Gradient::Sample(0, Clock.GradientTime()).WithAlpha(Alpha), 0, 0);
	}

}
