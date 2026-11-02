#include "points.h"

#include <base/str.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/shared/jobs.h>
namespace LucentScoreboard
{
	class CPointsParseJob : public IJob
	{
		std::shared_ptr<IHttpRequest> m_pRequest;
		std::string m_Name;
		void Run() override
		{
			unsigned char *pData;
			size_t Size;
			m_pRequest->Result(&pData, &Size);
			m_Points = ParsePoints(reinterpret_cast<const char *>(pData), Size, m_Name);
		}

	public:
		int m_Points = UnknownPoints;
		CPointsParseJob(std::shared_ptr<IHttpRequest> pRequest, std::string Name) : m_pRequest(std::move(pRequest)), m_Name(std::move(Name)) {}
	};
	struct CPointsService::CImpl
	{
		CPointsCache m_Cache;
		FRequestFactory m_Factory;
		struct CWork
		{
			int m_Slot = -1;
			bool m_Discard = false;
			std::shared_ptr<IHttpRequest> m_pRequest;
			std::shared_ptr<CPointsParseJob> m_pJob;
		};
		std::array<CWork, CPointsCache::Parallel> m_aWork;
		double m_NextPoll = 0, m_NextRoster = 0;
	};
	CPointsService::CPointsService(FRequestFactory Factory) : m_pImpl(std::make_unique<CImpl>())
	{
		m_pImpl->m_Factory = Factory ? std::move(Factory) : FRequestFactory([](const char *pUrl) { return std::shared_ptr<IHttpRequest>(HttpGet(pUrl)); });
	}
	CPointsService::~CPointsService() { Reset(); }

	void CPointsService::Reset()
	{
		for(auto &W : m_pImpl->m_aWork)
		{
			if(W.m_pRequest)
				W.m_pRequest->Abort();
			if(W.m_Slot >= 0)
				W.m_Discard = true;
		}
		m_pImpl->m_Cache.Begin();
		m_pImpl->m_NextPoll = m_pImpl->m_NextRoster = 0;
	}
	int CPointsService::Lookup(const char *pName, double Now) const { return m_pImpl->m_Cache.Lookup(pName, Now); }
	void CPointsService::Update(const std::array<const char *, 128> &Names, double Now, IHttp *pHttp, IEngine *pEngine)
	{
		auto &S = *m_pImpl;
		if(Now < S.m_NextPoll)
			return;
		S.m_NextPoll = Now + .05;
		if(Now >= S.m_NextRoster)
		{
			S.m_NextRoster = Now + .25;
			S.m_Cache.Begin();
			for(auto &E : S.m_Cache.m_aEntries)
				for(const auto *pName : Names)
					if(pName && E.m_Name == pName)
					{
						E.m_Wanted = true;
						E.m_Used = Now;
						break;
					}
			for(const auto *pName : Names)
				if(pName)
					S.m_Cache.Touch(pName, Now);
		}
		for(auto &W : S.m_aWork)
		{
			if(W.m_Slot < 0)
				continue;
			if(W.m_Discard || !S.m_Cache.m_aEntries[W.m_Slot].m_Wanted)
			{
				W.m_Discard = true;
				W.m_pRequest->Abort();
				// Keep cancellation counted until both transport and parser are terminal.
				if(!W.m_pRequest->Done() || (W.m_pJob && W.m_pJob->State() != IJob::STATE_DONE))
					continue;
				S.m_Cache.m_aEntries[W.m_Slot].m_Pending = false;
				W = {};
				continue;
			}
			if(W.m_pJob)
			{
				if(W.m_pJob->State() == IJob::STATE_DONE)
				{
					S.m_Cache.Complete(W.m_Slot, W.m_pJob->m_Points, Now);
					W = {};
				}
			}
			else if(W.m_pRequest->Done())
			{
				const EHttpState State = W.m_pRequest->State();
				// Done() also includes ERROR and ABORTED. Response accessors require DONE.
				const int Status = State == EHttpState::DONE ? W.m_pRequest->StatusCode() : 0;
				if(State == EHttpState::DONE && Status == 200)
				{
					S.m_Cache.m_Budget.Success(Now);
					W.m_pJob = std::make_shared<CPointsParseJob>(W.m_pRequest, S.m_Cache.m_aEntries[W.m_Slot].m_Name);
					pEngine->AddJob(W.m_pJob);
				}
				else
				{
					if(Status == 429 || Status == 503)
						S.m_Cache.m_Budget.Limited(Now);
					else
						S.m_Cache.m_Budget.TransportError(Now);
					S.m_Cache.Complete(W.m_Slot, UnknownPoints, Now);
					W = {};
				}
			}
		}
		for(auto &W : S.m_aWork)
		{
			if(W.m_Slot >= 0)
				continue;
			const int Slot = S.m_Cache.Start(Now);
			if(Slot < 0)
				break;
			const auto &Name = S.m_Cache.m_aEntries[Slot].m_Name;
			char aEscaped[3 * 64 + 1], aUrl[256];
			EscapeUrl(aEscaped, Name.c_str());
			str_format(aUrl, sizeof(aUrl), "https://ddnet.org/players/?json2=%s", aEscaped);
			W.m_Slot = Slot;
			W.m_pRequest = S.m_Factory(aUrl);
			W.m_pRequest->Timeout({4000, 12000, 256, 5});
			// Preserve HTTP 429/503 as DONE + status; transport ERROR/ABORTED has no readable response.
			W.m_pRequest->FailOnErrorStatus(false);
			W.m_pRequest->MaxResponseSize(2 * 1024 * 1024);
			W.m_pRequest->LogProgress(HTTPLOG::NONE);
			pHttp->Run(W.m_pRequest);
		}
	}
}
