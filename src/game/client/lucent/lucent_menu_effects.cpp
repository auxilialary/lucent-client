#include "lucent_menu_effects.h"

#include <engine/graphics.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr float TAU = 6.28318530718f;

	float OrbitDirection(int Index)
	{
		constexpr float Angles[] = {3.95f, 3.12f, 2.65f, 5.50f, 0.0f, 0.65f};
		return Angles[Index];
	}

	constexpr double ORBIT_PERIODS[] = {10.8, 8.6, 11.9};
	float OrbitPhase(double Time, int Arc)
	{
		return static_cast<float>(std::fmod(Time, ORBIT_PERIODS[Arc]) / ORBIT_PERIODS[Arc]) * TAU * (Arc == 1 ? -1 : 1);
	}

	vec2 OrbitPoint(vec2 Center, float Scale, float Angle)
	{
		const float X = 174 * Scale * std::cos(Angle);
		const float Y = 48 * Scale * std::sin(Angle);
		return Center + vec2(X * 0.989f + Y * 0.148f, Y * 0.989f - X * 0.148f);
	}

	vec2 SelectionSparklePoint(vec2 Center, float Radius, float Angle)
	{
		const float X = std::cos(Angle), Y = std::sin(Angle);
		return Center + vec2(X * X * X, Y * Y * Y) * Radius;
	}

	float ConnectionPulse(float T, float Age)
	{
		const float Tail = Age * 1.25f - T;
		return Age >= 0 && Tail >= 0 && Tail < 0.24f ? std::pow(std::sin(TAU * 0.5f * Tail / 0.24f), 2.0f) : 0;
	}

	vec2 Bezier(vec2 A, vec2 B, vec2 C, vec2 D, float T)
	{
		const float U = 1 - T;
		return A * (U * U * U) + B * (3 * U * U * T) + C * (3 * U * T * T) + D * (T * T * T);
	}

	// Solid stroke core plus a one-physical-pixel alpha fringe. Geometry is
	// expressed in UI coordinates; only the fringe depends on framebuffer DPI.
	void Ribbon(IGraphics *pGraphics, vec2 A, vec2 B, vec2 NA, vec2 NB, float Radius, ColorRGBA Color, const ColorRGBA *pEndColor = nullptr)
	{
		const vec2 Delta = B - A;
		if(length(Delta) < 0.0001f)
			return;
		const float Fringe = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
		const float Core = std::max(0.0f, Radius - Fringe * 0.5f);
		ColorRGBA EndColor = pEndColor ? *pEndColor : Color;
		// Preserve coverage of strokes thinner than one framebuffer pixel.
		const float Coverage = std::min(1.0f, 2 * Radius / Fringe);
		Color.a *= Coverage;
		EndColor.a *= Coverage;
		pGraphics->SetColor4(Color, EndColor, Color, EndColor);
		IGraphics::CFreeformItem Body(A - NA * Core, B - NB * Core, A + NA * Core, B + NB * Core);
		pGraphics->QuadsDrawFreeform(&Body, 1);
		const ColorRGBA Clear(Color.r, Color.g, Color.b, 0);
		for(int Side : {-1, 1})
		{
			pGraphics->SetColor4(Color, EndColor, Clear, ColorRGBA(EndColor.r, EndColor.g, EndColor.b, 0));
			IGraphics::CFreeformItem Quad(A + NA * (Core * Side), B + NB * (Core * Side), A + NA * ((Radius + Fringe * 0.5f) * Side), B + NB * ((Radius + Fringe * 0.5f) * Side));
			pGraphics->QuadsDrawFreeform(&Quad, 1);
		}
	}

	void Stroke(IGraphics *pGraphics, vec2 A, vec2 B, float Radius, ColorRGBA Color)
	{
		if(distance(A, B) < 0.0001f)
			return;
		const vec2 N = normalize(vec2(A.y - B.y, B.x - A.x));
		Ribbon(pGraphics, A, B, N, N, Radius, Color);
	}

	struct CIconGeometry
	{
		struct CSegment
		{
			vec2 m_A, m_B, m_NA, m_NB;
		};
		std::array<CSegment, 256> m_aSegments;
		int m_Count = 0;
		void Line(vec2 A, vec2 B)
		{
			if(distance(A, B) > 0.0001f)
				m_aSegments[m_Count++] = {A, B, {}, {}};
		}
		void Join()
		{
			// Shared miter vertices prevent alpha overdraw at tessellation seams.
			for(int i = 0; i < m_Count; ++i)
			{
				auto &S = m_aSegments[i];
				const vec2 N = normalize(vec2(S.m_A.y - S.m_B.y, S.m_B.x - S.m_A.x));
				S.m_NA = S.m_NB = N;
				for(int j = 0; j < m_Count; ++j)
				{
					if(i == j)
						continue;
					const auto &Other = m_aSegments[j];
					const vec2 M = normalize(vec2(Other.m_A.y - Other.m_B.y, Other.m_B.x - Other.m_A.x));
					const float Denom = 1 + dot(N, M);
					if(Denom < 0.5f)
						continue;
					if(distance(S.m_A, Other.m_B) < 0.0001f)
						S.m_NA = (N + M) / Denom;
					if(distance(S.m_B, Other.m_A) < 0.0001f)
						S.m_NB = (N + M) / Denom;
				}
			}
		}

		void Curve(vec2 A, vec2 B, vec2 C, vec2 D)
		{
			for(int i = 0; i < 16; ++i)
				Line(Bezier(A, B, C, D, i / 16.0f), Bezier(A, B, C, D, (i + 1) / 16.0f));
		}
		void RoundBox(float X, float Y, float W, float H, float R)
		{
			constexpr float K = 0.55228475f;
			Line({X + R, Y}, {X + W - R, Y});
			Curve({X + W - R, Y}, {X + W - R + R * K, Y}, {X + W, Y + R - R * K}, {X + W, Y + R});
			Line({X + W, Y + R}, {X + W, Y + H - R});
			Curve({X + W, Y + H - R}, {X + W, Y + H - R + R * K}, {X + W - R + R * K, Y + H}, {X + W - R, Y + H});
			Line({X + W - R, Y + H}, {X + R, Y + H});
			Curve({X + R, Y + H}, {X + R - R * K, Y + H}, {X, Y + H - R + R * K}, {X, Y + H - R});
			Line({X, Y + H - R}, {X, Y + R});
			Curve({X, Y + R}, {X, Y + R - R * K}, {X + R - R * K, Y}, {X + R, Y});
		}
	};

	const std::array<CIconGeometry, 15> &Icons()
	{
		// Tessellated once, no heap or GPU resource ownership. 20 x 20 viewbox.
		static const auto s_Icons = [] {
			std::array<CIconGeometry, 15> G{};
			G[0].Line({-4, -6.5f}, {6, 0});
			G[0].Line({6, 0}, {-4, 6.5f});
			G[0].Line({-4, 6.5f}, {-4, -6.5f});
			G[1].RoundBox(-7, -6.5f, 14, 13, 1.8f);
			for(float X : {-3.5f, 3.5f})
				G[1].Line({X, -6.5f}, {X, 6.5f});
			for(float Y : {-2.2f, 2.2f})
			{
				G[1].Line({-7, Y}, {-3.5f, Y});
				G[1].Line({3.5f, Y}, {7, Y});
			}
			G[2].Line({-6.5f, 6.5f}, {-5, 1});
			G[2].Line({-5, 1}, {3.5f, -7.5f});
			G[2].Curve({3.5f, -7.5f}, {4, -8}, {4.5f, -8}, {5, -7.5f});
			G[2].Line({5, -7.5f}, {7.5f, -5});
			G[2].Curve({7.5f, -5}, {8, -4.5f}, {8, -4}, {7.5f, -3.5f});
			G[2].Line({7.5f, -3.5f}, {-1, 5});
			G[2].Line({-1, 5}, {-6.5f, 6.5f});
			G[2].Line({2, -6}, {6, -2});
			for(float Y : {-6.5f, 2.0f})
			{
				G[3].RoundBox(-7, Y, 14, 5, 1.5f);
				G[3].Line({-4, Y + 2.5f}, {-2, Y + 2.5f});
				G[3].Line({2, Y + 2.5f}, {4.5f, Y + 2.5f});
			}
			for(int i = 0; i < 3; ++i)
			{
				float Y = -6.0f + i * 6;
				float X = i == 1 ? 3.0f : -3.0f;
				G[4].Line({-7, Y}, {X - 2, Y});
				G[4].Line({X + 2, Y}, {7, Y});
				G[4].RoundBox(X - 2, Y - 2, 4, 4, 1.5f);
			}
			G[5].Line({0, -8}, {0, -1});
			G[5].Curve({-4, -5.7f}, {-10, -1.5f}, {-6.5f, 7}, {0, 7});
			G[5].Curve({0, 7}, {6.5f, 7}, {10, -1.5f}, {4, -5.7f});
			// Secondary symbols share the primary icons' 20 DIP viewbox and stroke.
			G[7].RoundBox(-8, -6, 16, 12, 1);
			G[7].RoundBox(-5, -3, 4, 4, 0.5f);
			for(float Y : {-3.0f, 0.0f, 3.0f})
				G[7].Line({2, Y}, {5, Y});
			G[7].Line({-5, 3}, {-1, 3});
			G[8].RoundBox(-8, -8, 16, 16, 8);
			G[8].RoundBox(-3.5f, -8, 7, 16, 3.5f);
			G[8].Line({-8, 0}, {8, 0});
			for(float Side : {-1.0f, 1.0f})
			{
				G[9].Line({0, -5}, {Side * 8, -7});
				G[9].Line({Side * 8, -7}, {Side * 8, 5});
				G[9].Line({Side * 8, 5}, {0, 7});
			}
			G[9].Line({0, 7}, {0, -5});
			G[10].Line({-9, -3}, {0, -7});
			G[10].Line({0, -7}, {9, -3});
			G[10].Line({9, -3}, {0, 1});
			G[10].Line({0, 1}, {-9, -3});
			G[10].Line({-5, -1}, {-5, 5});
			G[10].Curve({-5, 5}, {-2, 8}, {2, 8}, {5, 5});
			G[10].Line({5, 5}, {5, -1});
			G[10].Line({9, -3}, {9, 5});
			// Community/chat, rather than an unavailable Solid-font Discord brand.
			G[11].RoundBox(-8, -7, 16, 12, 3);
			G[11].Line({-4, 5}, {-5, 8});
			G[11].Line({-5, 8}, {0, 5});
			for(float X : {-4.0f, 0.0f, 4.0f})
				G[11].Line({X, -1}, {X, 1});
			// Join: arrow entering a door. Stop: a distinct square control.
			G[12].Line({-8, 0}, {3, 0});
			G[12].Line({-1, -4}, {3, 0});
			G[12].Line({3, 0}, {-1, 4});
			G[12].Line({2, -7}, {7, -7});
			G[12].Line({7, -7}, {7, 7});
			G[12].Line({7, 7}, {2, 7});
			G[13].RoundBox(-6, -6, 12, 12, 1);
			G[14].Line({8, 0}, {-7, 0});
			G[14].Line({-7, 0}, {-2, -5});
			G[14].Line({-7, 0}, {-2, 5});
			for(auto &Icon : G)
				Icon.Join();
			return G;
		}();
		return s_Icons;
	}

	const std::array<vec2, 21> &DotDirections()
	{
		static const auto s_Directions = [] {
			std::array<vec2, 21> Points{};
			for(int i = 0; i <= 20; ++i)
				Points[i] = vec2(std::cos(TAU * i / 20), std::sin(TAU * i / 20));
			return Points;
		}();
		return s_Directions;
	}

	struct CContour
	{
		static constexpr int SEGMENTS = 132;
		float m_Width = -1, m_Height = -1;
		std::array<vec2, SEGMENTS + 1> m_aPoints, m_aNormals;
		std::array<float, SEGMENTS + 1> m_aDistance;
		void Update(float W, float H)
		{
			if(W == m_Width && H == m_Height)
				return;
			m_Width = W;
			m_Height = H;
			const float R = std::min(10.0f, H / 2);
			const vec2 Centers[] = {{W - R, R}, {W - R, H - R}, {R, H - R}, {R, R}};
			// Exact tangent endpoints and shared analytic normals keep the straight
			// edges and circular corners joined, including their coverage fringes.
			for(int Side = 0; Side < 4; ++Side)
				for(int Step = 0; Step <= 32; ++Step)
				{
					const int i = Side * 33 + Step;
					const float A = (-1 + Side + Step / 32.0f) * TAU / 4;
					m_aNormals[i] = vec2(std::cos(A), std::sin(A));
					m_aPoints[i] = Centers[Side] + m_aNormals[i] * R;
				}
			m_aPoints[SEGMENTS] = m_aPoints[0];
			m_aNormals[SEGMENTS] = m_aNormals[0];
			m_aDistance[0] = 0;
			for(int i = 1; i <= SEGMENTS; ++i)
				m_aDistance[i] = m_aDistance[i - 1] + distance(m_aPoints[i - 1], m_aPoints[i]);
			for(int i = 1; i <= SEGMENTS; ++i)
				m_aDistance[i] /= m_aDistance[SEGMENTS];
		}
	};

	void Star(IGraphics *pGraphics, vec2 Center, float Pixel, float Brightness)
	{
		// An astroid has four slender, smoothly tapering rays. Shared vertices
		// form a white-to-pink fan and a subtle, texture-free coverage halo.
		constexpr int Segments = 96;
		for(int i = 0; i < Segments; ++i)
		{
			const auto Point = [Pixel](int Index) {
				const float A = TAU * (Index % Segments) / Segments;
				const float C = std::cos(A), S = std::sin(A);
				return vec2(7.0f * C * C * C * C * C, 9.4f * S * S * S * S * S) * Pixel;
			};
			const vec2 A = Point(i), B = Point(i + 1);
			const ColorRGBA White(1, 1, 1, Brightness);
			const ColorRGBA Pink(1, 0.77f, 0.88f, Brightness * 0.65f);
			const ColorRGBA Clear(1, 0.77f, 0.88f, 0);
			pGraphics->SetColor4(White, Pink, White, Pink);
			IGraphics::CFreeformItem Core(Center, Center + A, Center, Center + B);
			pGraphics->QuadsDrawFreeform(&Core, 1);
			// Radial expansion shares endpoints even at the concave ray valleys.
			const vec2 OA = A + normalize(A) * Pixel, OB = B + normalize(B) * Pixel;
			pGraphics->SetColor4(Pink, Pink, Clear, Clear);
			IGraphics::CFreeformItem Edge(Center + A, Center + B, Center + OA, Center + OB);
			pGraphics->QuadsDrawFreeform(&Edge, 1);
		}
	}

}

