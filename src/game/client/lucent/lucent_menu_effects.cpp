#include "lucent_menu_effects.h"

#include <engine/graphics.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float TAU = 6.28318530718f;

vec2 Bezier(vec2 A, vec2 B, vec2 C, vec2 D, float T)
{
	const float U = 1 - T;
	return A * (U * U * U) + B * (3 * U * U * T) + C * (3 * U * T * T) + D * (T * T * T);
}

// Feathered ribbons keep a consistent visual thickness on every backend.
void Stroke(IGraphics *pGraphics, vec2 A, vec2 B, float Radius, ColorRGBA Color)
{
	const vec2 Delta = B - A;
	if(length(Delta) < 0.0001f)
		return;
	const vec2 N = normalize(vec2(-Delta.y, Delta.x)) * Radius;
	const ColorRGBA Clear(Color.r, Color.g, Color.b, 0);
	for(int Side : {-1, 1})
	{
		pGraphics->SetColor4(Color, Color, Clear, Clear);
		IGraphics::CFreeformItem Quad(A, B, A + N * Side, B + N * Side);
		pGraphics->QuadsDrawFreeform(&Quad, 1);
	}
}
}

void CLucentMenuEffects::Connection(IGraphics *pGraphics, vec2 Center, float Scale, const CUIRect &Target, int Index, float Progress, float Highlight, double Time, float PulseAge)
{
	if(Progress <= 0)
		return;
	const int Side = Index < 3 ? -1 : 1;
	const int Row = Index % 3;
	// Same tilted ellipse as the logo. Attach to its outward-facing perimeter.
	const float Angle = Side < 0 ? 3.14159265f + (Row - 1) * -0.65f : (Row - 1) * 0.65f;
	const float X = 174 * Scale * std::cos(Angle);
	const float Y = 37 * Scale * std::sin(Angle);
	const vec2 A = Center + vec2(X * 0.989f + Y * 0.148f, Y * 0.989f - X * 0.148f);
	const vec2 D(Side < 0 ? Target.x + Target.w : Target.x, Target.y + Target.h / 2);
	const vec2 B = A + vec2(Side * (32 + Row * 5), (Row - 1) * (26 + Index * 2));
	const vec2 C = D - vec2(Side * (26 + Index * 2), (Row - 1) * (10 + Index));
	const float Reveal = 1 - std::pow(1 - std::min(1.0f, Progress * 1.65f), 3.0f);
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	const float Head = static_cast<float>(std::fmod(Time * (0.19 + Index * 0.012) + Index * 0.173, 1.45));
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	constexpr int Segments = 48;
	for(int i = 0; i < Segments; ++i)
	{
		const float T = (i + 0.5f) / Segments;
		const auto Wave = [T](float H) {
			const float Tail = H - T;
			return Tail >= 0 && Tail < 0.24f ? std::pow(1 - Tail / 0.24f, 2.0f) * std::min(1.0f, Tail / 0.025f) : 0.0f;
		};
		const float Pulse = Time > 0 ? Wave(Head) * (0.10f + Highlight * 0.17f) : 0;
		const float Arrival = PulseAge >= 0 && PulseAge < 1.1f ? Wave(PulseAge * 1.25f) * 0.38f : 0;
		const float Alpha = Progress * (0.085f + Highlight * 0.13f + Pulse + Arrival);
		const ColorRGBA Color(1, 0.88f - T * 0.13f, 0.94f - T * 0.06f, Alpha);
		Stroke(pGraphics, Bezier(A, B, C, D, i * Reveal / Segments), Bezier(A, B, C, D, (i + 1) * Reveal / Segments), Pixel * 1.3f, Color);
	}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, bool Accent)
{
	Rect.Draw(ColorRGBA(0.8f, 0.65f, 0.75f, Alpha * (Accent ? 0.32f : 0.10f)), IGraphics::CORNER_ALL, 10);
	CUIRect Inner = {Rect.x + 0.7f, Rect.y + 0.7f, Rect.w - 1.4f, Rect.h - 1.4f};
	Inner.Draw(ColorRGBA(0.065f, 0.056f, 0.078f, Alpha * 0.94f), IGraphics::CORNER_ALL, 9.3f);
	const vec2 Origin(Rect.x + std::min(18.0f, Rect.w * 0.14f), Rect.y + Rect.h / 2);
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	const ColorRGBA Color(0.98f, Accent ? 0.78f : 0.87f, 0.93f, Alpha * 0.85f);
	const auto Line = [&](float X, float Y, float U, float V) { Stroke(pGraphics, Origin + vec2(X, Y), Origin + vec2(U, V), std::max(Pixel, 0.6f), Color); };
	const auto Box = [&](float X, float Y, float W, float H) {
		Line(X, Y, X + W, Y); Line(X + W, Y, X + W, Y + H);
		Line(X + W, Y + H, X, Y + H); Line(X, Y + H, X, Y);
	};
	switch(Icon)
	{
	case 0: Line(-4, -6, 6, 0); Line(6, 0, -4, 6); Line(-4, 6, -4, -6); break;
	case 1:
		Box(-7, -6, 14, 12); Line(-3, -6, -3, 6); Line(3, -6, 3, 6);
		Line(-7, -2, -3, -2); Line(-7, 2, -3, 2); Line(3, -2, 7, -2); Line(3, 2, 7, 2); break;
	case 2:
		Line(-6, 6, -4, 0); Line(-4, 0, 4, -8); Line(4, -8, 8, -4); Line(8, -4, 0, 4); Line(0, 4, -6, 6); Line(2, -6, 6, -2); break;
	case 3: case 12:
		Box(-7, -7, 14, 6); Box(-7, 2, 14, 6); Line(-4, -4, -2, -4); Line(-4, 5, -2, 5); break;
	case 4:
		for(int i = 0; i < 3; ++i) { const float Y = -5 + i * 5; const float X = i == 1 ? 3 : -3; Line(-7, Y, X - 2, Y); Line(X + 2, Y, 7, Y); Box(X - 2, Y - 2, 4, 4); } break;
	case 5:
		Line(0, -8, 0, 0);
		for(int i = 0; i < 24; ++i) { const float A = -1.0f + i * (TAU - 1.15f) / 24; const float B = -1.0f + (i + 1) * (TAU - 1.15f) / 24; Line(7 * std::cos(A), 7 * std::sin(A), 7 * std::cos(B), 7 * std::sin(B)); } break;
	default:
		for(int i = -1; i <= 1; ++i) Line(i * 5 - 0.6f, 0, i * 5 + 0.6f, 0);
	}
	pGraphics->QuadsEnd();
	pGraphics->SetColor(1, 1, 1, 1);
}
