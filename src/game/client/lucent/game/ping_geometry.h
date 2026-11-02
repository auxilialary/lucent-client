#ifndef GAME_CLIENT_LUCENT_GAME_PING_GEOMETRY_H
#define GAME_CLIENT_LUCENT_GAME_PING_GEOMETRY_H
#include <base/vmath.h>

#include <vector>
namespace LucentPing
{
	struct CProjection
	{
		vec2 m_Position;
		vec2 m_Direction;
		bool m_Offscreen;
	};
	CProjection Project(vec2 Point, vec2 WorldMin, vec2 WorldSize, vec2 ViewSize, float Margin);
	enum class EDistanceView
	{
		Gameplay,
		Follow,
		Freeview
	};
	vec2 DistanceOrigin(EDistanceView View, vec2 Camera, const vec2 *pLocal, const vec2 *pFollow);
	// Returns the label background's top-left; Size includes text padding.
	vec2 DistanceLabelPosition(vec2 Marker, vec2 ViewSize, vec2 Size);
	vec2 SenderLabelSize(float NameWidth, float DistanceWidth, vec2 View, bool Identity);
	struct CIndicatorLayout
	{ vec2 m_Marker, m_Label, m_Size; };
	// Stack along the edge only; the caller retains the original world-space direction.
	CIndicatorLayout PlaceIndicator(vec2 Marker, vec2 ViewSize, vec2 LabelSize, const std::vector<CIndicatorLayout> &Placed);
	float DistanceTiles(vec2 Origin, vec2 Point);
}
#endif
