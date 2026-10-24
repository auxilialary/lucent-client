#ifndef GAME_CLIENT_LUCENT_MAP_PREVIEW_H
#define GAME_CLIENT_LUCENT_MAP_PREVIEW_H
#include "browser_state.h"
#include "preview_policy.h"

#include <engine/graphics.h>

#include <array>
#include <memory>
#include <string>
class IHttp;
class IHttpRequest;
class IEngine;
class CUIRect;
class CServerInfo;
class CLucentMapPreview
{
	class CDecodeJob;
	struct CEntry
	{
		std::string m_Key;
		IGraphics::CTextureHandle m_Texture;
		double m_Used = 0;
		int m_Width = 1, m_Height = 1;
	};
	// Eight aspect-preserving textures, <= 57,600 pixels each: <= 1.76 MiB.
	std::array<CEntry, 8> m_aCache;
	LucentPreview::CFailures m_Failures;
	void Fail(double Now, bool Animate);
	CLucentPreviewSelection m_Selection;
	std::shared_ptr<IHttpRequest> m_pRequest;
	std::shared_ptr<CDecodeJob> m_pDecode;
	uint64_t m_RequestGeneration = 0;
	bool m_Metadata = false;
	std::string m_Map;
	int m_Current = -1, m_Previous = -1;
	double m_FadeStart = 0;
	void Change(int Index, double Now, bool Animate);

public:
	void Update(const CServerInfo *pServer, double Now, bool Animate, IHttp *pHttp, IEngine *pEngine, IGraphics *pGraphics);
	void Render(IGraphics *pGraphics, const CUIRect &Screen, double Now, bool Animate);
	void Shutdown(IGraphics *pGraphics);
};
#endif
