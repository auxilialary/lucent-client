#include "test.h"

#include <base/str.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/shared/jobs.h>

#include <game/client/lucent/scoreboard/layout.h>
#include <game/client/lucent/scoreboard/points.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <vector>
using namespace LucentScoreboard;

TEST(LucentScoreboardPoints, ValidatesExactProfileAndGlobalPointsNotTotalOrMapScore)
{
	const auto Parse = [](const char *pJson, const char *pName = "name") { return ParsePoints(pJson, strlen(pJson), pName); };
	EXPECT_EQ(Parse(R"({"player":"name","points":{"points":4321,"total":99999},"score":10})"), 4321);
	EXPECT_EQ(Parse(R"({"player":"name","points":{"points":0}})"), 0);
	for(const char *pJson : {"{}", "null", "[]", "{", R"({"player":"other","points":{"points":88}})",
		    R"({"player":"name","points":{"total":88}})", R"({"player":"name","points":{"points":null}})",
		    R"({"player":"name","points":{"points":-1}})", R"({"player":"name","points":{"points":1.5}})",
		    R"({"player":"name","points":{"points":"88"}})", R"({"player":"name","points":{"points":2147483648}})",
		    R"({"player":"name\u0000other","points":{"points":88}})"})
		EXPECT_EQ(Parse(pJson), UnknownPoints);
	EXPECT_EQ(Parse(R"({"player":"Игрок雪","points":{"points":42}})", "Игрок雪"), 42);
	EXPECT_EQ(Parse(R"({"player":"Name","points":{"points":42}})"), UnknownPoints);
	EXPECT_EQ(ParsePoints(nullptr, 0, "name"), UnknownPoints);
	EXPECT_EQ(ParsePoints("{}", 2 * 1024 * 1024 + 1, "name"), UnknownPoints);
}
TEST(LucentScoreboardPoints, CacheDeduplicatesLimitsConcurrencyAndPreservesUnknownVersusZero)
{
	CPointsCache C;
	const int A = C.Touch("Игрок雪", 0), B = C.Touch("other", 0);
	EXPECT_EQ(A, C.Touch("Игрок雪", 0));
	C.Touch("third", 0);
	C.Touch("fourth", 0);
	C.Touch("fifth", 0);
	EXPECT_EQ(C.Start(0), A);
	EXPECT_EQ(C.Start(.25), -1);
	EXPECT_EQ(C.Start(.5), B);
	EXPECT_EQ(C.Start(1), 2);
	EXPECT_EQ(C.Start(1.5), 3);
	EXPECT_EQ(C.Start(2), -1);
	C.Complete(A, 0, 2);
	EXPECT_EQ(C.Lookup("Игрок雪", 3), 0);
	EXPECT_EQ(C.Lookup("other", 3), UnknownPoints);
	EXPECT_EQ(C.Start(3), 4);
	EXPECT_EQ(C.Lookup("Игрок雪", 1802), UnknownPoints);
	C.Complete(B, UnknownPoints, 4);
	C.Begin();
	C.Touch("other", 5);
	EXPECT_EQ(C.Start(123), -1);
	EXPECT_EQ(C.Start(124), B);
}

