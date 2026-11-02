#include "texture_quality.h"

#include <engine/image.h>

#include <cstdint>
#include <cstdlib>
namespace LucentSkins
{
	bool LimitTexture(CImageInfo &Image, int Limit, size_t GridX, size_t GridY)
	{
		if(!Image.m_pData || Image.m_Format != CImageInfo::FORMAT_RGBA)
			return false;
		const auto Size = LimitedSize(Image.m_Width, Image.m_Height, Limit, GridX, GridY);
		if(!Size.m_Width || !Size.m_Height)
			return false;
		if(Size.m_Width == Image.m_Width && Size.m_Height == Image.m_Height)
			return true;
		auto *pPixels = static_cast<uint8_t *>(malloc(Size.m_Width * Size.m_Height * 4));
		if(!pPixels)
			return false;
		for(size_t Y = 0; Y < Size.m_Height; ++Y)
			for(size_t X = 0; X < Size.m_Width; ++X)
			{
				const size_t X0 = X * Image.m_Width / Size.m_Width, X1 = (X + 1) * Image.m_Width / Size.m_Width;
				const size_t Y0 = Y * Image.m_Height / Size.m_Height, Y1 = (Y + 1) * Image.m_Height / Size.m_Height;
				uint64_t Color[3] = {}, Alpha = 0;
				for(size_t Sy = Y0; Sy < Y1; ++Sy)
					for(size_t Sx = X0; Sx < X1; ++Sx)
					{
						const auto *pSource = Image.m_pData + (Sy * Image.m_Width + Sx) * 4;
						Alpha += pSource[3];
						for(int C = 0; C < 3; ++C)
							Color[C] += uint64_t(pSource[C]) * pSource[3];
					}
				auto *pDest = pPixels + (Y * Size.m_Width + X) * 4;
				for(int C = 0; C < 3; ++C)
					pDest[C] = Alpha ? uint8_t(Color[C] / Alpha) : 0;
				pDest[3] = uint8_t(Alpha / ((X1 - X0) * (Y1 - Y0)));
			}
		free(Image.m_pData);
		Image.m_pData = pPixels;
		Image.m_Width = Size.m_Width;
		Image.m_Height = Size.m_Height;
		return true;
	}
}