// A short orbit-relative cubic carries selection light into the scene. The
// astroid has four tapered cusps and concave sides, with a DPI-sized alpha fringe.
void CLucentMenuEffects::SceneCurve(IGraphics *pGraphics, vec2 A, vec2 B, vec2 C, vec2 D, float Alpha)
{
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	constexpr int Steps = 64;
	for(int i = 0; i < Steps; ++i)
	{
		const float T = float(i) / Steps, U = float(i + 1) / Steps;
		const float Light = std::pow(std::sin((T + U) * .25f * TAU), 2.f) * Alpha;
		Stroke(pGraphics, Bezier(A, B, C, D, T), Bezier(A, B, C, D, U), .35f, ColorRGBA(1, .81f, .91f, Light));
	}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::SelectionTransfer(IGraphics *pGraphics, vec2 From, vec2 To, float Progress, float Alpha)
{
	if(Progress <= 0 || Progress >= 1 || Alpha <= 0)
		return;
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	const vec2 B = From + vec2(28, -9), C = To - vec2(25, 5);
	const float Head = std::min(1.f, Progress * 1.45f);
	for(int i = 0; i < 32; ++i)
	{
		const float T = i / 32.f, Next = (i + 1) / 32.f;
		const float Tail = Head - T;
		if(Tail < 0 || Tail > .32f)
			continue;
		const float Light = std::pow(std::sin(Tail / .32f * TAU * .5f), 2.f) * std::sin(Progress * TAU * .5f) * Alpha;
		const vec2 A = Bezier(From, B, C, To, T), D = Bezier(From, B, C, To, Next);
		Stroke(pGraphics, A, D, 1.6f, ColorRGBA(1, .65f, .82f, .10f * Light));
		Stroke(pGraphics, A, D, .38f, ColorRGBA(1, .85f, .93f, .85f * Light));
	}
	const float StarTime = std::clamp((Progress - .30f) / .70f, 0.f, 1.f);
	const float Light = std::sin(StarTime * TAU * .5f) * Alpha;
	if(Light > .001f)
	{
		const float Radius = 7.f * (.85f + .15f * StarTime);
		const float Fringe = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
		const auto Point = [&](float Angle) {
			return SelectionSparklePoint(To, Radius, Angle);
		};
		for(int i = 0; i < 64; ++i)
		{
			const float T = TAU * i / 64, U = TAU * (i + 1) / 64;
			const vec2 A = Point(T), D = Point(U);
			const auto Normal = [&](float Angle) {
				const vec2 Tangent = Point(Angle + .01f) - Point(Angle - .01f);
				return normalize(vec2(Tangent.y, -Tangent.x));
			};
			const ColorRGBA Core(1, .96f, .98f, Light), Edge(1, .79f, .90f, Light);
			pGraphics->SetColor4(Core, Edge, Core, Edge);
			IGraphics::CFreeformItem Fill(To, A, To, D);
			pGraphics->QuadsDrawFreeform(&Fill, 1);
			pGraphics->SetColor4(Edge, Edge, Edge.WithAlpha(0), Edge.WithAlpha(0));
			IGraphics::CFreeformItem Rim(A, D, A + Normal(T) * Fringe, D + Normal(U) * Fringe);
			pGraphics->QuadsDrawFreeform(&Rim, 1);
		}
	}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::FocusStar(IGraphics *pGraphics, vec2 Center, float Scale, float Alpha)
{
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	Star(pGraphics, Center, Scale, Alpha);
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::Orbit(IGraphics *pGraphics, vec2 Center, float Scale, double Time, const float *pHighlight, bool Foreground, float Energy, float Alpha)
{
	// Independent periodic flows on a shared ellipse. Depth follows position,
	// so the rear hemisphere is occluded by the wordmark.
	constexpr float Starts[] = {2.60f, 4.45f, -0.35f};
	constexpr float Ends[] = {4.05f, 5.50f, 1.20f};
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	for(int Arc = 0; Arc < 3; ++Arc)
	{
		const float Phase = OrbitPhase(Time, Arc);
		const float Breath = 0.94f + 0.06f * std::sin(static_cast<float>(std::fmod(Time, 10.0) * TAU / 10) + Arc);
		for(int i = 0; i < 56; ++i)
		{
			const float T = (i + 0.5f) / 56;
			const float Angle = mix(Starts[Arc], Ends[Arc], T) + Phase;
			const vec2 Mid = OrbitPoint(Center, Scale, Angle);
			if(Foreground != (Mid.y > Center.y + 30 * Scale))
				continue;
			float Reaction = 0;
			for(int Chip = 0; pHighlight && Chip < 6; ++Chip)
			{
				if(pHighlight[Chip] <= 0.001f)
					continue;
				const float Direction = OrbitDirection(Chip);
				const float Delta = std::atan2(std::sin(Direction - Starts[Arc] - Phase), std::cos(Direction - Starts[Arc] - Phase));
				const float Nearest = Delta >= 0 && Delta <= Ends[Arc] - Starts[Arc] ? 1.0f : std::max(std::cos(Direction - Starts[Arc] - Phase), std::cos(Direction - Ends[Arc] - Phase));
				Reaction = std::max(Reaction, pHighlight[Chip] * (0.30f * std::pow((Nearest + 1) * 0.5f, 8.0f) + 0.7f * std::pow(std::max(0.0f, std::cos(Angle - Direction)), 12.0f)));
			}
			const float Head = static_cast<float>(std::fmod(Time / (2.8 + Arc * 0.4) + Arc * 0.31, 1.0));
			const float Dist = std::abs(T - Head);
			const float Pulse = std::pow(std::max(0.0f, 1 - Dist / 0.16f), 3.0f);
			const float Taper = std::min(1.0f, std::min(T, 1 - T) * 9);
			const vec2 A = OrbitPoint(Center, Scale, mix(Starts[Arc], Ends[Arc], i / 56.0f) + Phase);
			const vec2 B = OrbitPoint(Center, Scale, mix(Starts[Arc], Ends[Arc], (i + 1) / 56.0f) + Phase);
			Stroke(pGraphics, A, B, Pixel * 3.0f, ColorRGBA(1, 0.65f, 0.81f, Alpha * Taper * (0.025f + Reaction * 0.08f)));
			Stroke(pGraphics, A, B, Pixel * (0.85f + 0.12f * Breath + Energy * 0.15f), ColorRGBA(1, 0.81f, 0.91f, Alpha * Taper * (0.22f * Breath + Reaction * 0.52f + Pulse * (0.45f + Energy * 0.2f) + Energy * 0.08f)));
		}
	}
	// Keep the light anchors on their original orbit and period.
	if(!Foreground)
		for(float Angle : {3.12f, 5.15f})
		{
			const float Phase = static_cast<float>(std::fmod(Time, 9.7) / 9.7) * TAU;
			const vec2 P = OrbitPoint(Center, Scale, Angle + Phase);
			Star(pGraphics, P, Pixel, Alpha * (0.78f + 0.12f * std::sin(Phase * 2 + Angle)));
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
		// T=0 is the orbit. Keep the cubic fixed even as the arcs rotate.
		const auto ColorAt = [&](float T) {
			const float U = std::clamp(T / 0.27f, 0.0f, 1.0f);
			const float Dissolve = U * U * (3 - 2 * U);
			const float Pulse = ConnectionPulse(T, PulseAge);
			return ColorRGBA(1, 0.88f - T * 0.13f, 0.94f - T * 0.06f,
				Progress * Dissolve * ((0.13f + Highlight * 0.104f) + Pulse * Highlight * 0.12f));
		};
		const float T0 = 1 - i * Reveal / Segments, T1 = 1 - (i + 1) * Reveal / Segments;
		const vec2 P0 = Bezier(A, B, C, D, T0), P1 = Bezier(A, B, C, D, T1);
		const auto Normal = [&](float T) {
			const vec2 V = (B - A) * (3 * (1 - T) * (1 - T)) + (C - B) * (6 * (1 - T) * T) + (D - C) * (3 * T * T);
			return normalize(vec2(-V.y, V.x));
		};
		const ColorRGBA C0 = ColorAt(T0), C1 = ColorAt(T1);
		Ribbon(pGraphics, P0, P1, Normal(T0), Normal(T1), Pixel * 1.3f, C0, &C1);
	}
	pGraphics->QuadsEnd();
}

void CLucentMenuEffects::Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, float Highlight, double Time, int Glyph, float ContentAlpha, float ContentOffset, bool ContentOnly)
{
	const bool Secondary = Icon >= 6;
	const float Strength = Secondary ? 0.75f : 1.0f;
	const float Pixel = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
	const float Border = 0.7f + Highlight * 0.25f;
	const vec2 Offset(Rect.x, Rect.y);
	const vec2 Center(Rect.x + Rect.w / 2, Rect.y + Rect.h / 2);
	const vec2 Origin(Rect.x + ContentOffset + std::min(18.0f, Rect.w * 0.14f), Rect.y + Rect.h / 2);
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	if(!ContentOnly)
	{
		static std::array<CContour, 13> s_Contours;
		auto &Contour = s_Contours[std::clamp(Icon, 0, 12)];
		Contour.Update(Rect.w, Rect.h);
		const float Head = static_cast<float>(std::fmod(Time / 1.8, 1.0));
		const ColorRGBA Fill(0.065f + (Icon == 0 ? 0.012f : 0) + Highlight * 0.016f, 0.056f, 0.078f + Highlight * 0.013f, Icon == 6 ? Alpha * Highlight * 0.45f : Alpha);
		for(int i = 0; i < CContour::SEGMENTS; ++i)
		{
			const vec2 NA = Contour.m_aNormals[i], NB = Contour.m_aNormals[i + 1];
			const vec2 A = Offset + Contour.m_aPoints[i], B = Offset + Contour.m_aPoints[i + 1];
			const vec2 IA = A - NA * (Pixel * 0.5f), IB = B - NB * (Pixel * 0.5f);
			pGraphics->SetColor(Fill);
			IGraphics::CFreeformItem Body(Center, IA, Center, IB);
			pGraphics->QuadsDrawFreeform(&Body, 1);
			pGraphics->SetColor4(Fill, Fill, ColorRGBA(Fill.r, Fill.g, Fill.b, 0), ColorRGBA(Fill.r, Fill.g, Fill.b, 0));
			IGraphics::CFreeformItem Edge(IA, IB, A + NA * (Pixel * 0.5f), B + NB * (Pixel * 0.5f));
			pGraphics->QuadsDrawFreeform(&Edge, 1);
		}
		for(int i = 0; i < CContour::SEGMENTS; ++i)
		{
			const vec2 NA = Contour.m_aNormals[i], NB = Contour.m_aNormals[i + 1];
			const vec2 A = Offset + Contour.m_aPoints[i] - NA * (Border * 0.5f);
			const vec2 B = Offset + Contour.m_aPoints[i + 1] - NB * (Border * 0.5f);
			if(Highlight > 0.001f)
				for(int Layer = 3; Layer > 0; --Layer)
					Ribbon(pGraphics, A, B, NA, NB, Border * 0.5f + Layer * 1.4f, ColorRGBA(1, 0.66f, 0.83f, Alpha * Highlight * 0.018f * Strength));
			Ribbon(pGraphics, A, B, NA, NB, Border * 0.5f, ColorRGBA(1, 0.81f + Highlight * 0.09f, 0.91f + Highlight * 0.05f, Alpha * ((Icon == 6 ? 0.0f : Icon == 0 ? 0.23f :
																						   0.14f) +
																			  Highlight * 0.62f) *
																		  Strength));
			if(Highlight > 0.001f)
			{
				const auto LightAt = [Head, Highlight, Alpha](float T) {
					const float Tail = std::fmod(Head - T + 1, 1.0f);
					const float Light = Tail < 0.22f ? std::pow(std::sin(TAU * 0.5f * Tail / 0.22f), 2.0f) : 0;
					return ColorRGBA(1, 0.88f, 0.95f, Light * Highlight * Alpha * 0.85f);
				};
				// Sample straight spans too, and interpolate endpoint alpha to avoid
				// stepping at the moving highlight's head and between corner segments.
				const int Steps = std::max(1, static_cast<int>(std::ceil(distance(A, B) / (2 * Pixel))));
				for(int Step = 0; Step < Steps; ++Step)
				{
					const float T0 = Step / static_cast<float>(Steps), T1 = (Step + 1) / static_cast<float>(Steps);
					const ColorRGBA C0 = LightAt(mix(Contour.m_aDistance[i], Contour.m_aDistance[i + 1], T0));
					const ColorRGBA C1 = LightAt(mix(Contour.m_aDistance[i], Contour.m_aDistance[i + 1], T1));
					Ribbon(pGraphics, mix(A, B, T0), mix(A, B, T1), mix(NA, NB, T0), mix(NA, NB, T1), 0.65f, C0, &C1);
				}
			}
		}
	}
	Alpha *= ContentAlpha;
	const ColorRGBA Color(0.88f + Highlight * 0.12f, 0.85f + Highlight * 0.1f, 0.92f + Highlight * 0.06f, Alpha * (0.82f + Highlight * 0.18f));
	const auto &Geometry = Icons()[Glyph < 0 ? Icon : Glyph];
	const float IconScale = std::min(1.0f, Rect.h / 24.0f);
	for(int i = 0; i < Geometry.m_Count; ++i)
		Ribbon(pGraphics, Origin + Geometry.m_aSegments[i].m_A * IconScale, Origin + Geometry.m_aSegments[i].m_B * IconScale, Geometry.m_aSegments[i].m_NA, Geometry.m_aSegments[i].m_NB, 0.85f * IconScale, Color);
	if(Icon == 6)
		for(float X : {-5.0f, 0.0f, 5.0f})
		{
			const vec2 Center = Origin + vec2(X, 0);
			const float Fringe = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
			for(int i = 0; i < 20; ++i)
			{
				const vec2 A = DotDirections()[i];
				const vec2 B = DotDirections()[i + 1];
				pGraphics->SetColor(Color);
				IGraphics::CFreeformItem Core(Center, Center + A, Center, Center + B);
				pGraphics->QuadsDrawFreeform(&Core, 1);
				pGraphics->SetColor4(Color, Color, ColorRGBA(Color.r, Color.g, Color.b, 0), ColorRGBA(Color.r, Color.g, Color.b, 0));
				IGraphics::CFreeformItem Edge(Center + A, Center + B, Center + A * (1 + Fringe), Center + B * (1 + Fringe));
				pGraphics->QuadsDrawFreeform(&Edge, 1);
			}
		}

	pGraphics->QuadsEnd();
	pGraphics->SetColor(1, 1, 1, 1);
}
