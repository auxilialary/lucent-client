#include "lucent_menu_effects.h"

#include <engine/graphics.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TAU = 6.28318530718f;

	float OrbitDirection(int Index)
	{
		constexpr float Angles[] = {3.95f, 3.12f, 2.65f, 5.50f, 0.0f, 0.65f};
		return Angles[Index];
	}

	vec2 OrbitPoint(vec2 Center, float Scale, float Angle)
	{
		const float X = 174 * Scale * std::cos(Angle);
		const float Y = 48 * Scale * std::sin(Angle);
		return Center + vec2(X * 0.989f + Y * 0.148f, Y * 0.989f - X * 0.148f);
	}

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

void CLucentMenuEffects::Orbit(IGraphics *pGraphics, vec2 Center, float Scale, double Time, const float *pHighlight, bool Foreground)
{
	// Three fixed, tapered arcs. Only the lower arc sits in front of the text;
	// its central span stays below the baseline to preserve the wordmark.
	constexpr float Starts[] = {2.60f, 4.45f, -0.35f};
	constexpr float Ends[] = {4.05f, 5.50f, 1.20f};
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	for(int Arc = 0; Arc < 3; ++Arc)
	{
		if(Foreground != (Arc == 2))
			continue;
		const float Breath = 0.94f + 0.06f * std::sin(static_cast<float>(Time * 0.65) + Arc);
		for(int i = 0; i < 56; ++i)
		{
			const float T = (i + 0.5f) / 56;
			const float Angle = mix(Starts[Arc], Ends[Arc], T);
			float Reaction = 0;
			for(int Chip = 0; pHighlight && Chip < 6; ++Chip)
			{
				if(pHighlight[Chip] <= 0.001f)
					continue;
				const float Direction = OrbitDirection(Chip);
				Reaction = std::max(Reaction, pHighlight[Chip] * std::pow(std::max(0.0f, std::cos(Angle - Direction)), 12.0f));
			}
			const float Taper = std::min(1.0f, std::min(T, 1 - T) * 9);
			const vec2 A = OrbitPoint(Center, Scale, mix(Starts[Arc], Ends[Arc], i / 56.0f));
			const vec2 B = OrbitPoint(Center, Scale, mix(Starts[Arc], Ends[Arc], (i + 1) / 56.0f));
			Stroke(pGraphics, A, B, Pixel * 3.0f, ColorRGBA(1, 0.65f, 0.81f, Taper * (0.025f + Reaction * 0.08f)));
			Stroke(pGraphics, A, B, Pixel * 0.95f, ColorRGBA(1, 0.81f, 0.91f, Taper * (0.22f * Breath + Reaction * 0.52f)));
		}
	}
	// Two quiet light anchors, rendered as feathered crosses rather than discs.
	if(!Foreground)
		for(float Angle : {3.12f, 5.15f})
		{
			const vec2 P = OrbitPoint(Center, Scale, Angle);
			Stroke(pGraphics, P - vec2(2.4f, 0), P + vec2(2.4f, 0), Pixel * 1.8f, ColorRGBA(1, 0.87f, 0.94f, 0.55f));
		}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::Connection(IGraphics *pGraphics, vec2 Center, float Scale, const CUIRect &Target, int Index, float Progress, float Highlight, float PulseAge)
{
	if(Progress <= 0)
		return;
	const int Side = Index < 3 ? -1 : 1;
	const int Row = Index % 3;
	// Share the exact arc geometry; fixed cubic handles avoid animated wiggles.
	const float Angle = OrbitDirection(Index);
	const vec2 A = OrbitPoint(Center, Scale, Angle);
	const vec2 D(Side < 0 ? Target.x + Target.w : Target.x, Target.y + Target.h / 2);
	const vec2 B = A + vec2(Side * (30 + Row * 4), (Row - 1) * 14);
	const vec2 C = D - vec2(Side * (30 + Index), 0);
	const float Reveal = 1 - std::pow(1 - std::min(1.0f, Progress * 1.65f), 3.0f);
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
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

		const float Arrival = PulseAge >= 0 && PulseAge < 1.1f ? Wave(PulseAge * 1.25f) * 0.38f : 0;
		const float Alpha = Progress * ((0.13f + Highlight * 0.25f) * (1 - T * 0.55f) + Arrival * Highlight);
		const ColorRGBA Color(1, 0.88f - T * 0.13f, 0.94f - T * 0.06f, Alpha);
		Stroke(pGraphics, Bezier(A, B, C, D, i * Reveal / Segments), Bezier(A, B, C, D, (i + 1) * Reveal / Segments), Pixel * 1.3f, Color);
	}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, float Highlight)
{
	const bool Secondary = Icon >= 6 && Icon != 12;
	const float Strength = Secondary ? 0.75f : 1.0f;
	// Layered rounded outlines form a narrow white-pink contour and soft halo.
	if(Highlight > 0.001f)
		for(int Layer = 3; Layer > 0; --Layer)
		{
			const float Spread = Layer * 1.4f;
			CUIRect Glow = {Rect.x - Spread, Rect.y - Spread, Rect.w + Spread * 2, Rect.h + Spread * 2};
			Glow.Draw(ColorRGBA(1, 0.66f, 0.83f, Alpha * Highlight * 0.018f * Strength), IGraphics::CORNER_ALL, 10 + Spread);
		}
	Rect.Draw(ColorRGBA(1, 0.81f + Highlight * 0.09f, 0.91f + Highlight * 0.05f, Alpha * (0.14f + Highlight * 0.62f) * Strength), IGraphics::CORNER_ALL, 10);
	const float Border = 0.7f + Highlight * 0.25f;
	CUIRect Inner = {Rect.x + Border, Rect.y + Border, Rect.w - Border * 2, Rect.h - Border * 2};
	Inner.Draw(ColorRGBA(0.065f + Highlight * 0.016f, 0.056f, 0.078f + Highlight * 0.013f, Alpha), IGraphics::CORNER_ALL, 10 - Border);
	const vec2 Origin(Rect.x + std::min(18.0f, Rect.w * 0.14f), Rect.y + Rect.h / 2);
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	const ColorRGBA Color(0.88f + Highlight * 0.12f, 0.85f + Highlight * 0.1f, 0.92f + Highlight * 0.06f, Alpha * (0.82f + Highlight * 0.18f));
	const auto Line = [&](float X, float Y, float U, float V) { Stroke(pGraphics, Origin + vec2(X, Y), Origin + vec2(U, V), std::max(Pixel * 1.15f, 0.7f), Color); };
	const auto Box = [&](float X, float Y, float W, float H) {
		Line(X, Y, X + W, Y);
		Line(X + W, Y, X + W, Y + H);
		Line(X + W, Y + H, X, Y + H);
		Line(X, Y + H, X, Y);
	};
	switch(Icon)
	{
	case 0:
		Line(-4, -6, 6, 0);
		Line(6, 0, -4, 6);
		Line(-4, 6, -4, -6);
		break;
	case 1:
		Box(-7, -6, 14, 12);
		Line(-3, -6, -3, 6);
		Line(3, -6, 3, 6);
		Line(-7, -2, -3, -2);
		Line(-7, 2, -3, 2);
		Line(3, -2, 7, -2);
		Line(3, 2, 7, 2);
		break;
	case 2:
		Line(-6, 6, -4, 0);
		Line(-4, 0, 4, -8);
		Line(4, -8, 8, -4);
		Line(8, -4, 0, 4);
		Line(0, 4, -6, 6);
		Line(2, -6, 6, -2);
		break;
	case 3:
	case 12:
		Box(-7, -6, 14, 5);
		Box(-7, 2, 14, 5);
		Line(-4, -3.5f, -2, -3.5f);
		Line(-4, 4.5f, -2, 4.5f);
		break;
	case 4:
		for(int i = 0; i < 3; ++i)
		{
			const float Y = -5 + i * 5;
			const float X = i == 1 ? 3 : -3;
			Line(-7, Y, X - 2, Y);
			Line(X + 2, Y, 7, Y);
			Box(X - 2, Y - 2, 4, 4);
		}
		break;
	case 5:
		Line(0, -7, 0, -1);
		for(int i = 0; i < 24; ++i)
		{
			const float A = -0.95f + i * (TAU - 1.24f) / 24;
			const float B = -0.95f + (i + 1) * (TAU - 1.24f) / 24;
			Line(7 * std::cos(A), 7 * std::sin(A), 7 * std::cos(B), 7 * std::sin(B));
		}
		break;
	default:
		for(int i = -1; i <= 1; ++i)
			Line(i * 5 - 0.6f, 0, i * 5 + 0.6f, 0);
	}
	pGraphics->QuadsEnd();
	pGraphics->SetColor(1, 1, 1, 1);
}
