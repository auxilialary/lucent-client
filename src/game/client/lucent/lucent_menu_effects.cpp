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

	vec2 Bezier(vec2 A, vec2 B, vec2 C, vec2 D, float T)
	{
		const float U = 1 - T;
		return A * (U * U * U) + B * (3 * U * U * T) + C * (3 * U * T * T) + D * (T * T * T);
	}

	// Solid stroke core plus a one-physical-pixel alpha fringe. Geometry is
	// expressed in UI coordinates; only the fringe depends on framebuffer DPI.
	void Ribbon(IGraphics *pGraphics, vec2 A, vec2 B, vec2 NA, vec2 NB, float Radius, ColorRGBA Color)
	{
		const vec2 Delta = B - A;
		if(length(Delta) < 0.0001f)
			return;
		const float Fringe = pGraphics->GetScreen().Size().y / pGraphics->ScreenSize().y;
		const float Core = std::max(0.0f, Radius - Fringe * 0.5f);
		pGraphics->SetColor(Color);
		IGraphics::CFreeformItem Body(A - NA * Core, B - NB * Core, A + NA * Core, B + NB * Core);
		pGraphics->QuadsDrawFreeform(&Body, 1);
		const ColorRGBA Clear(Color.r, Color.g, Color.b, 0);
		for(int Side : {-1, 1})
		{
			pGraphics->SetColor4(Color, Color, Clear, Clear);
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

	const std::array<CIconGeometry, 7> &Icons()
	{
		// Tessellated once, no heap or GPU resource ownership. 20 x 20 viewbox.
		static const auto s_Icons = [] {
			std::array<CIconGeometry, 7> G{};
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
		float m_Width = -1, m_Height = -1;
		std::array<vec2, 257> m_aPoints;
		void Update(float W, float H)
		{
			if(W == m_Width && H == m_Height)
				return;
			m_Width = W;
			m_Height = H;
			const float R = std::min(10.0f, H / 2);
			const float L = W - 2 * R, V = H - 2 * R, Q = TAU * R / 4;
			for(int i = 0; i <= 256; ++i)
			{
				float D = (2 * L + 2 * V + 4 * Q) * i / 256;
				vec2 P;
				for(int Side = 0; Side < 4; ++Side)
				{
					const float Span = Side % 2 == 0 ? L : V;
					const vec2 Start[] = {{R, 0}, {W, R}, {W - R, H}, {0, H - R}};
					const vec2 Dir[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
					const vec2 Center[] = {{W - R, R}, {W - R, H - R}, {R, H - R}, {R, R}};
					if(D <= Span)
					{
						P = Start[Side] + Dir[Side] * D;
						break;
					}
					D -= Span;
					if(D <= Q || Side == 3)
					{
						const float A = -TAU / 4 + Side * TAU / 4 + D / R;
						P = Center[Side] + vec2(std::cos(A), std::sin(A)) * R;
						break;
					}
					D -= Q;
				}
				m_aPoints[i] = P;
			}
		}
	};
}

void CLucentMenuEffects::Orbit(IGraphics *pGraphics, vec2 Center, float Scale, double Time, const float *pHighlight, bool Foreground, float Energy)
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
			Stroke(pGraphics, A, B, Pixel * 3.0f, ColorRGBA(1, 0.65f, 0.81f, Taper * (0.025f + Reaction * 0.08f)));
			Stroke(pGraphics, A, B, Pixel * (0.85f + 0.12f * Breath + Energy * 0.15f), ColorRGBA(1, 0.81f, 0.91f, Taper * (0.22f * Breath + Reaction * 0.52f + Pulse * (0.45f + Energy * 0.2f) + Energy * 0.08f)));
		}
	}
	// Two quiet light anchors, rendered as feathered crosses rather than discs.
	if(!Foreground)
		for(float Angle : {3.12f, 5.15f})
		{
			const float Phase = static_cast<float>(std::fmod(Time, 9.7) / 9.7) * TAU;
			const vec2 P = OrbitPoint(Center, Scale, Angle + Phase);
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

void CLucentMenuEffects::Chip(IGraphics *pGraphics, const CUIRect &Rect, int Icon, float Alpha, float Highlight, double Time)
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
	const ColorRGBA Color(0.88f + Highlight * 0.12f, 0.85f + Highlight * 0.1f, 0.92f + Highlight * 0.06f, Alpha * (0.82f + Highlight * 0.18f));
	static std::array<CContour, 13> s_Contours;
	auto &Contour = s_Contours[std::clamp(Icon, 0, 12)];
	Contour.Update(Rect.w, Rect.h);
	const float Head = static_cast<float>(std::fmod(Time / 1.8, 1.0));
	for(int i = 0; i < 256 && Highlight > 0.001f; ++i)
	{
		const float Tail = std::fmod(Head - (i + 0.5f) / 256 + 1, 1.0f);
		const float Light = std::pow(std::max(0.0f, 1 - Tail / 0.22f), 2.0f);
		Stroke(pGraphics, vec2(Rect.x, Rect.y) + Contour.m_aPoints[i], vec2(Rect.x, Rect.y) + Contour.m_aPoints[i + 1], 0.65f, ColorRGBA(1, 0.88f, 0.95f, Light * Highlight * Alpha * 0.85f));
	}
	const auto &Geometry = Icons()[Icon < 6 ? Icon : Icon == 12 ? 3 :
								      6];
	const float IconScale = std::min(1.0f, Rect.h / 24.0f);
	for(int i = 0; i < Geometry.m_Count; ++i)
		Ribbon(pGraphics, Origin + Geometry.m_aSegments[i].m_A * IconScale, Origin + Geometry.m_aSegments[i].m_B * IconScale, Geometry.m_aSegments[i].m_NA, Geometry.m_aSegments[i].m_NB, 0.85f * IconScale, Color);
	if(Icon >= 6 && Icon != 12)
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
