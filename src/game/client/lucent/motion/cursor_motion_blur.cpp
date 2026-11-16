#include "cursor_motion_blur.h"

#include <cmath>

namespace LucentStyle::Motion
{
void CCursorMotionBlur::Reset()
{
	m_Time = -1;
}

vec4 CCursorMotionBlur::ReadHistory(float X, float Y) const
{
	const int Left = (int)std::floor(X), Top = (int)std::floor(Y);
	const float Fx = X - Left, Fy = Y - Top;
	const auto Read = [&](int Px, int Py) {
		return Px >= 0 && Py >= 0 && Px < Resolution && Py < Resolution ? m_vHistory[Py * Resolution + Px] : vec4(0, 0, 0, 0);
	};
	return mix(mix(Read(Left, Top), Read(Left + 1, Top), Fx), mix(Read(Left, Top + 1), Read(Left + 1, Top + 1), Fx), Fy);
}

bool CCursorMotionBlur::Update(const CCursorPixels &Source, vec2 Position, float Scale, double Now, int Context, int Strength)
{
	if(Strength <= 0 || Source.m_Width <= 0 || Source.m_Height <= 0 ||
		Source.m_vRgba.size() != (size_t)Source.m_Width * Source.m_Height * 4 ||
		!std::isfinite(Now) || !std::isfinite(Position.x) || !std::isfinite(Position.y) || !std::isfinite(Scale) || Scale <= 0)
	{
		Reset();
		return false;
	}
	if(m_vHistory.empty())
	{
		m_vHistory.resize(Resolution * Resolution, vec4(0, 0, 0, 0));
		m_vNext.resize(Resolution * Resolution);
		m_vUpload.resize(Resolution * Resolution * 4);
	}
	const double Dt = Now - m_Time;
	const bool Valid = m_Time >= 0 && Dt > 0 && Dt <= 0.25 && m_Context == Context &&
		m_Revision == Source.m_Revision && m_Scale == Scale && m_Strength == Strength && distance(Position, m_Position) < 200.f * Scale;
	const float Amount = std::clamp(Strength, 0, 100) / 100.f;
	// Exponential frame persistence: the same decay per second at any refresh rate.
	const float Persistence = Valid ? std::pow(0.58f * Amount * Amount, (float)Dt * 60.f) : 0.f;
	const vec2 Shift = (Position - m_Position) / Scale;
	const float Diagonal = std::sqrt((float)Source.m_Width * Source.m_Width + (float)Source.m_Height * Source.m_Height);
	const float Width = 64.f * Source.m_Width / Diagonal;
	const float Height = 64.f * Source.m_Height / Diagonal;
	const auto ReadSource = [&](int X, int Y) {
		if(X < 0 || Y < 0 || X >= Source.m_Width || Y >= Source.m_Height)
			return vec4(0, 0, 0, 0);
		const auto *p = &Source.m_vRgba[(Y * Source.m_Width + X) * 4];
		const float A = p[3] / 255.f;
		return vec4(p[0] / 255.f * A, p[1] / 255.f * A, p[2] / 255.f * A, A);
	};
	for(int Y = 0; Y < Resolution; ++Y)
		for(int X = 0; X < Resolution; ++X)
		{
			const float U = ((X + 0.5f - Resolution / 2.f) / Width + 0.5f) * Source.m_Width - 0.5f;
			const float V = ((Y + 0.5f - Resolution / 2.f) / Height + 0.5f) * Source.m_Height - 0.5f;
			const int Sx = (int)std::floor(U), Sy = (int)std::floor(V);
			vec4 Current(0, 0, 0, 0);
			if(U >= -0.5f && V >= -0.5f && U < Source.m_Width - 0.5f && V < Source.m_Height - 0.5f)
				Current = mix(mix(ReadSource(Sx, Sy), ReadSource(Sx + 1, Sy), U - Sx), mix(ReadSource(Sx, Sy + 1), ReadSource(Sx + 1, Sy + 1), U - Sx), V - Sy);
			const vec4 Previous = Persistence > 0.f ? ReadHistory(X + Shift.x, Y + Shift.y) : vec4(0, 0, 0, 0);
			const vec4 Pixel = mix(Current, Previous, Persistence);
			const int Index = Y * Resolution + X;
			m_vNext[Index] = Pixel;
			// Convert to straight alpha only for upload; history stays premultiplied.
			const float aChannels[] = {Pixel.r, Pixel.g, Pixel.b};
			for(int C = 0; C < 3; ++C)
				m_vUpload[Index * 4 + C] = Pixel.a > 0.0001f ? (uint8_t)std::round(std::clamp(aChannels[C] / Pixel.a, 0.f, 1.f) * 255.f) : 0;
			m_vUpload[Index * 4 + 3] = (uint8_t)std::round(std::clamp(Pixel.a, 0.f, 1.f) * 255.f);
		}
	m_vHistory.swap(m_vNext);
	m_Position = Position;
	m_Time = Now;
	m_Context = Context;
	m_Scale = Scale;
	m_Strength = Strength;
	m_Revision = Source.m_Revision;
	return true;
}
}
