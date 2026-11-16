#include "cursor_motion_blur.h"

#include <engine/image.h>
#include <engine/client/graphics_threaded.h>

namespace LucentStyle::Motion
{
void CCursorMotionBlur::Shutdown(IGraphics *pGraphics)
{
	if(m_Texture.IsValid())
		pGraphics->UnloadTexture(&m_Texture);
	Reset();
}

bool CCursorMotionBlur::Render(IGraphics *pGraphics, const CCursorPixels &Source, vec2 Position, vec2 Origin, float Scale, float Alpha, double Now, int Context, int Strength)
{
	if(!Update(Source, Position, Scale, Now, Context, Strength))
		return false;
	CImageInfo Image;
	Image.m_Width = Image.m_Height = Resolution;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = m_vUpload.data();
	// LoadTextureRaw copies the pixels into the render command; no readback or wait.
	if(m_Texture.IsValid())
		pGraphics->UnloadTexture(&m_Texture);
	m_Texture = pGraphics->LoadTextureRaw(Image, TextureFlag::NO_MIPMAPS, "lucent-cursor-history");
	Image.m_pData = nullptr;
	if(!m_Texture.IsValid() || m_Texture.IsNullTexture())
	{
		Reset();
		return false;
	}
	pGraphics->TextureSet(m_Texture);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.f, 1.f, 1.f, Alpha);
	pGraphics->QuadsSetSubset(0, 0, 1, 1);
	IGraphics::CQuadItem Quad(Position.x + Origin.x, Position.y + Origin.y, Resolution * Scale, Resolution * Scale);
	pGraphics->QuadsDraw(&Quad, 1);
	pGraphics->QuadsEnd();
	return true;
}
}
