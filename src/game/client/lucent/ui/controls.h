#ifndef GAME_CLIENT_LUCENT_UI_CONTROLS_H
#define GAME_CLIENT_LUCENT_UI_CONTROLS_H

#include "checkbox_geometry.h"
#include "surfaces.h"

#include <game/client/lucent/motion/transition.h>

#include <array>

namespace LucentUi
{
	enum class EChannel
	{
		HOVER,
		ACTIVE,
		FOCUS
	};

	float Highlight(const void *pId, bool Target, EChannel Channel = EChannel::HOVER, float Duration = 0);
	bool FilterTab(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, const CUIRect &Rect, bool Selected);
	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags = BUTTONFLAG_LEFT);
	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags, bool ControlRight);
	// Visual availability is independent from temporary input blocking during a fade.
	int CheckBox(CUi *pUi, const void *pId, const char *pText, const char *pMark, const CUIRect &Rect, unsigned Flags, bool ControlRight, bool VisualEnabled);
	int FilledCheckBox(CUi *pUi, const void *pId, const char *pText, bool Checked, const CUIRect &Rect);
	// Wrapped rows start the control at the content anchor; input may be blocked independently.
	int FilledCheckBox(CUi *pUi, const void *pId, const char *pText, bool Checked, const CUIRect &Rect, bool VisualEnabled, bool WrapLabel, float IdleLabelBrightness = 1.f);
	bool Button(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, bool Enabled = true);
	bool SceneButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Enabled = true);
	bool TextButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pText, CUIRect Rect, bool Selected = false, float Size = 11, int Align = TEXTALIGN_MC, bool Underline = true, const char *pIcon = nullptr);
	bool IconButton(CUi *pUi, ITextRender *pTextRender, const void *pId, const char *pIcon, const char *pHint, CUIRect Rect, bool Selected = false, bool Tooltip = true);

	inline float FilterItemOpacity(const void *pId, bool Active, bool Hover)
	{
		const float Hot = Highlight(pId, Hover);
		const float Selected = Highlight(pId, Active, EChannel::ACTIVE);
		return LucentStyle::Motion::Lerp(LucentStyle::Motion::Lerp(.30f, .75f, Hot), 1.f, Selected);
	}

	template<size_t N, typename FColor>
	inline void CheckboxMesh(IGraphics *pGraphics, vec2 Center, const std::array<vec2, N> &Inner, const std::array<vec2, N> &Outer, FColor Color)
	{
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(size_t I = 0; I < N; ++I)
		{
			const size_t J = (I + 1) % N;
			pGraphics->SetColor4(Color(Center), Color(Inner[I]), Color(Inner[J]), Color(Inner[J]));
			const IGraphics::CFreeformItem Triangle(Center, Inner[I], Inner[J], Inner[J]);
			pGraphics->QuadsDrawFreeform(&Triangle, 1);
			// SetColor4 takes TL, TR, BL, BR; freeform points are TL, TR, BR, BL.
			pGraphics->SetColor4(Color(Inner[I]), Color(Outer[I]).WithAlpha(0), Color(Outer[J]).WithAlpha(0), Color(Inner[J]));
			const IGraphics::CFreeformItem Edge(Inner[I], Outer[I], Inner[J], Outer[J]);
			pGraphics->QuadsDrawFreeform(&Edge, 1);
		}
		pGraphics->QuadsEnd();
	}
}
#endif
