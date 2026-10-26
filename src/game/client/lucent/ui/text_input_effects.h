#ifndef GAME_CLIENT_LUCENT_UI_TEXT_INPUT_EFFECTS_H
#define GAME_CLIENT_LUCENT_UI_TEXT_INPUT_EFFECTS_H

class CTextCursor;

namespace LucentUi
{
	// Kept on the rendering stack until all TextEx calls using its callback finish.
	struct SInputGlyphOpacity
	{
		double m_Now;
		int m_CompositionBegin, m_CompositionLength;
		static float Sample(int Character, const void *pContext);
	};

	void ResetTextInputAppearance();
	void ReplaceTextInputAppearance(int Begin, int Removed, int Added, double Now);
	void ApplyTextInputOpacity(CTextCursor &Cursor, const SInputGlyphOpacity &Opacity, bool HasText);
	void ResetTextInputMotion(bool Activating);
}
#endif
