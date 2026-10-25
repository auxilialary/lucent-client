#include "browser_ambient.h"

#include "lucent_menu_effects.h"

#include <engine/graphics.h>

#include <algorithm>
#include <cmath>

void CLucentBrowserAmbient::Update(float Dt, bool Animate, vec2 Cursor)
{
	const float Step = std::clamp(Dt, 0.f, .1f);
	if(Animate)
	{
		m_Time += Step;
		const float Blend = 1 - std::exp(-Step * 3.f);
		m_Cursor += (Cursor - m_Cursor) * Blend;
	}
	else
	{
		m_Time = 0;
		m_Cursor = vec2(0, 0);
	}
}

void CLucentBrowserAmbient::Render(IGraphics *pGraphics, const CUIRect &Screen, float Pulse, float Opacity) const
{
	// Freeze at a deliberate phase in reduced-motion mode, supplied by Update.
	const double Time = m_Time;
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	// A bounded, coarse mesh approximates soft elliptical illumination. Keep
	// the field out of the table and below header text; no blur passes required.
	constexpr int XSteps = 24, YSteps = 20;
	const auto Light = [&](float U, float V) {
		const float X = U - .73f - .012f * std::sin(Time * .027f) - m_Cursor.x * .004f;
		const float Y = V - .40f - .018f * std::sin(Time * .019f) - m_Cursor.y * .005f;
		const float Boundary = std::clamp((U - .41f) / .15f, 0.f, 1.f);
		const float Header = std::clamp((V - .10f) / .12f, 0.f, 1.f);
		const float Field = std::exp(-X * X * 16 - Y * Y * 7);
		const float Local = std::exp(-std::pow((U - .51f) * 15, 2.f) - std::pow((V - .45f) * 5, 2.f));
		return ColorRGBA(.91f, .68f, .80f, (.072f * Field + .045f * Local * Pulse) * Boundary * Header * Opacity);
	};
	for(int Y = 0; Y < YSteps; ++Y)
		for(int X = 0; X < XSteps; ++X)
		{
			const float U = float(X) / XSteps, V = float(Y) / YSteps;
			const float R = float(X + 1) / XSteps, B = float(Y + 1) / YSteps;
			pGraphics->SetColor4(Light(U, V), Light(R, V), Light(U, B), Light(R, B));
			IGraphics::CQuadItem Tile(Screen.x + Screen.w * U, Screen.y + Screen.h * V, Screen.w / XSteps, Screen.h / YSteps);
			pGraphics->QuadsDrawTL(&Tile, 1);
		}
	pGraphics->QuadsEnd();
	// Asymmetric fragments compose the upper scene and its lower outer edge.
	// Reuse the menu's Bezier/stroke renderer, without a closed orbit or frame.
	const float Drift = 3 * std::sin(Time * .045f) + m_Cursor.x * 1.5f;
	const auto Point = [&](float X, float Y) { return vec2(Screen.x + Screen.w * X + Drift, Screen.y + Screen.h * Y); };
	CLucentMenuEffects::SceneCurve(pGraphics, Point(.48f, .24f), Point(.55f, .10f), Point(.82f, .095f), Point(.94f, .19f), .28f * Opacity);
	CLucentMenuEffects::SceneCurve(pGraphics, Point(.83f, .72f), Point(.96f, .69f), Point(.99f, .53f), Point(.96f, .45f), .21f * Opacity);
	// Sparse elongated vector stars sit outside artwork/title/control bounds.
	for(int I = 0; I < 3; ++I)
	{
		const float X[] = {.49f, .91f, .96f};
		const float Y[] = {.20f, .17f, .55f};
		const float Alpha = .085f + .025f * std::sin(Time * .12f + I * 2.1f);
		CLucentMenuEffects::FocusStar(pGraphics, vec2(Screen.x + Screen.w * X[I], Screen.y + Screen.h * Y[I]), .42f + I * .08f, Alpha * Opacity);
	}
}
