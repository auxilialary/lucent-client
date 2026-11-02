#ifndef GAME_CLIENT_LUCENT_SKINS_TEXTURE_QUALITY_H
#define GAME_CLIENT_LUCENT_SKINS_TEXTURE_QUALITY_H
#include <algorithm>
#include <array>
#include <cstddef>
class CImageInfo;
namespace LucentSkins
{
	inline constexpr std::array TextureLimits = {128, 256, 512, 1024, 2048, 4096, 8192};
	struct CTextureSize
	{
		size_t m_Width, m_Height;
	};
	inline CTextureSize LimitedSize(size_t W, size_t H, int Limit, size_t GridX, size_t GridY)
	{
		if(!W || !H || !GridX || !GridY)
			return {};
		const size_t Max = std::clamp(Limit, 128, 8192);
		if(W <= Max && H <= Max)
			return {W, H};
		const long double Scale = (long double)Max / std::max(W, H);
		const size_t NewW = size_t(W * Scale) / GridX * GridX;
		const size_t NewH = size_t(H * Scale) / GridY * GridY;
		return NewW && NewH ? CTextureSize{NewW, NewH} : CTextureSize{};
	}
	// Reduces owned RGBA images before grayscale copies and GPU upload.
	// Allocation failure leaves the original image intact; transparent RGB is alpha-weighted.
	bool LimitTexture(CImageInfo &Image, int Limit, size_t GridX, size_t GridY);
}
#endif
