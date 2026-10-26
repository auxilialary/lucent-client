// Standalone geometry/temporal checks against the production tessellation.
// Rendering is intentionally unavailable in this CPU-only test.
#include <game/client/lucent/ui/scene_effects.cpp>

#include <cstdio>
#include <cstdlib>

#define CHECK(Expr) \
	do \
	{ \
		if(!(Expr)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #Expr); \
			std::abort(); \
		} \
	} while(false)

void CUIRect::Draw(ColorRGBA, int, float) const
{
	std::abort();
}

int main()
{
	// Production sparkle: four 7-DIP cusps and concave, tapered sides.
	for(int i = 0; i < 4; ++i)
	{
		CHECK(std::abs(length(SelectionSparklePoint(vec2(0, 0), 7, i * TAU / 4)) - 7) < .001f);
		CHECK(length(SelectionSparklePoint(vec2(0, 0), 7, (i + .5f) * TAU / 4)) < 3.51f);
	}
	for(int i = 0; i <= 64; ++i)
	{
		const float Angle = i * TAU / 64;
		const vec2 Tangent = SelectionSparklePoint(vec2(0, 0), 7, Angle + .01f) - SelectionSparklePoint(vec2(0, 0), 7, Angle - .01f);
		CHECK(length(Tangent) > 0 && std::isfinite(normalize(Tangent).x));
	}
	// One pulse moves from the orbit towards the button and never loops.
	CHECK(ConnectionPulse(0.1f, -1) == 0);
	CHECK(ConnectionPulse(0.1f, 0) == 0);
	CHECK(ConnectionPulse(0.1f, 0.176f) > 0.99f);
	CHECK(ConnectionPulse(0.8f, 0.176f) == 0);
	CHECK(ConnectionPulse(0.8f, 0.736f) > 0.99f);
	CHECK(ConnectionPulse(0.1f, 0.736f) == 0);
	CHECK(ConnectionPulse(0.8f, 2.536f) == 0);
	for(const auto &Icon : Icons())
	{
		CHECK(Icon.m_Count < static_cast<int>(Icon.m_aSegments.size()));
		for(int i = 0; i < Icon.m_Count; ++i)
		{
			const auto &S = Icon.m_aSegments[i];
			CHECK(distance(S.m_A, S.m_B) > 0);
			CHECK(std::abs(S.m_A.x) <= 10 && std::abs(S.m_A.y) <= 10);
			CHECK(std::isfinite(S.m_NA.x) && std::isfinite(S.m_NB.y));
			CHECK(length(S.m_NA) <= 2 && length(S.m_NB) <= 2);
		}
	}
	CContour Contour;
	for(const vec2 Size : {vec2(164, 34), vec2(84, 26), vec2(82, 34), vec2(120, 34)})
	{
		Contour.Update(Size.x, Size.y);
		CHECK(distance(Contour.m_aPoints.front(), Contour.m_aPoints.back()) < 0.001f);
		for(size_t i = 0; i < Contour.m_aPoints.size(); ++i)
		{
			const vec2 P = Contour.m_aPoints[i];
			CHECK(P.x >= -0.001f && P.x <= Size.x + 0.001f && P.y >= -0.001f && P.y <= Size.y + 0.001f);
			if(i)
			{
				const vec2 Previous = Contour.m_aPoints[i - 1];
				if(i % 33 == 0)
				{
					// Tangent endpoints span an entire straight edge.
					CHECK(std::abs(P.x - Previous.x) < 0.001f || std::abs(P.y - Previous.y) < 0.001f);
					CHECK(distance(Contour.m_aNormals[i], Contour.m_aNormals[i - 1]) < 0.001f);
				}
				else
					CHECK(distance(P, Previous) < 2);
			}
		}
	}
	for(int Arc = 0; Arc < 3; ++Arc)
	{
		const auto Position = [Arc](double T) { return OrbitPoint(vec2(0, 0), 1, OrbitPhase(T, Arc)); };
		const double Period = ORBIT_PERIODS[Arc];
		CHECK(distance(Position(0), Position(Period)) < 0.001f);
		CHECK(distance(Position(Period - 0.00001), Position(Period + 0.00001)) < 0.01f);
		for(int Fps : {60, 144, 240})
		{
			vec2 Previous = Position(0);
			for(int Frame = 1; Frame <= Fps * 36; ++Frame)
			{
				const vec2 P = Position(static_cast<double>(Frame) / Fps);
				CHECK(distance(P, Previous) <= 130.0f / Fps);
				Previous = P;
			}
			CHECK(distance(Previous, Position(36)) < 0.001f);
		}
	}
	std::puts("Vector bounds, finite joins, resized closed contours and 36-second orbital trajectories at 60/144/240 FPS passed");
}
