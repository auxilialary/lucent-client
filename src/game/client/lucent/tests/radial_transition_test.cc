#include "../radial_transition.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(Expression) \
	do \
	{ \
		if(!(Expression)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #Expression); \
			std::abort(); \
		} \
	} while(false)

int main()
{
	const vec2 TopLeft(0, 0), Size(1066, 600);
	const vec2 aOrigins[] = {vec2(0, 0), vec2(1066, 0), vec2(0, 600), Size, Size / 2, vec2(190, 280)};
	for(const vec2 Origin : aOrigins)
		for(int Fps : {30, 60, 144, 240})
		{
			CRadialTransition Transition;
			CHECK(!Transition.Active() && !Transition.Completed());
			CHECK(Transition.Start(10, Origin, true));
			CHECK(Transition.Coverage(Origin, TopLeft, Size, 40) == 0);
			CHECK(!Transition.Start(10.1, Size / 2, true));
			CHECK(Transition.Origin() == Origin);
			float Previous = 0;
			for(int Frame = 1; Frame <= Fps; ++Frame)
			{
				Transition.Update(10 + static_cast<double>(Frame) / Fps, true);
				CHECK(Transition.Progress() >= Previous);
				Previous = Transition.Progress();
			}
			CHECK(Transition.Completed());
			for(const vec2 Corner : {TopLeft, vec2(Size.x, 0), vec2(0, Size.y), Size})
			{
				CHECK(Transition.MaxRadius(TopLeft, Size) >= distance(Origin, Corner));
				CHECK(Transition.Coverage(Corner, TopLeft, Size, 40) == 1);
			}
			CHECK(!Transition.Start(12, Origin, true));
			Transition.Reset();
			CHECK(Transition.Start(20, Origin, false));
			CHECK(Transition.Completed());
		}
	CRadialTransition Transition;
	Transition.Start(0, vec2(0, 0), true);
	Transition.Update(0.2, true);
	const float Progress = Transition.Progress();
	const float Radius = Transition.Radius(TopLeft, Size, 40);
	CHECK(Transition.Coverage(vec2(Radius - 40, 0), TopLeft, Size, 40) > 0.999f);
	CHECK(std::abs(Transition.Coverage(vec2(Radius - 20, 0), TopLeft, Size, 40) - 0.5f) < 0.001f);
	CHECK(Transition.Coverage(vec2(Radius, 0), TopLeft, Size, 40) == 0);
	Transition.Update(0.1, true);
	CHECK(Transition.Progress() == Progress);
	Transition.Update(0.2, false);
	CHECK(Transition.Completed());
	Transition.Reset();
	Transition.Start(0, Size / 2, true);
	Transition.Update(3600, true); // Focus loss / suspended frame.
	CHECK(Transition.Completed());
	// The renderer uses a precomputed mask; its coordinates also work for
	// offset viewports and a changed aspect ratio without restarting time.
	const vec2 Offset(30, 20), Resized(1600, 600);
	for(int Visit = 0; Visit < 1000; ++Visit)
	{
		Transition.Reset();
		CHECK(Transition.Start(Visit, Offset + Size / 2, true));
		Transition.Update(Visit + 0.25, true);
		const auto Mask = Transition.Mask(Offset, Resized, 40);
		for(const vec2 Point : {Offset, Offset + Resized, Offset + Size / 2})
			CHECK(Mask.Coverage(Point) == Transition.Coverage(Point, Offset, Resized, 40));
		Transition.Update(Visit + 1.0, true);
		CHECK(Transition.Completed());
		CHECK(Transition.Coverage(Offset + Resized, Offset, Resized, 40) == 1);
	}
	// Exact deadline, repeat-click ownership, interruption and restart.
	Transition.Reset();
	const vec2 Click(215.25f, 386.75f);
	CHECK(Transition.Start(0, Click, true));
	Transition.Update(0.52, true);
	CHECK(Transition.Active());
	CHECK(!Transition.Start(0.53, vec2(0, 0), true));
	CHECK(Transition.Origin() == Click);
	Transition.Update(CRadialTransition::DURATION - 0.0001, true);
	CHECK(Transition.Active());
	Transition.Update(CRadialTransition::DURATION, true);
	CHECK(Transition.Completed());
	const auto CompleteMask = Transition.Mask(TopLeft, Size, 24);
	for(const vec2 Corner : {TopLeft, vec2(Size.x, 0), vec2(0, Size.y), Size})
		CHECK(CompleteMask.Coverage(Corner) > 0.99999f);
	Transition.Reset(); // Escape / external state change releases ownership.
	CHECK(!Transition.Active() && !Transition.Completed());
	CHECK(Transition.Start(5, Click, true));
	Transition.Update(5.3, true);
	Transition.Update(0, false); // Resize finishes immediately, without stretching cached scenes.
	CHECK(Transition.Completed());
	// Quadratic ease-out still advances meaningfully in the last quarter.
	Transition.Reset();
	Transition.Start(0, Click, true);
	Transition.Update(CRadialTransition::DURATION * 0.75, true);
	const float Radius75 = Transition.Radius(TopLeft, Size, 24);
	CHECK(Radius75 < 0.95f * (Transition.MaxRadius(TopLeft, Size) + 24));
	// Composition is a single convex blend, even at near-zero coverage.
	for(int Step = 0; Step <= 100; ++Step)
	{
		const float X = CompleteMask.m_Radius - 24.0f * Step / 100;
		const float Coverage = CompleteMask.Coverage(Click + vec2(X, 0));
		const float Source = 0.2f, Destination = 0.8f;
		const float Color = Destination * Coverage + Source * (1 - Coverage);
		CHECK(Color >= Source && Color <= Destination);
	}
	std::puts("Radial transition checks passed");
}
