#include <game/client/lucent/game/ping.h>
#include <game/client/lucent/game/ping_geometry.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(X) \
	do \
	{ \
		if(!(X)) \
		{ \
			std::fprintf(stderr, "line %d: %s\n", __LINE__, #X); \
			std::abort(); \
		} \
	} while(false)
int main()
{
	using namespace LucentPing;
	const vec2 Min(-500, -300), Size(1000, 600), View(1000, 600);
	const vec2 Points[] = {{-1000, 0}, {1000, 0}, {0, -1000}, {0, 1000}, {-1000, -600}, {1000, -600}, {-1000, 600}, {1000, 600}};
	for(auto Point : Points)
	{
		auto P = Project(Point, Min, Size, View, 40);
		CHECK(P.m_Offscreen);
		CHECK(P.m_Position.x >= 39.99 && P.m_Position.x <= 960.01);
		CHECK(P.m_Position.y >= 39.99 && P.m_Position.y <= 560.01);
		CHECK(std::abs(P.m_Position.x - 40) < .01 || std::abs(P.m_Position.x - 960) < .01 || std::abs(P.m_Position.y - 40) < .01 || std::abs(P.m_Position.y - 560) < .01);
		CHECK(std::abs(length(P.m_Direction) - 1) < .001);
	}
	CHECK(distance(Project(vec2(-1000, 0), Min, Size, View, 40).m_Position, vec2(40, 300)) < .001f);
	CHECK(distance(Project(vec2(1000, 0), Min, Size, View, 40).m_Position, vec2(960, 300)) < .001f);
	CHECK(distance(Project(vec2(0, -1000), Min, Size, View, 40).m_Position, vec2(500, 40)) < .001f);
	CHECK(distance(Project(vec2(0, 1000), Min, Size, View, 40).m_Position, vec2(500, 560)) < .001f);
	const auto Resized = Project(vec2(2000, 0), Min, Size, vec2(1600, 900), 40);
	CHECK(Resized.m_Offscreen && distance(Resized.m_Position, vec2(1560, 450)) < .001f);
	CHECK(Project(vec2(0, 0), Min, Size, View, 40).m_Position == View / 2.f);
	CHECK(!Project(vec2(500, 300), Min, Size, View, 40).m_Offscreen);
	CHECK(Project(vec2(600, 0), Min, Size, View, 40).m_Offscreen);
	CHECK(!Project(vec2(600, 0), vec2(-1000, -600), Size * 2.f, View, 40).m_Offscreen);
	CHECK(distance(Project(vec2(0, 0), Min, Size, vec2(1600, 900), 40).m_Position, vec2(800, 450)) < .001f);
	CHECK(Project(vec2(100, 50), vec2(-400, -250), Size, View, 40).m_Position == View / 2.f);
	CHECK(DistanceTiles(vec2(0, 0), vec2(0, 0)) == 0);
	CHECK(DistanceTiles(vec2(0, 0), vec2(96, 128)) == 5);
	const vec2 Camera(320, 0), Local(0, 0), Follow(160, 0), Ping(1360, 0);
	CHECK(DistanceTiles(DistanceOrigin(EDistanceView::Gameplay, Camera, &Local, &Follow), Ping) == 42.5f);
	CHECK(DistanceTiles(DistanceOrigin(EDistanceView::Follow, Camera, &Local, &Follow), Ping) == 37.5f);
	CHECK(DistanceTiles(DistanceOrigin(EDistanceView::Freeview, Camera, &Local, &Follow), Ping) == 32.5f);
	CHECK(DistanceOrigin(EDistanceView::Follow, Camera, &Local, nullptr) == Camera);
	CHECK(DistanceOrigin(EDistanceView::Gameplay, Camera, nullptr, nullptr) == Camera);
	for(const vec2 Viewport : {View, vec2(600, 900), vec2(1600, 600)})
	{
		for(auto Point : Points)
		{
			const auto Projection = Project(Point * 4.f, Min, Size, Viewport, 48);
			for(float Width : {30.f, 90.f, 180.f})
			{
				const vec2 LabelSize(Width, 15);
				const auto Label = DistanceLabelPosition(Projection.m_Position, Viewport, LabelSize);
				CHECK(Label.x >= 4 && Label.y >= 4);
				CHECK(Label.x + Width <= Viewport.x - 4 && Label.y + 15 <= Viewport.y - 4);
			}
		}
	}
	CHECK(DistanceLabelPosition(vec2(48, 300), View, vec2(60, 15)).x > 48);
	CHECK(DistanceLabelPosition(vec2(952, 300), View, vec2(60, 15)).x + 60 < 952);
	CHECK(DistanceLabelPosition(vec2(500, 48), View, vec2(60, 15)).y > 48);
	CHECK(DistanceLabelPosition(vec2(500, 552), View, vec2(60, 15)).y + 15 < 552);
	CState State;
	State.Add(vec2(0, 0), 0);
	CHECK(State.Events()[0].Alpha(0) == 1);
	CHECK(std::abs(State.Events()[0].Alpha(3.7) - .5) < .001);
	CHECK(State.Events()[0].Alpha(4) == 0);
	State.Update(4);
	CHECK(State.Events().empty());
	for(int i = 0; i < 9; ++i)
		State.Add(vec2(float(i), 0), 10 + i * .1);
	CHECK(State.Events().size() == 8);
	CHECK(State.Events().front().m_Position.x == 1);
	CHECK(State.Events().front().m_Id < State.Events().back().m_Id);
	State.Clear();
	CHECK(State.Events().empty());
	State.Add(vec2(0, 0), 20);
	State.Update(24);
	CHECK(State.Events().empty());
	for(const vec2 V : {vec2(320, 600), vec2(1000, 600), vec2(1600, 900)})
	{
		const vec2 LabelSize = SenderLabelSize(10000, 45, V, true);
		CHECK(LabelSize.x <= 132 && LabelSize.x <= V.x - 8);
		CHECK(LabelSize.y == 34);
		for(const vec2 Marker : {vec2(48, V.y / 2), vec2(V.x - 48, V.y / 2), vec2(V.x / 2, 48), vec2(V.x / 2, V.y - 48), vec2(48, 48), vec2(V.x - 48, 48), vec2(48, V.y - 48), vec2(V.x - 48, V.y - 48)})
		{
			std::vector<CIndicatorLayout> Layouts;
			for(int i = 0; i < 8; ++i)
			{
				const auto Layout = PlaceIndicator(Marker, V, LabelSize, Layouts);
				CHECK(Layout.m_Label.x >= 4 && Layout.m_Label.y >= 4);
				CHECK(Layout.m_Label.x + LabelSize.x <= V.x - 4 && Layout.m_Label.y + LabelSize.y <= V.y - 4);
				CHECK(Layout.m_Marker.x >= 48 && Layout.m_Marker.x <= V.x - 48);
				CHECK(Layout.m_Marker.y >= 48 && Layout.m_Marker.y <= V.y - 48);
				Layouts.push_back(Layout);
			}
			CHECK(Layouts[0].m_Marker != Layouts[1].m_Marker);
		}
	}
	std::puts("ping tests passed");
}
