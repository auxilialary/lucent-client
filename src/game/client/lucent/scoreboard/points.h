#ifndef GAME_CLIENT_LUCENT_SCOREBOARD_POINTS_H
#define GAME_CLIENT_LUCENT_SCOREBOARD_POINTS_H
#include <engine/external/json-parser/json.h>

#include <algorithm>
#include <array>
#include <climits>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
class IHttp;
class IHttpRequest;
class IEngine;
namespace LucentScoreboard
{
	inline constexpr int UnknownPoints = -1;
	inline int ParsePoints(const char *pData, size_t Size, std::string_view Name)
	{
		if(!pData || Size == 0 || Size > 2 * 1024 * 1024)
			return UnknownPoints;
		json_settings Settings{};
		Settings.max_memory = 8 * 1024 * 1024;
		json_value *pRoot = json_parse_ex(&Settings, pData, Size, nullptr);
		int Result = UnknownPoints;
		if(pRoot && pRoot->type == json_object)
		{
			const auto &Player = (*pRoot)["player"];
			const auto &Group = (*pRoot)["points"];
			if(Player.type == json_string && std::string_view(Player.u.string.ptr, Player.u.string.length) == Name && Group.type == json_object)
			{
				const auto &Points = Group["points"];
				if(Points.type == json_integer && Points.u.integer >= 0 && Points.u.integer <= INT_MAX)
					Result = int(Points.u.integer);
			}
		}
		if(pRoot)
			json_value_free(pRoot);
		return Result;
	}

	// Application budget, not a claimed upstream quota. No catch-up burst after a stall/TAB.
	struct CPointsBudget
	{
		static constexpr int MaxParallel = 4, MaxStarts = 3;
		static constexpr double MaxRate = 3;
		double m_Rate = 2, m_NextStart = 0, m_CooldownUntil = 0, m_LimitedUntil = 0;
		int m_Parallel = MaxParallel, m_Strikes = 0, m_Healthy = 0;
		double m_HealthySince = -1;
		std::array<double, MaxStarts> m_aStarts{-1e9, -1e9, -1e9};
		int m_StartCursor = 0;
		bool CanStart(int Active, double Now) const
		{
			return Active < m_Parallel && Now + 1e-9 >= m_NextStart && Now >= m_CooldownUntil &&
			       Now - m_aStarts[m_StartCursor] + 1e-9 >= 1;
		}
		void Started(double Now)
		{
			m_aStarts[m_StartCursor] = Now;
			m_StartCursor = (m_StartCursor + 1) % MaxStarts;
			m_NextStart = Now + 1 / m_Rate;
		}
		void Limited(double Now)
		{
			if(Now >= m_LimitedUntil)
			{
				m_Strikes = std::min(4, m_Strikes + 1);
				m_Rate = std::max(.25, m_Rate / 2);
				m_Parallel = std::max(1, m_Parallel / 2);
				m_LimitedUntil = Now + std::min(900., 120. * (1 << (m_Strikes - 1)));
			}
			else
				m_LimitedUntil = std::max(m_LimitedUntil, Now + 120);
			m_CooldownUntil = std::max(m_CooldownUntil, m_LimitedUntil);
			m_Healthy = 0;
			m_HealthySince = -1;
		}
		void TransportError(double Now)
		{
			m_CooldownUntil = std::max(m_CooldownUntil, Now + 10);
			m_Healthy = 0;
			m_HealthySince = -1;
		}
		void Success(double Now)
		{
			if(Now < m_CooldownUntil)
				return;
			if(m_HealthySince < 0)
				m_HealthySince = Now;
			if(++m_Healthy >= 8 && Now - m_HealthySince >= 5)
			{
				m_Rate = std::min(MaxRate, m_Rate + (m_Strikes ? .25 : .5));
				m_Parallel = std::min(MaxParallel, m_Parallel + 1);
				if(m_Rate == MaxRate && m_Parallel == MaxParallel)
					m_Strikes = 0;
				m_Healthy = 0;
				m_HealthySince = Now;
			}
		}
	};
	// Public profile keys are exact UTF-8 nicknames, never slot IDs, clans or IPs.
	struct CPointsCache
	{
		static constexpr int Capacity = 256, Parallel = CPointsBudget::MaxParallel;
		static constexpr double PositiveTtl = 1800, NegativeTtl = 120;
		struct CEntry
		{
			std::string m_Name;
			int m_Points = UnknownPoints;
			double m_Expires = 0, m_Used = 0;
			bool m_Wanted = false, m_Pending = false;
			double m_LastAttempt = -1;
		};
		std::array<CEntry, Capacity> m_aEntries{};
		CPointsBudget m_Budget;
		void Begin()
		{
			for(auto &E : m_aEntries)
				E.m_Wanted = false;
		}
		int Touch(std::string_view Name, double Now)
		{
			if(Name.empty() || Name.size() > 64)
				return -1;
			int Slot = -1;
			for(int I = 0; I < Capacity; ++I)
			{
				auto &E = m_aEntries[I];
				if(E.m_Name == Name)
				{
					E.m_Wanted = true;
					E.m_Used = Now;
					return I;
				}
				if(!E.m_Pending && !E.m_Wanted && (Slot < 0 || E.m_Used < m_aEntries[Slot].m_Used))
					Slot = I;
			}
			if(Slot >= 0)
				m_aEntries[Slot] = {std::string(Name), UnknownPoints, 0, Now, true, false};
			return Slot;
		}
		int Start(double Now)
		{
			int Active = 0;
			for(const auto &E : m_aEntries)
				Active += E.m_Pending;
			if(!m_Budget.CanStart(Active, Now))
				return -1;
			int Best = -1;
			for(int I = 0; I < Capacity; ++I)
			{
				const auto &E = m_aEntries[I];
				if(E.m_Wanted && !E.m_Pending && E.m_Expires <= Now &&
					(Best < 0 || E.m_LastAttempt < m_aEntries[Best].m_LastAttempt))
					Best = I;
			}
			if(Best >= 0)
			{
				m_aEntries[Best].m_Pending = true;
				m_aEntries[Best].m_LastAttempt = Now;
				m_Budget.Started(Now);
			}
			return Best;
		}

		void Complete(int Slot, int Points, double Now)
		{
			auto &E = m_aEntries[Slot];
			E.m_Pending = false;
			E.m_Points = Points;
			E.m_Expires = Now + (Points >= 0 ? PositiveTtl : NegativeTtl);
		}
		int Lookup(std::string_view Name, double Now) const
		{
			for(const auto &E : m_aEntries)
				if(E.m_Name == Name && Now < E.m_Expires)
					return E.m_Points;
			return UnknownPoints;
		}
	};
	class CPointsService
	{
		struct CImpl;
		std::unique_ptr<CImpl> m_pImpl;

	public:
		using FRequestFactory = std::function<std::shared_ptr<IHttpRequest>(const char *)>;
		explicit CPointsService(FRequestFactory Factory = {});
		~CPointsService();
		void Reset();
		void Update(const std::array<const char *, 128> &Names, double Now, IHttp *pHttp, IEngine *pEngine);
		int Lookup(const char *pName, double Now) const;
	};
}
#endif
