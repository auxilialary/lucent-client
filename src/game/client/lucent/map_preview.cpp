#include "map_preview.h"

#include <base/str.h>

#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/gfx/image_loader.h>
#include <engine/http.h>
#include <engine/serverbrowser.h>
#include <engine/shared/jobs.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstring>

class CLucentMapPreview::CDecodeJob : public IJob
{
	std::shared_ptr<IHttpRequest> m_pRequest;
	void Run() override
	{
		unsigned char *pData;
		size_t Size;
		m_pRequest->Result(&pData, &Size);
		// Reject bombs before the PNG decoder allocates memory. Require PNG + IHDR.
		uint32_t W, H;
		if(!LucentPreview::PngDimensions(pData, Size, W, H))
			return;
		CImageInfo Source;
		int Incompatible;
		CByteBufferReader Reader(pData, Size);
		if(!CImageLoader::LoadPng(Reader, "Lucent map preview", Source, Incompatible) || Source.m_Width != W || Source.m_Height != H)
			return;
		// Preserve the complete artwork, including portrait thumbnails. Decode once
		// on the worker into a bounded texture; never crop text into a giant wallpaper.
		const float Scale = LucentPreview::DecodeScale(W, H);
		m_Image.m_Width = std::max(1, int(W * Scale));
		m_Image.m_Height = std::max(1, int(H * Scale));
		m_Image.m_Format = CImageInfo::FORMAT_RGBA;
		m_Image.Allocate();
		for(size_t y = 0; y < m_Image.m_Height; ++y)
			for(size_t x = 0; x < m_Image.m_Width; ++x)
			{
				const auto C = Source.PixelColor(std::min(size_t(W - 1), x * W / m_Image.m_Width), std::min(size_t(H - 1), y * H / m_Image.m_Height));
				const float Grey = C.r * .2126f + C.g * .7152f + C.b * .0722f;
				m_Image.SetPixelColor(x, y, ColorRGBA(Grey * .18f + C.r * .82f, Grey * .18f + C.g * .82f, Grey * .18f + C.b * .82f, C.a));
			}
	}

public:
	CImageInfo m_Image;
	uint64_t m_Generation;
	CDecodeJob(std::shared_ptr<IHttpRequest> pRequest, uint64_t Generation) : m_pRequest(std::move(pRequest)), m_Generation(Generation) {}
};

void CLucentMapPreview::Change(int Index, double Now, bool Animate)
{
	if(Index == m_Current)
		return;
	m_Previous = Animate ? m_Current : -1;
	m_Current = Index;
	if(Index >= 0)
		m_aCache[Index].m_RevealStart = Now;
	m_FadeStart = Now;
}

void CLucentMapPreview::Fail(double Now, bool Animate)
{
	m_Failures.Add(m_Selection.Key(), Now);
	Change(-1, Now, Animate);
}

