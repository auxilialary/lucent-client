#ifndef GAME_CLIENT_LUCENT_MOTION_CURSOR_MOTION_BLUR_H
#define GAME_CLIENT_LUCENT_MOTION_CURSOR_MOTION_BLUR_H

#include <base/vmath.h>
#include <engine/graphics.h>
#include <cstdint>
#include <vector>

namespace LucentStyle::Motion
{
struct CCursorPixels
{
	int m_Width = 0, m_Height = 0;
	uint64_t m_Revision = 0;
	std::vector<uint8_t> m_vRgba;
};

// Transparent cursor-only frame accumulation, in premultiplied RGBA.
// The fixed local canvas never contains map, character or menu pixels.
class CCursorMotionBlur
{
public:
	static constexpr int Resolution = 192;
private:
	std::vector<vec4> m_vHistory, m_vNext;
	std::vector<uint8_t> m_vUpload;
	IGraphics::CTextureHandle m_Texture;
	vec2 m_Position = vec2(0, 0);
	double m_Time = -1;
	float m_Scale = 1.f;
	int m_Context = 0, m_Strength = 0;
	uint64_t m_Revision = 0;
	vec4 ReadHistory(float X, float Y) const;
public:
	void Reset();
	bool Update(const CCursorPixels &Source, vec2 Position, float Scale, double Now, int Context, int Strength);
	const std::vector<vec4> &Pixels() const { return m_vHistory; }
	const std::vector<uint8_t> &Rgba() const { return m_vUpload; }
	bool Render(IGraphics *pGraphics, const CCursorPixels &Source, vec2 Position, vec2 Origin, float Scale, float Alpha, double Now, int Context, int Strength);
	void Shutdown(IGraphics *pGraphics);
};
}
#endif
