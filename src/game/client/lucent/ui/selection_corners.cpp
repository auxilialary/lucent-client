#include "selection_corners.h"

#include <game/client/lucent/motion/clock.h>
#include <game/client/lucent/ui/surfaces.h>
#include <game/client/lucent/ui/controls.h>
namespace LucentUi
{
	namespace
	{
		void Band(IGraphics *pGraphics, vec2 A, vec2 B, vec2 NA, vec2 NB, float Inner, float Outer, ColorRGBA C0, ColorRGBA C1)
		{
			pGraphics->SetColor4(C0, C1, C1, C0);
			const IGraphics::CFreeformItem Quad(A + NA * Inner, A + NA * Outer, B + NB * Inner, B + NB * Outer);
			pGraphics->QuadsDrawFreeform(&Quad, 1);
		}
		void Cap(IGraphics *pGraphics, vec2 Center, vec2 Normal, vec2 Outward, float Core, float Edge, ColorRGBA Color)
		{
			constexpr int Steps = 16;
			for(int I = 0; I < Steps; ++I)
			{
				const float A = pi * I / Steps, B = pi * (I + 1) / Steps;
				const vec2 NA = Normal * std::cos(A) + Outward * std::sin(A), NB = Normal * std::cos(B) + Outward * std::sin(B);
				pGraphics->SetColor(Color);
				const IGraphics::CFreeformItem Triangle(Center, Center + NA * Core, Center + NB * Core, Center + NB * Core);
				pGraphics->QuadsDrawFreeform(&Triangle, 1);
				Band(pGraphics, Center, Center, NA, NB, Core, Edge, Color, Color.WithAlpha(0));
			}
		}
	}
 void DrawSelectionBackground(IGraphics *pGraphics,CUIRect Rect,float Pixel,ColorRGBA Color)
 {
  if(Rect.w<=0||Rect.h<=0||Color.a<=0)return;
  const auto G=CSelectionBackgroundGeometry::Calculate(Rect,Pixel);
  Color.a*=CUIRect::DrawOpacity();
  CheckboxMesh(pGraphics,Rect.Center(),G.m_Inner,G.m_Outer,[&](vec2){return Color;});
 }
	void DrawSelectionCorners(IGraphics *pGraphics, CUIRect Rect, float Pixel, float Progress)
	{
		Progress = std::clamp(Progress, 0.f, 1.f);
		if(Progress <= 0 || Rect.w <= 0 || Rect.h <= 0)
			return;
		const auto G = CSelectionCornersGeometry::Calculate(Rect, Pixel, Progress);
		const float Core = std::max(0.f, (G.m_Thickness - G.m_Feather) / 2);
		const float Edge = (G.m_Thickness + G.m_Feather) / 2;
		const auto Color = Accent(LucentStyle::Style::ButtonUnderline * Progress * CUIRect::DrawOpacity() * std::min(1.f, G.m_Thickness / std::max(.001f, Pixel)));
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		for(const auto &Path : G.m_Corners)
		{
			for(int I = 1; I < Path.m_Count; ++I)
			{
				const auto A = Path.m_Points[I - 1], B = Path.m_Points[I], NA = Path.m_Normals[I - 1], NB = Path.m_Normals[I];
				Band(pGraphics, A, B, NA, NB, -Core, Core, Color, Color);
				Band(pGraphics, A, B, NA, NB, Core, Edge, Color, Color.WithAlpha(0));
				Band(pGraphics, A, B, NA, NB, -Core, -Edge, Color, Color.WithAlpha(0));
			}
			const int Last = Path.m_Count - 1;
			Cap(pGraphics, Path.m_Points[0], Path.m_Normals[0], vec2(-Path.m_Normals[Last].x, 0), Core, Edge, Color);
			Cap(pGraphics, Path.m_Points[Last], Path.m_Normals[Last], vec2(0, -Path.m_Normals[0].y), Core, Edge, Color);
		}
		pGraphics->QuadsEnd();
	}
	void SelectionCorners(CUi *pUi, CUIRect Rect, LucentStyle::Motion::CTransition &Motion, bool Selected, bool Animate)
	{
		const auto &Clock = LucentStyle::Motion::Clock();
		const float Progress = Motion.Update(Selected, Clock.m_Now, Clock.m_Animate && Animate, LucentStyle::Motion::Selection);
		DrawSelectionCorners(pUi->Graphics(), Rect, pUi->PixelSize(), Progress);
	}
}