void CLucentMapPreview::Update(const CServerInfo *pServer, double Now, bool Animate, IHttp *pHttp, IEngine *pEngine, IGraphics *pGraphics)
{
	char aKey[256] = "";
	if(pServer && pServer->m_aMap[0])
		str_format(aKey, sizeof(aKey), "%s|%08x|%d", pServer->m_aMap, unsigned(pServer->m_MapCrc), pServer->m_MapSize);
	if(m_Selection.Select(aKey, Now))
	{
		if(m_pRequest)
			m_pRequest->Abort();
		m_pRequest.reset();
		m_Map = pServer ? pServer->m_aMap : "";
	}
	if(!Animate || Now - m_FadeStart >= .42)
		m_Previous = -1;
	if(m_pDecode && m_pDecode->Done() && (!Animate || Now - m_FadeStart >= .42))
	{
		if(m_Selection.Accept(m_pDecode->m_Generation) && m_pDecode->m_Image.m_pData)
		{
			const int Slot = LucentPreview::EvictionSlot(m_aCache, m_Current, m_Previous);
			if(Slot >= 0)
			{
				auto &Entry = m_aCache[Slot];
				pGraphics->UnloadTexture(&Entry.m_Texture);
				Entry.m_Key = m_Selection.Key();
				Entry.m_Used = Now;
				Entry.m_Width = m_pDecode->m_Image.m_Width;
				Entry.m_Height = m_pDecode->m_Image.m_Height;
				Entry.m_Texture = pGraphics->LoadTextureRawMove(m_pDecode->m_Image, 0, "Lucent map atmosphere");
				if(Entry.m_Texture.IsValid())
					Change(Slot, Now, Animate);
				else
					Fail(Now, Animate);
			}
		}
		else if(m_Selection.Accept(m_pDecode->m_Generation))
			Fail(Now, Animate);
		m_pDecode.reset();
	}
	if(m_pRequest && m_pRequest->Done())
	{
		if(m_pRequest->State() == EHttpState::DONE && m_Selection.Accept(m_RequestGeneration))
		{
			if(m_Metadata)
			{
				json_value *pJson = m_pRequest->ResultJson();
				std::string Url;
				if(pJson)
				{
					const auto &Name = (*pJson)["name"], &Thumbnail = (*pJson)["thumbnail"];
					if(Name.type == json_string && Thumbnail.type == json_string && m_Map == Name.u.string.ptr)
						Url = Thumbnail.u.string.ptr;
					json_value_free(pJson);
				}
				// Only fetch canonical first-party thumbnails; never arbitrary server URLs.
				if(LucentPreview::ThumbnailUrl(Url))
				{
					m_pRequest = HttpGet(Url.c_str());
					m_Metadata = false;
					m_pRequest->Timeout({3000, 10000, 256, 5});
					m_pRequest->MaxResponseSize(4 * 1024 * 1024);
					m_pRequest->LogProgress(HTTPLOG::NONE);
					pHttp->Run(m_pRequest);
				}
				else
				{
					m_pRequest.reset();
					Fail(Now, Animate);
				}
			}
			else
			{
				m_pDecode = std::make_shared<CDecodeJob>(m_pRequest, m_RequestGeneration);
				pEngine->AddJob(m_pDecode);
				m_pRequest.reset();
			}
		}
		else
		{
			m_pRequest.reset();
			Fail(Now, Animate);
		}
	}
	if(m_Selection.Key().empty())
	{
		Change(-1, Now, Animate);
		return;
	}
	if(!m_Selection.Ready(Now) || m_pDecode || m_pRequest || (Animate && Now - m_FadeStart < .42))
		return;
	m_Selection.Start();
	if(m_Failures.Contains(m_Selection.Key(), Now))
	{
		Change(-1, Now, Animate);
		return;
	}
	const int Cached = LucentPreview::Find(m_aCache, m_Selection.Key());
	if(Cached >= 0)
	{
		m_aCache[Cached].m_Used = Now;
		Change(Cached, Now, Animate);
		return;
	}
	// The map API supplies the canonical thumbnail, including punctuation and
	// Unicode names. It exposes current artwork, not revision-specific hashes.
	// https://ddnet.org/maps/?json=Kobra%204
	char aEscaped[768], aUrl[1024];
	EscapeUrl(aEscaped, m_Map.c_str());
	str_format(aUrl, sizeof(aUrl), "https://ddnet.org/maps/?json=%s", aEscaped);
	// HTTP has a fixed 256-byte URL buffer. Never silently truncate UTF-8 names.
	if(str_length(aUrl) >= 256)
	{
		Change(-1, Now, Animate);
		return;
	}
	m_pRequest = HttpGet(aUrl);
	m_Metadata = true;
	m_RequestGeneration = m_Selection.Generation();
	m_pRequest->Timeout({3000, 10000, 256, 5});
	m_pRequest->MaxResponseSize(512 * 1024);
	m_pRequest->LogProgress(HTTPLOG::NONE);
	pHttp->Run(m_pRequest);
}

