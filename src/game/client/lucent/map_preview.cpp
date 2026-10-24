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
		m_Image.m_Width = 320;
		m_Image.m_Height = 180;
		m_Image.m_Format = CImageInfo::FORMAT_RGBA;
		m_Image.Allocate();
		// Box sample in source space: inexpensive low-resolution blur and desaturation,
		// performed once on a worker, never in the frame renderer.
		for(size_t y = 0; y < 180; ++y)
			for(size_t x = 0; x < 320; ++x)
			{
				ColorRGBA Sum(0, 0, 0, 0);
				const float Scale = std::max(320.f / W, 180.f / H);
				const int X = int((x + .5f - 160) / Scale + W * .5f), Y = int((y + .5f - 90) / Scale + H * .5f);
				const int Radius = std::max(1, int(1 / Scale));
				for(int dy = -2; dy <= 2; ++dy)
					for(int dx = -2; dx <= 2; ++dx)
					{
						const auto C = Source.PixelColor(std::clamp(X + dx * Radius, 0, int(W) - 1), std::clamp(Y + dy * Radius, 0, int(H) - 1));
						Sum.r += C.r / 25;
						Sum.g += C.g / 25;
						Sum.b += C.b / 25;
					}
				const float Grey = Sum.r * .2126f + Sum.g * .7152f + Sum.b * .0722f;
				const float nx = (x + .5f) / 160 - 1, ny = (y + .5f) / 90 - 1;
				const float Vignette = std::max(.35f, 1.f - .35f * (nx * nx + ny * ny));
				m_Image.SetPixelColor(x, y, ColorRGBA((Grey * .7f + Sum.r * .3f) * Vignette, (Grey * .7f + Sum.g * .3f) * Vignette, (Grey * .7f + Sum.b * .3f) * Vignette, 1));
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
	if(!Animate || Now - m_FadeStart >= .38)
		m_Previous = -1;
	if(m_pDecode && m_pDecode->Done() && (!Animate || Now - m_FadeStart >= .38))
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
		if(!Animate || Now - m_FadeStart >= .38)
			Change(-1, Now, Animate);
		return;
	}
	if(!m_Selection.Ready(Now) || m_pDecode || m_pRequest || (Animate && Now - m_FadeStart < .38))
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

void CLucentMapPreview::Render(IGraphics *pGraphics, const CUIRect &Screen, double Now, bool Animate)
{
	Screen.Draw(ColorRGBA(.018f, .015f, .027f, 1), 0, 0);
	const float T = Animate ? std::clamp(float((Now - m_FadeStart) / .38), 0.f, 1.f) : 1;
	const float Fade = T * T * (3 - 2 * T);
	const auto Draw = [&](int Index, float Alpha) {
		if(Index < 0 || Alpha <= 0)
			return;
		pGraphics->TextureSet(m_aCache[Index].m_Texture);
		pGraphics->WrapClamp();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(1, 1, 1, Alpha * .20f);
		// Cover with aspect-correct center crop.
		const float Scale = std::max(Screen.w / 320, Screen.h / 180);
		IGraphics::CQuadItem Quad(Screen.x + (Screen.w - 320 * Scale) / 2, Screen.y + (Screen.h - 180 * Scale) / 2, 320 * Scale, 180 * Scale);
		pGraphics->QuadsDrawTL(&Quad, 1);
		pGraphics->QuadsEnd();
		pGraphics->WrapNormal();
	};
	Draw(m_Previous, 1 - Fade);
	Draw(m_Current, Fade);
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