namespace
{
	class CResponse : public IHttpRequest
	{
	public:
		explicit CResponse(const char *pUrl) : IHttpRequest(pUrl) {}
		void Header(const char *) override {}
		const char *Url() const { return m_aUrl; }
		void Finish(const char *pJson, int Code = 200)
		{
			const size_t Size = strlen(pJson);
			m_pBuffer = static_cast<unsigned char *>(malloc(Size + 1));
			memcpy(m_pBuffer, pJson, Size + 1);
			m_ResponseLength = Size;
			m_StatusCode = Code;
			m_State = EHttpState::DONE;
		}
		void Cancelled() { m_State = EHttpState::ABORTED; }
		bool FailsOnHttpError() const { return m_FailOnErrorStatus; }
		void FailTransport(EHttpState State) { m_State = State; }
	};
	class CHttpRecorder : public IHttp
	{
	public:
		std::vector<std::shared_ptr<CResponse>> m_vRequests;
		std::vector<double> m_vStarts;
		double m_Now = 0;
		void Run(std::shared_ptr<IHttpRequest> pRequest) override
		{
			m_vRequests.push_back(std::static_pointer_cast<CResponse>(pRequest));
			m_vStarts.push_back(m_Now);
		}
		bool HasIpresolveBug() const override { return false; }
	};
	class CWorker : public IEngine
	{
	public:
		std::vector<std::shared_ptr<IJob>> m_vJobs;
		void Init() override {}
		void AddJob(std::shared_ptr<IJob> pJob) override { m_vJobs.push_back(std::move(pJob)); }
		void ShutdownJobs() override {}
		void SetAdditionalLogger(std::shared_ptr<ILogger> &&) override {}
		void Flush()
		{
			CJobPool Pool;
			Pool.Init(1);
			for(auto &Job : m_vJobs)
				Pool.Add(Job);
			Pool.Shutdown();
			m_vJobs.clear();
		}
	};
	CPointsService::FRequestFactory Factory()
	{
		return [](const char *pUrl) { return std::make_shared<CResponse>(pUrl); };
	}
}
TEST(LucentScoreboardPoints, ServiceUsesWorkerCachesAndMatchesNamesRatherThanPlayerSlots)
{
	CHttpRecorder Http;
	CWorker Engine;
	CPointsService S(Factory());
	std::array<const char *, 128> Names{};
	Names[7] = "Игрок雪";
	Names[99] = "Игрок雪";
	S.Update(Names, 0, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	EXPECT_NE(str_find(Http.m_vRequests[0]->Url(), "%"), nullptr);
	S.Update(Names, .1, &Http, &Engine);
	EXPECT_EQ(Http.m_vRequests.size(), 1u);
	Http.m_vRequests[0]->Finish(R"({"player":"Игрок雪","points":{"points":0}})");
	S.Update(Names, .3, &Http, &Engine);
	EXPECT_EQ(S.Lookup("Игрок雪", .3), UnknownPoints);
	ASSERT_EQ(Engine.m_vJobs.size(), 1u);
	Engine.Flush();
	S.Update(Names, .6, &Http, &Engine);
	EXPECT_EQ(S.Lookup("Игрок雪", .6), 0);
	Names[7] = "different";
	Names[99] = nullptr;
	EXPECT_EQ(S.Lookup(Names[7], .6), UnknownPoints);
	S.Reset();
	S.Update(Names, 2, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 2u);
	Http.m_vRequests[1]->Finish(R"({"player":"different","points":{"points":123}})");
	S.Update(Names, 2.3, &Http, &Engine);
	Engine.Flush();
	S.Update(Names, 2.6, &Http, &Engine);
	EXPECT_EQ(S.Lookup("different", 2.6), 123);
	EXPECT_EQ(S.Lookup("Игрок雪", 2.6), 0);
}
TEST(LucentScoreboardPoints, CancellationPinsSlotsAndServerResetDiscardsLateParserResults)
{
	CHttpRecorder Http;
	CWorker Engine;
	CPointsService S(Factory());
	std::array<const char *, 128> Names{};
	Names[0] = "a";
	Names[1] = "b";
	Names[2] = "c";
	Names[3] = "d";
	S.Update(Names, 0, &Http, &Engine);
	S.Update(Names, .5, &Http, &Engine);
	S.Update(Names, 1, &Http, &Engine);
	S.Update(Names, 1.5, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 4u);
	S.Reset();
	Names[0] = "new";
	Names[1] = Names[2] = Names[3] = nullptr;
	S.Update(Names, 2, &Http, &Engine);
	EXPECT_EQ(Http.m_vRequests.size(), 4u);
	EXPECT_TRUE(Http.m_vRequests[0]->IsAbortRequested());
	for(auto &Request : Http.m_vRequests)
		Request->Cancelled();
	S.Update(Names, 3, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 5u);
	Http.m_vRequests[4]->Finish(R"({"player":"new","points":{"points":123}})");
	S.Update(Names, 3.3, &Http, &Engine);
	ASSERT_EQ(Engine.m_vJobs.size(), 1u);
	S.Reset();
	Engine.Flush();
	S.Update(Names, 3.6, &Http, &Engine);
	EXPECT_EQ(S.Lookup("new", 3.6), UnknownPoints);
}
TEST(LucentScoreboardPoints, HttpErrorsBackOffAndNeverBecomeZero)
{
	for(int Code : {429, 503})
	{
		CHttpRecorder Http;
		CWorker Engine;
		CPointsService S(Factory());
		std::array<const char *, 128> Names{};
		Names[0] = "a";
		Names[1] = "b";
		S.Update(Names, 0, &Http, &Engine);
		ASSERT_EQ(Http.m_vRequests.size(), 1u);
		EXPECT_FALSE(Http.m_vRequests[0]->FailsOnHttpError());
		Http.m_vRequests[0]->Finish("{}", Code);
		S.Update(Names, .3, &Http, &Engine);
		S.Update(Names, 120, &Http, &Engine);
		EXPECT_EQ(Http.m_vRequests.size(), 1u);
		EXPECT_EQ(S.Lookup("a", 1), UnknownPoints);
		S.Update(Names, 121, &Http, &Engine);
		EXPECT_EQ(Http.m_vRequests.size(), 2u);
		EXPECT_TRUE(Engine.m_vJobs.empty());
	}
}

TEST(LucentScoreboard, PointsFieldsAreOrderedNonOverlappingAndUseOneFont)
{
	for(float Aspect : {.75f, 1.f, 4.f / 3, 16.f / 9, 21.f / 9, 32.f / 9})
		for(int Count : {1, 2, 16, 32, 64, 128})
			for(bool Teams : {false, true})
			{
				const float Needed = CFields::MinimumWidth(82, 48, 28);
				const auto L = Calculate(600 * Aspect, 600, Teams ? Count / 2 : Count, Teams ? Count - Count / 2 : 0, Teams, 0, Needed, Needed);
				const float Font = CommonFont(L.m_Row, L.ColumnWidth(), Needed);
				EXPECT_LE(Font, L.m_Row * .65f + .001f);
				const auto F = CFields::Calculate(L.ColumnWidth(), Font, 82, 48, 28, 500, 500);
				EXPECT_GT(F.m_NameWidth, Font);
				EXPECT_GT(F.m_ClanWidth, Font);
				EXPECT_LE(F.m_Score + F.m_ScoreWidth, F.m_Tee);
				EXPECT_LE(F.m_Tee + F.m_TeeWidth, F.m_Name);
				EXPECT_LE(F.m_Name + F.m_NameWidth, F.m_Clan);
				EXPECT_LE(F.m_Clan + F.m_ClanWidth, F.m_Country + .001f);
				EXPECT_LE(F.m_Country + F.m_CountryWidth, F.m_Points + .001f);
				EXPECT_LE(F.m_Points + F.m_PointsWidth, F.m_Ping + .001f);
				EXPECT_LE(F.m_Ping + F.m_PingWidth, L.ColumnWidth());
				for(int C = 0; C < L.m_Columns; ++C)
					EXPECT_FLOAT_EQ(Font, CommonFont(L.m_Row, L.ColumnWidth(), Needed));
			}
}
TEST(LucentScoreboard, UnicodeFieldsAreSingleLineWithoutPerNameFontShrinking)
{
	char aName[] = "ОченьДлинный Unicode雪 Clan\r\nline\tmore";
	SingleLine(aName);
	EXPECT_EQ(str_find(aName, "\n"), nullptr);
	EXPECT_EQ(str_find(aName, "\r"), nullptr);
	EXPECT_NE(str_find(aName, "雪"), nullptr);
	EXPECT_NE(SingleLineTextFlags & TEXTFLAG_STOP_AT_END, 0);
	EXPECT_NE(SingleLineTextFlags & TEXTFLAG_DISALLOW_NEWLINE, 0);
	EXPECT_NE(SingleLineTextFlags & TEXTFLAG_ELLIPSIS_AT_END, 0);
}

TEST(LucentScoreboardPoints, NegativeCacheCannotStarveNeverRequestedNames)
{
	CPointsCache Cache;
	for(int I = 0; I < 128; ++I)
		Cache.Touch("name" + std::to_string(I), 0);
	std::array<int, 128> Seen{};
	for(int I = 0; I < 128; ++I)
	{
		const int Slot = Cache.Start(I + 1);
		ASSERT_GE(Slot, 0);
		ASSERT_LT(Slot, 128);
		++Seen[Slot];
		Cache.Complete(Slot, UnknownPoints, I + 1);
	}
	for(int Visits : Seen)
		EXPECT_EQ(Visits, 1);
}
TEST(LucentScoreboard, UnbalancedTeamsReceiveDifferentColumnAllocations)
{
	const auto L = Calculate(1066, 600, 128, 0, true, 0, 440, 400);
	EXPECT_GT(L.m_RedColumns, L.m_BlueColumns);
	EXPECT_EQ(L.m_BlueColumns, 1);
	EXPECT_EQ(L.m_RedColumns + L.m_BlueColumns, L.m_Columns);
	int Seen = 0;
	for(int C = 0; C < L.m_RedColumns; ++C)
	{
		const auto Range = ColumnRange(128, C, L.m_RedColumns);
		Seen += Range.m_End - Range.m_Start;
	}
	EXPECT_EQ(Seen, 128);
}

TEST(LucentScoreboard, DDRaceTeamCaptionsHaveTheirOwnGutterBeforeTime)
{
	for(float Width : {170.f, 250.f, 400.f, 600.f})
	{
		const float Needed = CFields::MinimumWidth(90, 48, 28, 64);
		const float Font = CommonFont(14, Width, Needed);
		const auto F = CFields::Calculate(Width, Font, 90, 48, 28, 500, 200, 64);
		EXPECT_GE(F.m_Score, (7 + 64) * Font / 12 - .001f);
		EXPECT_GT(F.m_NameWidth, Font);
		EXPECT_GT(F.m_ClanWidth, Font);
		EXPECT_LE(F.m_Points + F.m_PointsWidth, F.m_Ping);
	}
}

TEST(LucentScoreboard, SparseRowsAllocateMeasuredContentBeforeEllipsizing)
{
	const float Preferred = CFields::PreferredWidth(90, 48, 28, 140, 90, 64);
	const float Minimum = CFields::MinimumWidth(90, 48, 28, 64);
	const auto L = Calculate(1066, 600, 1, 0, false, 0, Preferred, Minimum);
	ASSERT_EQ(L.m_Columns, 1);
	EXPECT_GE(L.ColumnWidth(), Preferred);
	const float Font = CommonFont(L.m_Row, L.ColumnWidth(), Minimum);
	EXPECT_FLOAT_EQ(Font, 12);
	const auto Fields = CFields::Calculate(L.ColumnWidth(), Font, 90, 48, 28, 140, 90, 64);
	EXPECT_GE(Fields.m_NameWidth - Font, 140);
	EXPECT_GE(Fields.m_ClanWidth, 90);
	const float ClanHeavy = CFields::PreferredWidth(90, 48, 28, 8, 200);
	const auto HeavyFields = CFields::Calculate(ClanHeavy, 12, 90, 48, 28, 8, 200);
	EXPECT_GE(HeavyFields.m_NameWidth - 12, 8);
	EXPECT_GE(HeavyFields.m_ClanWidth, 200);
}

TEST(LucentScoreboardPoints, TransportErrorsAndUnexpectedAbortsNeverReadResponseStatus)
{
	for(const auto State : {EHttpState::ERROR, EHttpState::ABORTED})
	{
		CHttpRecorder Http;
		CWorker Engine;
		CPointsService Service(Factory());
		std::array<const char *, 128> Names{};
		Names[0] = "failed";
		Names[1] = "next";
		Service.Update(Names, 0, &Http, &Engine);
		ASSERT_EQ(Http.m_vRequests.size(), 1u);
		Http.m_vRequests[0]->FailTransport(State);
		// Native StatusCode() asserts for both of these terminal states.
		Service.Update(Names, .3, &Http, &Engine);
		EXPECT_EQ(Service.Lookup("failed", .3), UnknownPoints);
		EXPECT_TRUE(Engine.m_vJobs.empty());
		Service.Update(Names, 10, &Http, &Engine);
		EXPECT_EQ(Http.m_vRequests.size(), 1u);
		Service.Update(Names, 11, &Http, &Engine);
		ASSERT_EQ(Http.m_vRequests.size(), 2u);
		EXPECT_NE(str_find(Http.m_vRequests.back()->Url(), "json2=next"), nullptr);
		Http.m_vRequests.back()->Finish(R"({"player":"next","points":{"points":42}})");
		Service.Update(Names, 11.3, &Http, &Engine);
		Engine.Flush();
		Service.Update(Names, 11.6, &Http, &Engine);
		EXPECT_EQ(Service.Lookup("next", 11.6), 42);
		EXPECT_EQ(Service.Lookup("failed", 11.6), UnknownPoints);
	}
}
TEST(LucentScoreboardPoints, TransportFailureAfterNineProfilesKeepsSuccessfulPointsAndQueue)
{
	CHttpRecorder Http;
	CWorker Engine;
	CPointsService Service(Factory());
	std::array<const char *, 128> Names{};
	std::array<std::string, 11> Keys;
	for(int I = 0; I < 11; ++I)
	{
		Keys[I] = "player" + std::to_string(I);
		Names[I] = Keys[I].c_str();
	}
	for(int I = 0; I < 9; ++I)
	{
		const double Now = I * .6;
		Service.Update(Names, Now, &Http, &Engine);
		ASSERT_EQ(Http.m_vRequests.size(), size_t(I + 1));
		const std::string Json = "{\"player\":\"" + Keys[I] + "\",\"points\":{\"points\":123}}";
		Http.m_vRequests.back()->Finish(Json.c_str());
		Service.Update(Names, Now + .1, &Http, &Engine);
		Engine.Flush();
		Service.Update(Names, Now + .2, &Http, &Engine);
		EXPECT_EQ(Service.Lookup(Keys[I].c_str(), Now + .2), 123);
	}
	Service.Update(Names, 5.4, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 10u);
	Http.m_vRequests.back()->FailTransport(EHttpState::ERROR);
	Service.Update(Names, 5.5, &Http, &Engine);
	EXPECT_EQ(Service.Lookup("player9", 5.5), UnknownPoints);
	for(int I = 0; I < 9; ++I)
		EXPECT_EQ(Service.Lookup(Keys[I].c_str(), 5.5), 123);
	Service.Update(Names, 16.1, &Http, &Engine);
	ASSERT_EQ(Http.m_vRequests.size(), 11u);
	EXPECT_NE(str_find(Http.m_vRequests.back()->Url(), "json2=player10"), nullptr);
	Http.m_vRequests.back()->Finish(R"({"player":"player10","points":{"points":99}})");
	Service.Update(Names, 16.3, &Http, &Engine);
	Engine.Flush();
	Service.Update(Names, 16.5, &Http, &Engine);
	EXPECT_EQ(Service.Lookup("player10", 16.5), 99);
}

TEST(LucentScoreboardPoints, AdaptiveBudgetPacesStartsWithoutCatchupBursts)
{
	CPointsBudget B;
	std::vector<double> Starts;
	for(int Tick = 0; Tick <= 2000; ++Tick)
	{
		const double Now = Tick / 100.;
		if(B.CanStart(0, Now))
		{
			B.Started(Now);
			Starts.push_back(Now);
			B.Success(Now + .01);
		}
	}
	EXPECT_DOUBLE_EQ(B.m_Rate, 3);
	EXPECT_EQ(B.m_Parallel, 4);
	ASSERT_GT(Starts.size(), 40u);
	for(size_t I = 0; I < Starts.size(); ++I)
	{
		int Window = 0;
		for(size_t J = I; J < Starts.size() && Starts[J] < Starts[I] + 1 - 1e-8; ++J)
			++Window;
		EXPECT_LE(Window, 3);
	}
	EXPECT_TRUE(B.CanStart(0, 1000));
	B.Started(1000);
	EXPECT_FALSE(B.CanStart(0, 1000)); // Idle time does not accumulate a burst allowance.
}
TEST(LucentScoreboardPoints, AdaptiveBudgetCoalescesLimitsAndRecoversGradually)
{
	CPointsBudget B;
	B.Limited(1);
	EXPECT_DOUBLE_EQ(B.m_Rate, 1);
	EXPECT_EQ(B.m_Parallel, 2);
	B.Limited(2);
	EXPECT_EQ(B.m_Strikes, 1);
	EXPECT_DOUBLE_EQ(B.m_Rate, 1);
	EXPECT_FALSE(B.CanStart(0, 121));
	B.Limited(123);
	EXPECT_EQ(B.m_Strikes, 2);
	EXPECT_DOUBLE_EQ(B.m_Rate, .5);
	EXPECT_EQ(B.m_Parallel, 1);
	EXPECT_FALSE(B.CanStart(0, 362));
	B.Success(350);
	EXPECT_EQ(B.m_Healthy, 0);
	for(int I = 0; I < 8; ++I)
		B.Success(364 + I * 2);
	EXPECT_DOUBLE_EQ(B.m_Rate, .75);
	EXPECT_EQ(B.m_Parallel, 2);
	for(int I = 0; I < 200; ++I)
		B.Success(400 + I * 2);
	EXPECT_DOUBLE_EQ(B.m_Rate, 3);
	EXPECT_EQ(B.m_Parallel, 4);
}
TEST(LucentScoreboardPoints, UniqueRepeatedAndCachedRostersLoadWithinBoundedBudget)
{
	for(int Unique : {1, 16, 40, 64, 128})
	{
		CHttpRecorder Http;
		CWorker Engine;
		CPointsService Service(Factory());
		std::array<std::string, 128> Keys;
		std::array<const char *, 128> Names{};
		for(int I = 0; I < Unique; ++I)
			Keys[I] = "load" + std::to_string(I);
		for(int I = 0; I < 128; ++I)
			Names[I] = Keys[I % Unique].c_str();
		bool Complete = false;
		double Finished = 0;
		for(int Tick = 0; Tick <= 1800 && !Complete; ++Tick)
		{
			const double Now = Tick * .05;
			Http.m_Now = Now;
			int Active = 0;
			for(size_t I = 0; I < Http.m_vRequests.size(); ++I)
			{
				auto &R = Http.m_vRequests[I];
				if(!R->Done() && Now - Http.m_vStarts[I] >= .08)
				{
					const char *pName = str_find(R->Url(), "json2=") + 6;
					const std::string Json = std::string("{\"player\":\"") + pName + "\",\"points\":{\"points\":42}}";
					R->Finish(Json.c_str());
				}
				Active += !R->Done();
			}
			EXPECT_LE(Active, 4);
			Service.Update(Names, Now, &Http, &Engine);
			if(!Engine.m_vJobs.empty())
				Engine.Flush();
			Complete = true;
			for(int I = 0; I < Unique; ++I)
				Complete &= Service.Lookup(Keys[I].c_str(), Now) == 42;
			Finished = Now;
		}
		ASSERT_TRUE(Complete) << Unique;
		EXPECT_EQ(Http.m_vRequests.size(), size_t(Unique));
		for(size_t I = 0; I < Http.m_vStarts.size(); ++I)
		{
			int Window = 0;
			for(size_t J = I; J < Http.m_vStarts.size() && Http.m_vStarts[J] < Http.m_vStarts[I] + 1 - 1e-8; ++J)
				++Window;
			EXPECT_LE(Window, 3);
		}
		const size_t Count = Http.m_vRequests.size();
		Service.Update(Names, Finished + 1, &Http, &Engine);
		Service.Reset();
		Service.Update(Names, Finished + 2, &Http, &Engine);
		EXPECT_EQ(Http.m_vRequests.size(), Count); // Reopening / a new session does not refetch fresh names.
		RecordProperty("unique_" + std::to_string(Unique) + "_seconds", std::to_string(Finished));
		if(Unique == 128)
			EXPECT_LT(Finished, 65);
	}
}
TEST(LucentScoreboard, GutterIsLocalToOrderedColumnRangesIncludingSplitTeams)
{
	const std::array<float, 8> Ordered{0, 0, 20, 80, 80, 0, 0, 0};
	EXPECT_EQ(ColumnGutter(Ordered, ColumnRange(8, 0, 4)), 0);
	EXPECT_EQ(ColumnGutter(Ordered, ColumnRange(8, 1, 4)), 80); // Two different teams.
	EXPECT_EQ(ColumnGutter(Ordered, ColumnRange(8, 2, 4)), 80); // Same team continues across the boundary.
	EXPECT_EQ(ColumnGutter(Ordered, ColumnRange(8, 3, 4)), 0);
	EXPECT_EQ(ColumnGutter(Ordered, {0, 0}), 0);
	const auto Plain = CFields::Calculate(500, 8, 90, 48, 28, 140, 90);
	const auto Grouped = CFields::Calculate(500, 8, 90, 48, 28, 140, 90, 80);
	EXPECT_NEAR(Plain.m_NameWidth + Plain.m_ClanWidth - Grouped.m_NameWidth - Grouped.m_ClanWidth, 80 * 8.f / 12, .001f);
	EXPECT_GT(Plain.m_NameWidth, Grouped.m_NameWidth);
	EXPECT_GT(Plain.m_ClanWidth, Grouped.m_ClanWidth);
	EXPECT_FLOAT_EQ(Plain.m_Points, Grouped.m_Points);
	EXPECT_FLOAT_EQ(Plain.m_Ping, Grouped.m_Ping);
	EXPECT_FLOAT_EQ(Plain.m_Country, Grouped.m_Country);
}