void CLucentMapPreview::Render(IGraphics *pGraphics, const CUIRect &Screen, double Now, bool Animate, float SelectionPulse)
{
	Screen.Draw(ColorRGBA(.018f, .015f, .027f, 1), 0, 0);
	const float T = Animate ? std::clamp(float((Now - m_FadeStart) / .42), 0.f, 1.f) : 1;
	const float Fade = T * T * (3 - 2 * T);
	const auto Draw = [&](int Index, float Alpha, bool Previous) {
		if(Index < 0 || Alpha <= 0)
			return;
		pGraphics->TextureSet(m_aCache[Index].m_Texture);
		pGraphics->WrapClamp();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(1, 1, 1, Alpha * .85f);
		const auto &Entry = m_aCache[Index];
		const float Settle = Animate ? std::pow(1.f - std::clamp(float((Now - Entry.m_RevealStart) / 3.5), 0.f, 1.f), 3.f) : 0.f;
		const float Scale = LucentPreview::FrameScale(Entry.m_Width, Entry.m_Height, Screen.w, Screen.h) * (1.f - .015f * Settle);
		const float Width = Entry.m_Width * Scale, Height = Entry.m_Height * Scale;
		const float X = Screen.x + Screen.w * .76f - Width * .5f;
		const float Y = Screen.y + Screen.h * .47f - Height * .5f;
		// Feather all four edges in texture space. Geometry stays identical through fades.
		constexpr int Steps = 24;
		for(int y = 0; y < Steps; ++y)
			for(int x = 0; x < Steps; ++x)
			{
				const auto Mask = [&](float u, float v) {
					const float Edge = std::clamp(std::min({u, v, 1 - u, 1 - v}) * 6.f, 0.f, 1.f);
					const float Feather = Edge * Edge * (3 - 2 * Edge);
					const float A = .9f * Feather;
					// Compensate alpha-over so equally framed images keep constant
					// brightness and the outgoing image vanishes without a final jump.
					return LucentPreview::FadeAlpha(A, Fade, Previous, m_Current >= 0);
				};
				const float U = float(x) / Steps, V = float(y) / Steps;
				const float R = float(x + 1) / Steps, B = float(y + 1) / Steps;
				pGraphics->QuadsSetSubset(U, V, R, B);
				pGraphics->SetColor4(ColorRGBA(1, 1, 1, Mask(U, V)), ColorRGBA(1, 1, 1, Mask(R, V)), ColorRGBA(1, 1, 1, Mask(U, B)), ColorRGBA(1, 1, 1, Mask(R, B)));
				IGraphics::CQuadItem Tile(X + Width * U, Y + Height * V, Width / Steps, Height / Steps);
				pGraphics->QuadsDrawTL(&Tile, 1);
			}
		pGraphics->QuadsSetSubset(0, 0, 1, 1);
		pGraphics->QuadsEnd();
		pGraphics->WrapNormal();
	};
	Draw(m_Previous, 1 - Fade, true);
	Draw(m_Current, Fade, false);
	// Preserve the artwork in open space, with a stronger scrim behind map names.
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	// Keep the table dark, release the scrim across its boundary, and let
	// recognizable artwork breathe in the right-hand scene.
	const auto ScrimBand = [&](float Left, float Right, float A, float B) {
		pGraphics->SetColor4(ColorRGBA(.018f, .015f, .027f, A), ColorRGBA(.018f, .015f, .027f, B), ColorRGBA(.018f, .015f, .027f, A), ColorRGBA(.018f, .015f, .027f, B));
		IGraphics::CQuadItem Band(Screen.x + Screen.w * Left, Screen.y, Screen.w * (Right - Left), Screen.h);
		pGraphics->QuadsDrawTL(&Band, 1);
	};
	ScrimBand(0, .52f, .86f, .86f);
	ScrimBand(.52f, .72f, .86f, .10f);
	const float Response = 1 - .12f * std::sin(std::clamp(SelectionPulse, 0.f, 1.f) * 3.14159265f);
	ScrimBand(.72f, 1, .10f * Response, .06f * Response);
	pGraphics->QuadsEnd();
}
void CLucentMapPreview::Shutdown(IGraphics *pGraphics)
{
	if(m_pRequest)
		m_pRequest->Abort();
	m_pRequest.reset();
	m_pDecode.reset();
	for(auto &Entry : m_aCache)
	{
		pGraphics->UnloadTexture(&Entry.m_Texture);
		Entry = CEntry{};
	}
	m_Current = m_Previous = -1;
	m_Selection = CLucentPreviewSelection{};
	m_Failures = LucentPreview::CFailures{};
}
