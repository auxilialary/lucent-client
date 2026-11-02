#include "color_picker.h"

#include <base/str.h>

#include <game/client/lucent/ui.h>
#include <game/localization.h>
namespace LucentUi
{
	namespace
	{
		void Ring(IGraphics *pGraphics, const CHueRingGeometry &G, float Pixel)
		{
			if(G.m_Outer <= 0)
				return;
			const int Segments = std::clamp(int(std::ceil(2 * pi * G.m_Outer / std::max(.1f, Pixel * 2))), 128, 720);
			const float Feather = std::min(Pixel / 2, (G.m_Outer - G.m_Inner) / 4);
			const float Radii[] = {G.m_Inner - Feather, G.m_Inner + Feather, G.m_Outer - Feather, G.m_Outer + Feather};
			const float Opacity[] = {0, 1, 1, 0};
			pGraphics->TextureClear();
			pGraphics->QuadsBegin();
			for(int I = 0; I < Segments; ++I)
			{
				const float A = float(I) / Segments, B = float(I + 1) / Segments;
				const auto ColorA = color_cast<ColorRGBA>(ColorHSLA(A, 1, .5f));
				const auto ColorB = color_cast<ColorRGBA>(ColorHSLA(B, 1, .5f));
				for(int Band = 0; Band < 3; ++Band)
				{
					pGraphics->SetColor4(ColorA.WithAlpha(Opacity[Band] * CUIRect::DrawOpacity()), ColorA.WithAlpha(Opacity[Band + 1] * CUIRect::DrawOpacity()), ColorB.WithAlpha(Opacity[Band + 1] * CUIRect::DrawOpacity()), ColorB.WithAlpha(Opacity[Band] * CUIRect::DrawOpacity()));
					const IGraphics::CFreeformItem Quad(G.Point(A, Radii[Band]), G.Point(A, Radii[Band + 1]), G.Point(B, Radii[Band]), G.Point(B, Radii[Band + 1]));
					pGraphics->QuadsDrawFreeform(&Quad, 1);
				}
			}
			pGraphics->QuadsEnd();
		}
	}
	void CColorPickerState::Cancel(CUi *pUi)
	{
		for(auto &Id : m_Ids)
		{
			if(pUi->ActiveItem() == &Id)
				pUi->SetActiveItem(nullptr);
			if(pUi->HotItem() == &Id)
				pUi->SetHotItem(nullptr);
		}
	}
	bool ColorPicker(CUi *pUi, ITextRender *pTextRender, CUIRect View, unsigned *pColor, bool Alpha, float Darkest, CColorPickerState &State)
	{
		State.m_Focus = std::clamp(State.m_Focus, 0, Alpha ? 3 : 2);
		const unsigned Original = *pColor;
		ColorHSLA Color(Original, Alpha);
		const ColorHSLA Before = Color;
		const float Pixel = std::max(.001f, pUi->PixelSize());
		pUi->ClipEnable(&View);
		CUIRect RingView, Controls;
		View.VSplitLeft(View.w * .44f, &RingView, &Controls);
		Controls.VSplitLeft(std::min(12.f, Controls.w * .05f), nullptr, &Controls);
		CUIRect HueLabel;
		RingView.HSplitTop(std::min(20.f, std::max(0.f, RingView.h) * .15f), &HueLabel, &RingView);
		const auto Geometry = CHueRingGeometry::Calculate(RingView, Pixel);
		const void *pRingId = &State.m_Ids[0];
		if(!pUi->Enabled())
			State.Cancel(pUi);
		else
		{
			if(pUi->ConsumeHotkey(CUi::HOTKEY_TAB))
				State.MoveFocus(Alpha, pUi->Input()->ShiftIsPressed());
			const bool Hover = pUi->MouseHovered(&RingView) && Geometry.Contains(pUi->MousePos());
			if(Hover)
				pUi->SetHotItem(pRingId);
			if(Hover && pUi->MouseButtonClicked(0))
			{
				pUi->SetActiveItem(pRingId);
				State.m_Focus = 0;
			}
			if(pUi->CheckActiveItem(pRingId))
			{
				// Keep capture while dragging through the hole or beyond the popup bounds.
				pUi->SetHotItem(pRingId);
				if(pUi->MouseButton(0))
					Color.h = Geometry.HueAt(pUi->MousePos(), Color.h);
				else
					pUi->SetActiveItem(nullptr);
			}
			for(int I = 1; I < (Alpha ? 4 : 3); ++I)
				if(pUi->ActiveItem() == &State.m_Ids[I])
					State.m_Focus = I;
			if(pUi->ConsumeHotkey(CUi::HOTKEY_LEFT) || pUi->ConsumeHotkey(CUi::HOTKEY_DOWN))
				Color[State.m_Focus] = State.Step(Color[State.m_Focus], -1);
			if(pUi->ConsumeHotkey(CUi::HOTKEY_RIGHT) || pUi->ConsumeHotkey(CUi::HOTKEY_UP))
				Color[State.m_Focus] = State.Step(Color[State.m_Focus], 1);
			if(pUi->ConsumeHotkey(CUi::HOTKEY_HOME))
				Color[State.m_Focus] = 0;
			if(pUi->ConsumeHotkey(CUi::HOTKEY_END))
				Color[State.m_Focus] = 1;
		}
		Ring(pUi->Graphics(), Geometry, Pixel);
		const auto Center = Geometry.Indicator(Color.h);
		const float Emphasis = Highlight(pRingId, pUi->HotItem() == pRingId || State.m_Focus == 0);
		const float Radius = std::max(3.f, Pixel) + Emphasis;
		if(Geometry.m_Outer > 0)
		{
			CheckboxMesh(pUi->Graphics(), Center, CheckboxBoundary(Center, Radius + Pixel / 2), CheckboxBoundary(Center, Radius + 1.5f * Pixel), [](vec2) { return LucentStyle::Style::Surface.WithAlpha(CUIRect::DrawOpacity()); });
			CheckboxMesh(pUi->Graphics(), Center, CheckboxBoundary(Center, Radius - Pixel / 2), CheckboxBoundary(Center, Radius + Pixel / 2), [](vec2) { return LucentStyle::Style::Text.WithAlpha(CUIRect::DrawOpacity()); });
		}
		char aText[96];
		str_format(aText, sizeof(aText), "%s %.1f°", Localize("Hue"), Color.h * 360);
		pUi->DoLabel(&HueLabel, aText, 11, TEXTALIGN_MC);
		const char *apNames[] = {Localize("Hue"), Localize("Contrast"), Localize("Brightness"), Localize("Alpha")};
		for(int I = 1; I < (Alpha ? 4 : 3); ++I)
		{
			CUIRect Row, Label, Rail;
			Controls.HSplitTop(std::min(44.f, std::max(0.f, View.h) / (Alpha ? 4 : 3)), &Row, &Controls);
			Row.HSplitTop(std::min(18.f, Row.h * .45f), &Label, &Rail);
			const float Display = I == 2 ? Color.UnclampLighting(Darkest).l : Color[I];
			str_format(aText, sizeof(aText), "%s %.1f%% (%03d)", apNames[I], Display * 100, round_to_int(Color[I] * 255));
			pTextRender->TextColor(State.m_Focus == I ? LucentStyle::Style::Accent : LucentStyle::Style::Text);
			SLabelProperties Props;
			Props.m_MaxWidth = Label.w;
			Props.m_EllipsisAtEnd = true;
			Props.m_EnableWidthCheck = false;
			pUi->DoLabel(&Label, aText, 11, TEXTALIGN_ML, Props);
			pTextRender->TextColor(pTextRender->DefaultTextColor());
			const float Next = pUi->DoScrollbarH(&State.m_Ids[I], &Rail, Color[I], nullptr, true, true);
			if(pUi->Enabled())
				Color[I] = Next;
		}
		CUIRect Swatch;
		Controls.HSplitTop(std::min(28.f, std::max(0.f, Controls.h)), &Swatch, nullptr);
		Swatch.Draw(color_cast<ColorRGBA>(Color.UnclampLighting(Darkest)).WithAlpha(1), IGraphics::CORNER_ALL, RADIUS);
		pUi->ClipDisable();
		if(Color != Before)
			*pColor = Color.Pack(Alpha);
		return *pColor != Original;
	}
}
