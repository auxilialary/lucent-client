#include "ping_geometry.h"

#include <algorithm>
#include <cmath>
namespace LucentPing
{
	CProjection Project(vec2 Point, vec2 WorldMin, vec2 WorldSize, vec2 ViewSize, float Margin)
	{
		const vec2 Screen((Point.x - WorldMin.x) / WorldSize.x * ViewSize.x, (Point.y - WorldMin.y) / WorldSize.y * ViewSize.y);
		const vec2 Center = ViewSize / 2.f;
		const vec2 Delta = Screen - Center;
		const float Length = std::hypot(Delta.x, Delta.y);
		const vec2 Direction = Length > 0 ? Delta / Length : vec2(0, 0);
		const bool Outside = Screen.x < 0 || Screen.y < 0 || Screen.x > ViewSize.x || Screen.y > ViewSize.y;
		if(!Outside)
			return {Screen, Direction, false};
		const vec2 Half(std::max(1.f, Center.x - Margin), std::max(1.f, Center.y - Margin));
		const float Scale = 1.f / std::max(std::abs(Delta.x) / Half.x, std::abs(Delta.y) / Half.y);
		return {Center + Delta * Scale, Direction, true};
	}
	vec2 DistanceOrigin(EDistanceView View, vec2 Camera, const vec2 *pLocal, const vec2 *pFollow)
	{
		if(View == EDistanceView::Gameplay && pLocal)
			return *pLocal;
		if(View == EDistanceView::Follow && pFollow)
			return *pFollow;
		return Camera;
	}
	vec2 DistanceLabelPosition(vec2 Marker, vec2 ViewSize, vec2 Size)
	{
		const float HorizontalEdge = std::min(Marker.x, ViewSize.x - Marker.x);
		const float VerticalEdge = std::min(Marker.y, ViewSize.y - Marker.y);
		vec2 Position;
		if(HorizontalEdge <= VerticalEdge)
			Position = vec2(Marker.x < ViewSize.x / 2 ? Marker.x + 12 : Marker.x - 12 - Size.x, Marker.y - Size.y / 2);
		else
			Position = vec2(Marker.x - Size.x / 2, Marker.y < ViewSize.y / 2 ? Marker.y + 12 : Marker.y - 12 - Size.y);
		return vec2(std::clamp(Position.x, 4.f, std::max(4.f, ViewSize.x - Size.x - 4)), std::clamp(Position.y, 4.f, std::max(4.f, ViewSize.y - Size.y - 4)));
	}
	vec2 SenderLabelSize(float NameWidth, float DistanceWidth, vec2 View, bool Identity)
	{
		const float TextWidth = std::max(0.f, std::min(NameWidth, 96.f));
		const float Width = Identity ? 36 + std::max(TextWidth, DistanceWidth) : DistanceWidth + 8;
		return vec2(std::min(Width, std::max(1.f, View.x - 8)), Identity ? 34.f : 15.f);
	}
	CIndicatorLayout PlaceIndicator(vec2 Marker, vec2 View, vec2 Size, const std::vector<CIndicatorLayout> &Placed)
	{
		const bool Vertical = std::min(Marker.x, View.x - Marker.x) <= std::min(Marker.y, View.y - Marker.y);
		const float Step = Vertical ? std::max(32.f, Size.y + 8) : Size.x + 24;
		CIndicatorLayout Best{Marker, DistanceLabelPosition(Marker, View, Size), Size};
		int BestHits = 1000;
		for(int Candidate = 0; Candidate < 17; ++Candidate)
		{
			const float Offset = Candidate == 0 ? 0 : float((Candidate + 1) / 2) * (Candidate % 2 ? 1.f : -1.f) * Step;
			vec2 Pos = Marker;
			if(Vertical)
				Pos.y = std::clamp(Pos.y + Offset, 48.f, std::max(48.f, View.y - 48));
			else
				Pos.x = std::clamp(Pos.x + Offset, 48.f, std::max(48.f, View.x - 48));
			CIndicatorLayout Layout{Pos, DistanceLabelPosition(Pos, View, Size), Size};
			int Hits = 0;
			for(const auto &Other : Placed)
			{
				const bool Labels = Layout.m_Label.x < Other.m_Label.x + Other.m_Size.x + 4 && Layout.m_Label.x + Size.x + 4 > Other.m_Label.x && Layout.m_Label.y < Other.m_Label.y + Other.m_Size.y + 4 && Layout.m_Label.y + Size.y + 4 > Other.m_Label.y;
				if(Labels)
					++Hits;
				if(distance(Pos, Other.m_Marker) < 24)
					Hits += 8;
			}
			if(Hits < BestHits)
			{
				Best = Layout;
				BestHits = Hits;
			}
			if(!Hits)
				break;
		}
		return Best;
	}
	float DistanceTiles(vec2 Tee, vec2 Point) { return std::hypot(Tee.x - Point.x, Tee.y - Point.y) / 32.f; }
}
