#include "test.h"

#include <base/str.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/shared/jobs.h>

#include <game/client/lucent/scoreboard/identity_text.h>
#include <game/client/lucent/scoreboard/layout.h>
#include <game/client/lucent/scoreboard/panel.h>
#include <game/client/lucent/scoreboard/points.h>
#include <game/client/lucent/scoreboard/row_data.h>

#include <gtest/gtest.h>

#if defined(CONF_SCOREBOARD_FREETYPE_TEST)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
				EXPECT_GT(F.m_IdentityWidth, Font);
				EXPECT_LE(F.m_Name + F.m_IdentityWidth, F.m_Country);
				EXPECT_LE(F.m_Score + F.m_ScoreWidth, F.m_Tee);
				EXPECT_LE(F.m_Tee + F.m_TeeWidth, F.m_Name);
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
		EXPECT_GT(F.m_IdentityWidth, Font);
		EXPECT_LE(F.m_Name + F.m_IdentityWidth, F.m_Country);
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
	EXPECT_GE(Fields.m_IdentityWidth - Font, 140);
	const float ClanHeavy = CFields::PreferredWidth(90, 48, 28, 8, 200);
	const auto HeavyFields = CFields::Calculate(ClanHeavy, 12, 90, 48, 28, 8, 200);
	EXPECT_GE(HeavyFields.m_IdentityWidth - 12, 8);
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
	EXPECT_NEAR(Plain.m_IdentityWidth - Grouped.m_IdentityWidth, 80 * 8.f / 12, .001f);
	EXPECT_FLOAT_EQ(Plain.m_Points, Grouped.m_Points);
	EXPECT_FLOAT_EQ(Plain.m_Ping, Grouped.m_Ping);
	EXPECT_FLOAT_EQ(Plain.m_Country, Grouped.m_Country);
}

TEST(LucentScoreboard, ClanPillsFitDenseTeamAndDDRaceRowsWithoutMovingNumericFields)
{
	for(int Count : {1, 2, 16, 32, 64, 128})
		for(float Aspect : {.75f, 16.f / 9, 32.f / 9})
			for(bool Teams : {false, true})
				for(float Gutter : {0.f, 64.f})
					for(int Spectators : {0, 128 - Count})
					{
						const float Minimum = CFields::MinimumWidth(90, 48, 28, Gutter);
						const auto L = Calculate(600 * Aspect, 600, Teams ? Count / 2 : Count, Teams ? Count - Count / 2 : 0, Teams, Spectators, 600, Minimum);
						const float Font = CommonFont(L.m_Row, L.ColumnWidth(), Minimum);
						const auto F = CFields::Calculate(L.ColumnWidth(), Font, 90, 48, 28, 600, 200, Gutter);
						const float Width = F.m_Country - F.m_Name - 4 * Font / 12;
						for(float Name : {24.f, 600.f})
							for(float Clan : {0.f, 18.f, 200.f, 1200.f})
								for(float Ready : {0.f, Font})
								{
									const auto I = CIdentity::Calculate(Width, Font, L.m_Row, Name, Clan, Clan > 0, Ready);
									EXPECT_GT(I.m_NameWidth, 0);
									EXPECT_LE(I.m_Height, L.m_Row);
									const float End = F.m_Name + I.ClanOffset(I.m_NameWidth) + I.m_ClanWidth;
									EXPECT_LE(End, F.m_Country - 4 * Font / 12 + .001f);
									EXPECT_LE(F.m_Country + F.m_CountryWidth, F.m_Points);
									EXPECT_LE(F.m_Points + F.m_PointsWidth, F.m_Ping);
									if(Clan > 0)
									{
										EXPECT_GT(I.ClanTextWidth(), 0);
										EXPECT_LE(2 * I.m_Padding + I.ClanTextWidth(), I.m_ClanWidth + .001f);
									}
								}
					}
}

TEST(LucentScoreboard, ClanPillsFollowShortNamesAndReleaseEmptyClanSpace)
{
	const auto Short = CIdentity::Calculate(220, 12, 22, 24, 18, true, 12);
	EXPECT_FLOAT_EQ(Short.ClanOffset(24), 24 + 12 + LucentStyle::Scoreboard::ClanGap);
	EXPECT_LT(Short.m_ClanWidth, 30);
	const auto Long = CIdentity::Calculate(220, 12, 22, 600, 1200, true, 12);
	const auto Empty = CIdentity::Calculate(220, 12, 22, 600, 0, false, 12);
	EXPECT_FLOAT_EQ(Empty.m_ClanWidth, 0);
	EXPECT_FLOAT_EQ(Empty.m_Gap, 0);
	EXPECT_FLOAT_EQ(Empty.m_NameWidth, 208);
	EXPECT_GT(Empty.m_NameWidth, Long.m_NameWidth);
	EXPECT_FLOAT_EQ(Long.ClanOffset(Long.m_NameWidth / 2), Long.m_NameWidth / 2 + 12 + LucentStyle::Scoreboard::ClanGap);
	// Unknown, zero and known points have the same pre-reserved Pts cell in production.
	for(int Points : {UnknownPoints, 0, 123456})
	{
		CPointsCache Cache;
		const int Slot = Cache.Touch("name", 0);
		Cache.Complete(Slot, Points, 0);
		EXPECT_EQ(Cache.Lookup("name", 1), Points);
		const auto F = CFields::Calculate(500, 12, 90, 48, 28, 600, 1200);
		const auto I = CIdentity::Calculate(F.m_Country - F.m_Name - 4, 12, 22, 600, 1200, true, 12);
		EXPECT_LT(F.m_Name + I.ClanOffset(I.m_NameWidth) + I.m_ClanWidth, F.m_Country);
	}
}

TEST(LucentScoreboard, UnicodeClanPillsKeepSingleLineTextAndCoverageAtPhysicalResolutions)
{
	char aClan[] = "ОченьДлинныйКлан雪\r\nClan\t";
	SingleLine(aClan);
	EXPECT_EQ(str_find(aClan, "\n"), nullptr);
	EXPECT_EQ(str_find(aClan, "\r"), nullptr);
	EXPECT_EQ(str_find(aClan, "\t"), nullptr);
	EXPECT_NE(str_find(aClan, "雪"), nullptr);
	for(float Font : {3.f, 6.f, 12.f})
		for(float Pixel : {.25f, .5f, 1.f, 1.25f})
		{
			const auto I = CIdentity::Calculate(160, Font, Font / .65f, 600, 1200, true, 0);
			LucentScoreboard::CPanelCache Cache;
			const LucentHud::CRect Rect{I.ClanOffset(I.m_NameWidth), 0, I.m_ClanWidth, I.m_Height};
			const auto &G = Cache.Get(Rect, {{0, 0}, {Pixel, Pixel}});
			EXPECT_GT(G.m_Count, 0);
			EXPECT_GT(G.m_Radius, 0);
			EXPECT_LE(G.m_Radius, G.m_Bounds.h / 2);
			EXPECT_EQ(&G, &Cache.Get(Rect, {{0, 0}, {Pixel, Pixel}}));
		}
}

TEST(LucentScoreboard, IdentityUsesMeasuredPairWithoutPrematureEllipsis)
{
	for(float Name : {24.f, 180.f, 600.f})
		for(float Clan : {0.f, 55.f, 300.f, 1200.f})
		{
			const float Pill = Clan > 0 ? CIdentity::ClanDemand(Clan) : 0;
			const float Width = Name + (Clan > 0 ? Pill + 4 : 0) + 12;
			const auto I = CIdentity::Calculate(Width, 12, 22, Name, Clan, Clan > 0, 12);
			EXPECT_FLOAT_EQ(I.m_NameWidth, Name);
			EXPECT_FLOAT_EQ(I.m_ClanWidth, Pill);
		}
	const auto Nephritis = CIdentity::Calculate(220, 12, 22, 180, 55, true, 0);
	EXPECT_FLOAT_EQ(Nephritis.m_ClanWidth, CIdentity::ClanDemand(55));
	EXPECT_GT(Nephritis.m_NameWidth, 140);
	const auto LongClan = CIdentity::Calculate(220, 12, 22, 24, 1200, true, 0);
	EXPECT_FLOAT_EQ(LongClan.m_NameWidth, 24);
	EXPECT_GT(LongClan.m_ClanWidth, 100);
}

namespace
{
	// Records the actual CCachedText submission boundary. Glyph metrics are a
	// deterministic model of text.cpp's bounded/unbounded and pixel-size rules;
	// this is deliberately not a GPU/font rasterization test.
	struct CIdentityTextRecorder
	{
		struct CSubmission
		{
			std::string m_Text;
			float m_Font, m_Limit, m_FullWidth;
			int m_Flags;
			bool m_Truncated;
			bool m_TrackInk;
			unsigned m_RenderFlags;
		};
		std::vector<CSubmission> m_vSubmissions;
		std::vector<vec2> m_vPositions;
		float m_PixelScale = 2;
		int m_Hard = 0, m_Soft = 0;
		unsigned m_RenderFlags = 0;
		unsigned GetRenderFlags() const { return m_RenderFlags; }
		void SetRenderFlags(unsigned Flags) { m_RenderFlags = Flags; }
		ColorRGBA GetTextColor() const { return ColorRGBA(1, 1, 1, 1); }
		ColorRGBA DefaultTextColor() const { return GetTextColor(); }
		ColorRGBA DefaultTextOutlineColor() const { return ColorRGBA(0, 0, 0, 1); }
		void TextColor(ColorRGBA) {}
		virtual float TextWidth(float Font, const char *pText, int = -1, float Limit = -1, int = 0) const
		{
			int Glyphs = 0;
			for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pText); *p; ++p)
				Glyphs += (*p & 0xc0) != 0x80;
			const float Aligned = std::floor(Font * m_PixelScale) / m_PixelScale;
			return std::max(0.f, Glyphs * Aligned * .6f - (Limit <= 0 ? Aligned * .25f : 0));
		}
		virtual void TextEx(CTextCursor *pCursor, const char *pText)
		{
			const float Full = TextWidth(pCursor->m_FontSize, pText, -1, pCursor->m_LineWidth);
			pCursor->m_LongestLineWidth = std::min(Full, pCursor->m_LineWidth);
			pCursor->m_AlignedFontSize = std::floor(pCursor->m_FontSize * m_PixelScale) / m_PixelScale;
			if(pCursor->m_TrackInk && pCursor->m_LongestLineWidth > 3 / m_PixelScale)
				pCursor->m_Ink.Include(1 / m_PixelScale, pCursor->m_LongestLineWidth - 2 / m_PixelScale);
		}
		void Submit(STextContainerIndex &Index, CTextCursor *pCursor, const char *pText)
		{
			Index.m_Index = 0;
			TextEx(pCursor, pText);
			const float Full = TextWidth(pCursor->m_FontSize, pText, -1, pCursor->m_LineWidth);
			pCursor->m_Truncated = Full > pCursor->m_LineWidth;
			pCursor->m_LongestLineWidth = std::min(Full, pCursor->m_LineWidth);
			pCursor->m_AlignedFontSize = std::floor(pCursor->m_FontSize * m_PixelScale) / m_PixelScale;
			m_vSubmissions.push_back({pText, pCursor->m_FontSize, pCursor->m_LineWidth, Full, pCursor->m_Flags, pCursor->m_Truncated, pCursor->m_TrackInk, m_RenderFlags});
		}
		void RecreateTextContainer(STextContainerIndex &Index, CTextCursor *pCursor, const char *pText)
		{
			++m_Hard;
			Submit(Index, pCursor, pText);
		}
		void RecreateTextContainerSoft(STextContainerIndex &Index, CTextCursor *pCursor, const char *pText)
		{
			++m_Soft;
			Submit(Index, pCursor, pText);
		}
		void RenderTextContainer(STextContainerIndex, ColorRGBA, ColorRGBA, float X, float Y) { m_vPositions.emplace_back(X, Y); }
		void DeleteTextContainer(STextContainerIndex &Index) { Index.Reset(); }
	};
}

TEST(LucentScoreboardText, WideTwoColumnsSubmitFullIdentityAndUseVisibleNameForPill)
{
	CIdentityTextRecorder Renderer;
	CCachedText Name, Clan;
	// Two wide columns, with over 300 physical pixels left beyond this pair.
	CLayout Wide;
	Wide.m_Columns = 2;
	Wide.m_Width = 1610;
	const auto F = CFields::Calculate(Wide.ColumnWidth(), 12, 90, 48, 28, 0, 0);
	const auto I = PrepareIdentityText(&Renderer, Name, Clan, "apolemia", "Nephritis", F.m_IdentityWidth, 12, 22, 0, vec2(2, 2));
	ASSERT_EQ(Renderer.m_vSubmissions.size(), 2u);
	for(const auto &S : Renderer.m_vSubmissions)
	{
		EXPECT_FALSE(S.m_Truncated);
		EXPECT_GE(S.m_Limit, S.m_FullWidth);
		EXPECT_EQ(S.m_Flags & TEXTFLAG_ELLIPSIS_AT_END, 0);
		EXPECT_NE(S.m_Flags & TEXTFLAG_STOP_AT_END, 0);
		EXPECT_NE(S.m_Flags & TEXTFLAG_DISALLOW_NEWLINE, 0);
	}
	EXPECT_GT((F.m_IdentityWidth - Name.Width() - I.m_Gap - I.m_ClanWidth) * 2, 300);
	const float Pill = F.m_Name + I.ClanOffset(Name.Width());
	Name.Render(&Renderer, vec2(F.m_Name, 40), ColorRGBA(1, 1, 1, 1));
	Clan.Render(&Renderer, vec2(Pill + I.m_Padding, 40), ColorRGBA(1, 1, 1, 1));
	EXPECT_FLOAT_EQ(Pill, F.m_Name + Name.Width() + I.m_Gap);
	EXPECT_LE(Pill + I.m_ClanWidth, F.m_Country);
	ASSERT_EQ(Renderer.m_vPositions.size(), 2u);
	EXPECT_FLOAT_EQ(Renderer.m_vPositions[1].x, Pill + I.m_Padding);
	// Reproduce the old path through the same cache submission code.
	CIdentityTextRecorder OldRenderer;
	CCachedText OldName;
	OldName.Update(&OldRenderer, "apolemia", 12, OldRenderer.TextWidth(12, "apolemia"), SingleLineTextFlags);
	ASSERT_EQ(OldRenderer.m_vSubmissions.size(), 1u);
	EXPECT_TRUE(OldRenderer.m_vSubmissions[0].m_Truncated);
}

TEST(LucentScoreboardText, CacheInvalidatesForRenameReflowFontAndMapping)
{
	CIdentityTextRecorder R;
	CCachedText N, C;
	const auto Prepare = [&](const char *pName, const char *pClan, float Width, float Font) {
		return PrepareIdentityText(&R, N, C, pName, pClan, Width, Font, 22, 0, vec2(R.m_PixelScale, R.m_PixelScale));
	};
	Prepare("apolemia", "Nephritis", 500, 12);
	const size_t Initial = R.m_vSubmissions.size();
	Prepare("apolemia", "Nephritis", 500, 12);
	EXPECT_EQ(R.m_vSubmissions.size(), Initial);
	Prepare("apolemib", "Nephritis", 500, 12);
	EXPECT_EQ(R.m_Soft, 1); // Same width and flags, different content.
	const int Hard = R.m_Hard;
	Prepare("apolemib", "Nephritis", 100, 12);
	EXPECT_GT(R.m_Hard, Hard);
	Prepare("apolemia", "Nephritis", 500, 12);
	EXPECT_FALSE(R.m_vSubmissions.back().m_Truncated);
	R.m_PixelScale = 1.97f; // Scoreboard reveal mapping / framebuffer change.
	const int BeforeMapping = R.m_Hard;
	Prepare("apolemia", "Nephritis", 500, 12);
	EXPECT_GE(R.m_Hard, BeforeMapping + 2);
	Prepare("apolemia", "Nephritis", 500, 8);
	const auto ClanSubmission = std::find_if(R.m_vSubmissions.rbegin(), R.m_vSubmissions.rend(), [](const auto &S) { return S.m_Text == "Nephritis"; });
	ASSERT_NE(ClanSubmission, R.m_vSubmissions.rend());
	EXPECT_FLOAT_EQ(ClanSubmission->m_Font, 8 * LucentStyle::Scoreboard::ClanFontScale);
	const auto Empty = Prepare("apolemia", "", 500, 8);
	EXPECT_FLOAT_EQ(Empty.m_Gap + Empty.m_ClanWidth + Empty.m_Padding, 0);
	N.Reset(&R);
	const size_t BeforeReset = R.m_vSubmissions.size();
	Prepare("apolemia", "", 500, 8);
	EXPECT_GT(R.m_vSubmissions.size(), BeforeReset);
}

TEST(LucentScoreboardText, DenseDDTeamAndFourColumnSubmissionsStayBeforeFlags)
{
	for(int Count : {1, 2, 16, 32, 64, 128})
		for(int Columns : {1, 2, 3, 4})
			for(float Gutter : {0.f, 64.f})
				for(float PixelScale : {1.25f, 2.f, 3.f})
				{
					const float Width = (1400.f - (Columns - 1) * ColumnGap) / Columns;
					const float Row = std::min(22.f, 360.f / ((Count + Columns - 1) / Columns));
					const float Font = CommonFont(Row, Width, CFields::MinimumWidth(90, 48, 28, Gutter));
					const auto F = CFields::Calculate(Width, Font, 90, 48, 28, 0, 0, Gutter);
					CIdentityTextRecorder R;
					R.m_PixelScale = PixelScale;
					CCachedText N, C;
					const auto I = PrepareIdentityText(&R, N, C, "ОченьДлинноеИмя雪雪雪雪雪雪雪雪雪雪", "ОченьДлинныйКлан雪雪雪", F.m_IdentityWidth, Font, Row, 0, vec2(PixelScale, PixelScale));
					ASSERT_EQ(R.m_vSubmissions.size(), 2u);
					EXPECT_LE(F.m_Name + I.ClanOffset(N.Width()) + I.m_ClanWidth, F.m_Country);
					for(const auto &S : R.m_vSubmissions)
					{
						EXPECT_GT(S.m_Limit, 0);
						if(S.m_FullWidth > S.m_Limit)
							EXPECT_NE(S.m_Flags & TEXTFLAG_ELLIPSIS_AT_END, 0);
					}
				}
}

TEST(LucentScoreboardText, CompactPairSetsPanelWidthAndSubmitsCompleteText)
{
	for(const auto &Pair : {std::pair{"apolemia", "Nephritis"}, std::pair{"TyDoP", "Nexusion"}})
	{
		CIdentityTextRecorder R;
		const float Name = IdentityTextWidth(&R, 12, Pair.first);
		const float Clan = std::ceil(IdentityTextWidth(&R, 12 * LucentStyle::Scoreboard::ClanFontScale, Pair.second) * R.m_PixelScale) / R.m_PixelScale;
		const float Demand = Name + Clan + 2 * LucentStyle::Scoreboard::ClanPadding + LucentStyle::Scoreboard::ClanGap;
		const float Preferred = CFields::PreferredWidth(90, 48, 28, Demand, 0);
		const auto L = Calculate(2133, 600, 16, 16, true, 0, Preferred, CFields::MinimumWidth(90, 48, 28));
		EXPECT_EQ(L.m_Columns, 2);
		EXPECT_NEAR(L.ColumnWidth(), Preferred, .001f);
		const auto F = CFields::Calculate(L.ColumnWidth(), 12, 90, 48, 28, 0, 0);
		CCachedText N, C;
		const auto I = PrepareIdentityText(&R, N, C, Pair.first, Pair.second, F.m_IdentityWidth, 12, 22, 0, vec2(2, 2));
		ASSERT_EQ(R.m_vSubmissions.size(), 2u);
		for(const auto &S : R.m_vSubmissions)
			EXPECT_FALSE(S.m_Truncated);
		EXPECT_LE(F.m_IdentityWidth - N.Width() - I.m_Gap - I.m_ClanWidth, std::max(0.f, 112 - Demand) + 2.f);
		EXPECT_LE(F.m_Name + I.ClanOffset(N.Width()) + I.m_ClanWidth, F.m_Country + .001f);
	}
}

TEST(LucentScoreboard, ContentSizedColumnsKeepAllPlayersAndStayInsideViewport)
{
	for(int Count : {1, 2, 16, 32, 64, 128})
		for(bool Teams : {false, true})
			for(float Viewport : {450.f, 1066.f, 2133.f})
				for(float Demand : {100.f, 240.f, 1200.f})
				{
					const float Preferred = CFields::PreferredWidth(90, 48, 28, Demand, 0, 64);
					const auto L = Calculate(Viewport, 600, Teams ? Count / 2 : Count, Teams ? Count - Count / 2 : 0, Teams, 0, Preferred, CFields::MinimumWidth(90, 48, 28, 64));
					EXPECT_LE(L.m_Width, Viewport - 32 + .001f);
					EXPECT_LE(L.ColumnWidth(), Preferred + .001f);
					int Seen = 0;
					for(int Group = 0; Group < (Teams ? 2 : 1); ++Group)
					{
						const int Size = Teams ? (Group ? Count - Count / 2 : Count / 2) : Count;
						const int Columns = Group ? L.m_BlueColumns : L.m_RedColumns;
						for(int Column = 0; Column < Columns; ++Column)
						{
							const auto Range = ColumnRange(Size, Column, Columns);
							Seen += Range.m_End - Range.m_Start;
						}
					}
					EXPECT_EQ(Seen, Count);
				}
}

TEST(LucentScoreboard, ClanCapsulesUsePinkTokensAndCoverageWithoutBorders)
{
	const auto &Fill = LucentStyle::Scoreboard::ClanSurface;
	const auto &Text = LucentStyle::Scoreboard::ClanText;
	EXPECT_FLOAT_EQ(Fill.r, 73 / 255.f);
	EXPECT_FLOAT_EQ(Fill.g, 55 / 255.f);
	EXPECT_FLOAT_EQ(Fill.b, 69 / 255.f);
	EXPECT_GT(Text.r, Fill.r);
	EXPECT_GT(Text.b, Fill.b);
	EXPECT_GE(Fill.a, .95f); // Team fill underneath cannot wash the tag out.
	for(float Pixel : {.25f, .5f, 1.f, 1.25f})
	{
		CPanelCache Cache;
		const auto &G = Cache.Get({0, 0, 80, 15}, {{0, 0}, {Pixel, Pixel}}, true);
		EXPECT_FLOAT_EQ(G.m_Radius, std::min(G.m_Bounds.w, G.m_Bounds.h) / 2);
		EXPECT_GT(G.m_Feather, 0);
		EXPECT_GT(G.m_Count, 0);
	}
}

TEST(LucentScoreboard, CandidateWidthsUsePairedIdentityAndOnlyTheirColumnGutters)
{
	CContentWidths Content;
	Content.m_Chrome = 240;
	Content.m_Identity[0][0] = 240;
	Content.m_Identity[0][1] = 120;
	Content.m_Gutter[0][1] = 64;
	EXPECT_FLOAT_EQ(Content.Preferred(2, 0, 1, 0), 240 + 240 + 64);
	EXPECT_FLOAT_EQ(Content.Preferred(2, 0, 2, 0), 240 + 240);
	CPlanner Planner;
	CInput Input{2133, 600, 32, 0, false, 0, 0, 352};
	Input.m_Content = Content;
	const auto Before = Planner.Update(Input);
	Input.m_Content.m_Identity[0][0] = 800;
	EXPECT_GT(Planner.Update(Input).m_Width, Before.m_Width);
}

TEST(LucentScoreboard, IndependentColumnsDropAbsentTimeAndNeverReserveDDTeamGutters)
{
	CInput Input{2133, 600, 2, 2, true, 0, 0, 0};
	auto &Content = Input.m_Content;
	Content.m_Independent = true;
	Content.m_Points = 48;
	Content.m_Ping = 28;
	Content.m_Chrome = CFields::ChromeWidth(0, 48, 28);
	for(int I = 0; I < 2; ++I)
	{
		Content.m_Identity[0][I] = 300;
		Content.m_Identity[1][I] = 90;
		Content.m_Score[0][I] = 60;
		Content.m_Team[0][I] = 1;
		Content.m_GroupTitle[0][I] = 64;
	}
	CPlanner Planner;
	const auto L = Planner.Update(Input);
	ASSERT_EQ(L.m_Columns, 2);
	EXPECT_GT(L.ColumnWidth(0), L.ColumnWidth(1) + 200);
	EXPECT_FLOAT_EQ(L.ColumnWidth(0), Content.ColumnPreferred(0, {0, 2}));
	EXPECT_FLOAT_EQ(L.ColumnWidth(1), Content.ColumnPreferred(1, {0, 2}));
	EXPECT_FLOAT_EQ(L.m_Scores[0], 60);
	EXPECT_FLOAT_EQ(L.m_Scores[1], 0);
	const auto F = CFields::Calculate(L.ColumnWidth(1), 12, L.m_Scores[1], 48, 28, 0, 0);
	EXPECT_FLOAT_EQ(F.m_Tee, 7); // No time or DDTeam gutter before the tee.
	EXPECT_FLOAT_EQ(F.m_Name + F.m_IdentityWidth + 4, F.m_Country);
	const float Left = (2133 - L.m_Width) / 2;
	const auto A = ColumnBounds(L, Left, 80, 0, 2), B = ColumnBounds(L, Left, 80, 1, 2);
	EXPECT_FLOAT_EQ(B.x, A.x + A.w + L.m_Gap);
	EXPECT_NEAR(Left, 2133 - B.x - B.w, .001f);
	EXPECT_NEAR(L.m_Width, A.w + B.w + L.m_Gap, .001f);
	Input.m_Content.m_Score[1][1] = 44; // A finish arrives in the second column.
	EXPECT_FLOAT_EQ(Planner.Update(Input).m_Scores[1], 44);
}

TEST(LucentScoreboard, DDTeamContainersCoverEveryMemberAndBudgetTheirHeaderHeight)
{
	for(int Count : {1, 2, 16, 32, 64, 128})
		for(int TeamSize : {0, 1, 2, 4, 16, 128})
			for(bool GameTeams : {false, true})
				for(float Viewport : {450.f, 1066.f, 2133.f})
					for(int Spectators : {0, 128 - Count})
					{
						CInput Input{Viewport, 600, GameTeams ? Count / 2 : Count, GameTeams ? Count - Count / 2 : 0, GameTeams, Spectators, 0, 0};
						auto &C = Input.m_Content;
						C.m_Independent = true;
						C.m_Points = 48;
						C.m_Ping = 28;
						C.m_Chrome = CFields::ChromeWidth(0, 48, 28);
						for(int Group = 0; Group < (GameTeams ? 2 : 1); ++Group)
							for(int I = 0; I < (Group ? Input.m_Blue : Input.m_Red); ++I)
							{
								C.m_Team[Group][I] = TeamSize ? (TeamSize == 128 ? 64 : I / TeamSize + 1) : 0;
								C.m_Identity[Group][I] = I % 3 ? 90 : 280;
								C.m_Score[Group][I] = I % 2 ? 50 : 0;
								C.m_GroupTitle[Group][I] = TeamSize ? 65 : 0;
							}
						CPlanner Planner;
						const auto L = Planner.Update(Input);
						int Seen = 0;
						EXPECT_LE(L.m_Width, Viewport - 32 + .001f);
						EXPECT_LE(L.m_Height + L.m_SpectatorHeight, 480 + .001f);
						for(int Column = 0; Column < L.m_Columns; ++Column)
						{
							const int Group = Column >= L.m_RedColumns ? 1 : 0;
							const auto Range = L.m_Ranges[Column];
							const auto Rows = CGroupRows::Calculate(C.m_Team[Group], Range);
							Seen += Range.m_End - Range.m_Start;
							EXPECT_NEAR(Rows.m_Units, L.m_Units[Column], .001f);
							EXPECT_NEAR(Rows.m_Units, Range.m_End - Range.m_Start + Rows.m_Count * (GroupHeaderRows + 2 * GroupPaddingRows), .001f);
							const auto Bounds = ColumnBounds(L, 0, 0, Column, Range.m_End - Range.m_Start);
							EXPECT_LE(Bounds.y + Bounds.h, L.m_Height + .001f);
							for(int I = Range.m_Start; I < Range.m_End; ++I)
							{
								EXPECT_LE((Rows.m_RowY[I] + 1) * L.m_Row, Rows.m_Units * L.m_Row + .001f);
								if(I + 1 < Range.m_End)
									EXPECT_GE(Rows.m_RowY[I + 1], Rows.m_RowY[I] + 1);
							}
							for(int I = 0; I < Rows.m_Count; ++I)
							{
								const auto &G = Rows.m_Groups[I];
								EXPECT_GT(G.m_Team, 0);
								for(int J = G.m_Start; J < G.m_End; ++J)
									EXPECT_EQ(C.m_Team[Group][J], G.m_Team);
							}
						}
						EXPECT_EQ(Seen, Count);
					}
}

TEST(LucentScoreboard, DDTeamSplitsIdentifyFragmentsAndNeverMergeDifferentIds)
{
	std::array<int, 128> Teams{};
	for(int I = 0; I < 16; ++I)
		Teams[I] = 1;
	const auto First = CGroupRows::Calculate(Teams, {0, 8}), Second = CGroupRows::Calculate(Teams, {8, 16});
	ASSERT_EQ(First.m_Count, 1);
	ASSERT_EQ(Second.m_Count, 1);
	EXPECT_EQ(First.m_Groups[0].m_Team, Second.m_Groups[0].m_Team);
	Teams[8] = 2;
	EXPECT_EQ(CGroupRows::Calculate(Teams, {0, 16}).m_Count, 3);
	Teams.fill(0);
	EXPECT_EQ(CGroupRows::Calculate(Teams, {0, 16}).m_Count, 0);
	Teams.fill(64); // Super is a distinct nonzero actual DDTeam id.
	EXPECT_EQ(CGroupRows::Calculate(Teams, {0, 16}).m_Groups[0].m_Team, 64);
	CContentWidths C;
	C.m_Independent = true;
	C.m_Team[0].fill(0);
	C.m_Team[0][0] = C.m_Team[0][1] = C.m_Team[0][2] = 1;
	// The balanced cut splits Team 1 and adds a second header on the right.
	C.m_Team[0][3] = 2;
	const auto R = C.Range(0, 4, 0, 2);
	EXPECT_EQ(R.m_End, 3);
}

TEST(LucentScoreboard, IndividualFinishStatesKeepTheirValuesAndUnknownsStayUnknown)
{
	const auto A = CRowScore::Read(EScoreMode::FINISH, 0, 83, 425);
	const auto B = CRowScore::Read(EScoreMode::FINISH, 0, FinishTime::NOT_FINISHED_MILLIS, 0);
	const auto C = CRowScore::Read(EScoreMode::FINISH, 0, FinishTime::UNSET, 0);
	EXPECT_TRUE(A.m_Known);
	EXPECT_EQ(A.m_Seconds, 83);
	EXPECT_EQ(A.m_Millis, 425);
	EXPECT_FALSE(B.m_Known);
	EXPECT_FALSE(C.m_Known);
	const auto Race = CRowScore::Read(EScoreMode::RACE7, 83425, 0, 0);
	EXPECT_EQ(Race.m_Seconds, A.m_Seconds);
	EXPECT_EQ(Race.m_Millis, A.m_Millis);
	EXPECT_FALSE(CRowScore::Read(EScoreMode::RACE7, protocol7::FinishTime::NOT_FINISHED, 0, 0).m_Known);
	EXPECT_FALSE(CRowScore::Read(EScoreMode::TIMESCORE, FinishTime::NOT_FINISHED_TIMESCORE, 0, 0).m_Known);
	EXPECT_TRUE(CRowScore::Read(EScoreMode::TIMESCORE, -83, 0, 0).m_Known);
	EXPECT_TRUE(CRowScore::Read(EScoreMode::POINTS, 0, 0, 0).m_Known);
}

TEST(LucentScoreboard, GroupMembershipChangesReflowWithoutDroppingRows)
{
	CPlanner P;
	CInput In{1066, 600, 16, 0, false, 0, 0, 0};
	auto &C = In.m_Content;
	C.m_Independent = true;
	C.m_Chrome = CFields::ChromeWidth(0, 48, 28);
	C.m_Points = 48;
	C.m_Ping = 28;
	for(int I = 0; I < 16; ++I)
		C.m_Identity[0][I] = 100;
	const auto Plain = P.Update(In);
	for(int I = 0; I < 16; ++I)
		C.m_Team[0][I] = I / 2 + 1;
	const auto Grouped = P.Update(In);
	float PlainUnits = 0, GroupedUnits = 0;
	for(int I = 0; I < Plain.m_Columns; ++I)
		PlainUnits += Plain.m_Units[I];
	for(int I = 0; I < Grouped.m_Columns; ++I)
		GroupedUnits += Grouped.m_Units[I];
	EXPECT_GT(GroupedUnits, PlainUnits);
	EXPECT_GE(Grouped.m_Columns, Plain.m_Columns);
	for(int NewCount : {17, 15, 32, 1, 128, 0})
	{
		In.m_Red = NewCount;
		for(int I = 0; I < NewCount; ++I)
		{
			C.m_Team[0][I] = I % 3 ? 64 : 0;
			C.m_Identity[0][I] = I % 2 ? 90 : 1200;
		}
		const auto L = P.Update(In);
		int Count = 0;
		for(int I = 0; I < L.m_Columns; ++I)
			Count += L.m_Ranges[I].m_End - L.m_Ranges[I].m_Start;
		EXPECT_EQ(Count, NewCount);
		EXPECT_LE(L.m_Height + L.m_SpectatorHeight, 480 + .001f);
	}
}

TEST(LucentScoreboard, VeryLongIdentityCannotStarveTheOtherColumnsNumericFields)
{
	CInput In{1066, 600, 16, 16, true, 0, 0, 0};
	auto &C = In.m_Content;
	C.m_Independent = true;
	C.m_Chrome = CFields::ChromeWidth(0, 48, 28);
	C.m_Points = 48;
	C.m_Ping = 28;
	C.m_Identity[0].fill(10000);
	C.m_Identity[1].fill(90);
	CPlanner P;
	const auto L = P.Update(In);
	for(int I = 0; I < L.m_Columns; ++I)
	{
		EXPECT_GE(L.ColumnWidth(I), CFields::ChromeWidth(L.m_Scores[I], 48, 28) + 72);
		const auto F = CFields::Calculate(L.ColumnWidth(I), 12, L.m_Scores[I], 48, 28, 0, 0);
		EXPECT_GE(F.m_IdentityWidth, 72 - .001f);
		EXPECT_LE(F.m_Country + F.m_CountryWidth, F.m_Points);
		EXPECT_LE(F.m_Points + F.m_PointsWidth, F.m_Ping);
	}
}

TEST(LucentScoreboard, SpectatorSurfaceCannotForceEmptyOrOversizedPlayerColumns)
{
	const auto Plain = Calculate(1066, 600, 1, 0, false, 0, 320, 300);
	const auto WithSpectators = Calculate(1066, 600, 1, 0, false, 127, 320, 300);
	EXPECT_EQ(WithSpectators.m_Columns, 1);
	EXPECT_FLOAT_EQ(WithSpectators.ColumnWidth(), Plain.ColumnWidth());
	EXPECT_GT(WithSpectators.m_SpectatorWidth, WithSpectators.m_Width);
	EXPECT_LE(WithSpectators.m_SpectatorWidth, 1066 - 32);
	EXPECT_GE(WithSpectators.m_SpectatorColumns * WithSpectators.m_SpectatorRows, 127);
	EXPECT_FLOAT_EQ(Plain.m_SpectatorWidth, 0);
}

TEST(LucentScoreboard, FourColumnsBalanceActualDDTeamHeightRatherThanPlayerCount)
{
	CContentWidths C;
	C.m_Independent = true;
	for(int I = 48; I < 64; ++I)
		C.m_Team[0][I] = I < 52 ? 4 : I < 56 ? 6 :
						       22;
	float BeforeMin = 1000, BeforeMax = 0, AfterMin = 1000, AfterMax = 0;
	const auto Partition = C.Partition(0, 64, 4);
	int Next = 0;
	for(int I = 0; I < 4; ++I)
	{
		const float Before = CGroupRows::Calculate(C.m_Team[0], {16 * I, 16 * (I + 1)}).m_Units;
		const float After = CGroupRows::Calculate(C.m_Team[0], Partition[I]).m_Units;
		BeforeMin = std::min(BeforeMin, Before);
		BeforeMax = std::max(BeforeMax, Before);
		AfterMin = std::min(AfterMin, After);
		AfterMax = std::max(AfterMax, After);
		EXPECT_EQ(Partition[I].m_Start, Next);
		EXPECT_GT(Partition[I].m_End, Next);
		Next = Partition[I].m_End;
	}
	EXPECT_EQ(Next, 64);
	EXPECT_LT(AfterMax, BeforeMax);
	EXPECT_LT(AfterMax - AfterMin, BeforeMax - BeforeMin);
	RecordProperty("before_units", std::to_string(BeforeMax - BeforeMin));
	RecordProperty("after_units", std::to_string(AfterMax - AfterMin));
	EXPECT_LE(AfterMax, 18.001f);
}

TEST(LucentScoreboard, HeightPartitionMatchesExhaustiveContiguousOptimum)
{
	for(int Bits = 0; Bits < 32; ++Bits)
		for(int Columns : {1, 2, 3, 4})
		{
			CContentWidths C;
			C.m_Independent = true;
			for(int I = 0; I < 8; ++I)
				C.m_Team[0][I] = Bits & (1 << (I % 5)) ? I / 2 + 1 : 0;
			float BestMax = 1000, BestSpread = 1000;
			const auto Search = [&](auto &&Self, int Start, int Left, float Max, float Min) -> void {
				if(Left == 0)
				{
					if(Start != 8)
						return;
					if(Max < BestMax - .0001f)
					{
						BestMax = Max;
						BestSpread = Max - Min;
					}
					else if(std::abs(Max - BestMax) < .0001f)
						BestSpread = std::min(BestSpread, Max - Min);
					return;
				}
				for(int End = Start + 1; End <= 8 - Left + 1; ++End)
				{
					const float H = CGroupRows::Calculate(C.m_Team[0], {Start, End}).m_Units;
					Self(Self, End, Left - 1, std::max(Max, H), std::min(Min, H));
				}
			};
			Search(Search, 0, Columns, 0, 1000);
			const auto P = C.Partition(0, 8, Columns);
			float Max = 0, Min = 1000;
			for(int I = 0; I < Columns; ++I)
			{
				const float H = CGroupRows::Calculate(C.m_Team[0], P[I]).m_Units;
				Max = std::max(Max, H);
				Min = std::min(Min, H);
			}
			EXPECT_NEAR(Max, BestMax, .001f);
			EXPECT_NEAR(Max - Min, BestSpread, .001f);
		}
}

TEST(LucentScoreboardText, ClanInkPaddingUsesProductionCacheAndSnappedCapsule)
{
	for(int Height : {720, 1080, 1440, 2160})
		for(float Mapping : {.98f, 1.f, 1.17f})
			for(float Font : {6.f, 9.f, 12.f})
				for(const char *pClan : {"Nephritis", "Nexusion", "雪", "ДлинныйUnicodeКлан雪雪雪雪雪雪雪", ""})
					for(float Available : {160.f, 500.f})
					{
						CIdentityTextRecorder R;
						R.m_PixelScale = Height / (600.f * Mapping);
						CCachedText N, C;
						const vec2 Metrics(R.m_PixelScale, R.m_PixelScale);
						const auto I = PrepareIdentityText(&R, N, C, "apolemia", pClan, Available, Font, 22, 0, Metrics);
						EXPECT_EQ(R.GetRenderFlags(), 0u);
						for(const auto &Submission : R.m_vSubmissions)
							if(Submission.m_Text == pClan && pClan[0])
							{
								EXPECT_TRUE(Submission.m_TrackInk);
								EXPECT_NE(Submission.m_RenderFlags & TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT, 0u);
							}
						if(!pClan[0])
						{
							EXPECT_FLOAT_EQ(I.m_ClanWidth + I.m_Gap, 0);
							continue;
						}
						const LucentHud::CPixelGrid Grid({-.37f, -.21f}, {1 / Metrics.x, 1 / Metrics.y});
						const auto P = CClanPillPlacement::Calculate(I, C.Ink(), 17.23f, N.Width(), 50, 22, Grid);
						if(!C.Ink().m_Valid)
							continue;
						const float Left = (P.m_Text.x + C.Ink().m_Left - P.m_Rect.x) * Metrics.x;
						const float Right = (P.m_Rect.x + P.m_Rect.w - P.m_Text.x - C.Ink().m_Right) * Metrics.x;
						EXPECT_NEAR(Left, Right, .001f);
						EXPECT_GE(Left, I.m_Padding * Metrics.x - .001f);
						EXPECT_LE(Left, I.m_Padding * Metrics.x + .501f);
						EXPECT_LE(P.m_Rect.x + P.m_Rect.w, 17.23f + Available + .001f);
						C.Render(&R, P.m_Text, ColorRGBA(1, 1, 1, 1));
						EXPECT_FLOAT_EQ(R.m_vPositions.back().x, P.m_Text.x);
					}
}

#if defined(CONF_SCOREBOARD_FREETYPE_TEST)
namespace
{
	// Actual shipped-font grayscale bitmaps, with the production cache and pill
	// submission path. This probes FreeType metrics without a GPU or GUI.
	struct CFontInkRecorder : CIdentityTextRecorder
	{
		FT_Face m_Face;
		explicit CFontInkRecorder(FT_Face Face) : m_Face(Face) {}
		void Layout(CTextCursor *pCursor, const char *pText) const
		{
			const int Size = std::max(1, int(pCursor->m_FontSize * m_PixelScale));
			FT_Set_Pixel_Sizes(m_Face, 0, Size);
			const float Scale = 1 / m_PixelScale;
			const int Padding = (Size > 48 ? 4 : Size >= 18 ? 2 :
									  1) +
					    1;
			float Draw = 0;
			const float Limit = pCursor->m_LineWidth > 0 ? pCursor->m_LineWidth : 1000000;
			for(const char *p = pText; *p;)
			{
				const int Code = str_utf8_decode(&p);
				if(Code < 0 || FT_Load_Char(m_Face, Code, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP))
					break;
				if(Draw + (m_Face->glyph->advance.x >> 6) * Scale > Limit)
				{
					pCursor->m_Truncated = true;
					break;
				}
				const auto &Bitmap = m_Face->glyph->bitmap;
				int First = int(Bitmap.width), Last = -1;
				for(unsigned Y = 0; Y < Bitmap.rows; ++Y)
					for(unsigned X = 0; X < Bitmap.width; ++X)
						if(Bitmap.buffer[Y * Bitmap.pitch + X])
						{
							First = std::min(First, int(X));
							Last = std::max(Last, int(X));
						}
				if(pCursor->m_TrackInk && Last >= First)
				{
					const float Bearing = float(m_Face->glyph->metrics.horiBearingX >> 6);
					pCursor->m_Ink.Include(Draw + (Bearing + Padding + First) * Scale, Draw + (Bearing + Padding + Last + 1) * Scale);
				}
				Draw += (m_Face->glyph->advance.x >> 6) * Scale;
			}
			pCursor->m_LongestLineWidth = Draw;
			pCursor->m_AlignedFontSize = Size * Scale;
		}
		void TextEx(CTextCursor *pCursor, const char *pText) override { Layout(pCursor, pText); }
		float TextWidth(float Font, const char *pText, int = -1, float Limit = -1, int = 0) const override
		{
			CTextCursor Cursor;
			Cursor.m_FontSize = Font;
			Cursor.m_LineWidth = Limit;
			Layout(&Cursor, pText);
			return Cursor.m_LongestLineWidth;
		}
	};
}

TEST(LucentScoreboardText, ShippedFreeTypeBitmapInkCentersInsideProductionPill)
{
	FT_Library Library;
	ASSERT_EQ(FT_Init_FreeType(&Library), 0);
	FT_Face Face;
	const auto FontPath = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "data/fonts/DejaVuSans.ttf";
	ASSERT_EQ(FT_New_Face(Library, FontPath.string().c_str(), 0, &Face), 0);
	for(int Height : {720, 1080, 1440, 2160})
		for(float Mapping : {.98f, 1.f, 1.17f})
			for(float Font : {6.f, 9.f, 12.f})
				for(const char *pClan : {"Nephritis", "Nexusion", "Клан", "ОченьДлинныйUnicodeКлан"})
				{
					CFontInkRecorder R(Face);
					R.m_PixelScale = Height / (600.f * Mapping);
					CCachedText Name, Clan;
					const auto I = PrepareIdentityText(&R, Name, Clan, "apolemia", pClan, 500, Font, 22, 0, vec2(R.m_PixelScale, R.m_PixelScale));
					ASSERT_TRUE(Clan.Ink().m_Valid);
					const LucentHud::CPixelGrid Grid({-.31f, -.17f}, {1 / R.m_PixelScale, 1 / R.m_PixelScale});
					const auto P = CClanPillPlacement::Calculate(I, Clan.Ink(), 17.23f, Name.Width(), 50, 22, Grid);
					const float Left = (P.m_Text.x + Clan.Ink().m_Left - P.m_Rect.x) * R.m_PixelScale;
					const float Right = (P.m_Rect.x + P.m_Rect.w - P.m_Text.x - Clan.Ink().m_Right) * R.m_PixelScale;
					EXPECT_NEAR(Left, Right, .001f);
					EXPECT_GE(Left, I.m_Padding * R.m_PixelScale - .001f);
					for(const auto &S : R.m_vSubmissions)
						EXPECT_FALSE(S.m_Truncated);
					Clan.Render(&R, P.m_Text, ColorRGBA(1, 1, 1, 1));
					EXPECT_FLOAT_EQ(R.m_vPositions.back().x, P.m_Text.x);
				}
	FT_Done_Face(Face);
	FT_Done_FreeType(Library);
}
#endif

TEST(LucentScoreboard, BalancedPartitionsPreserveOrderAndRetargetPanelHeights)
{
	for(int Count : {16, 32, 64, 128})
		for(int Columns : {1, 2, 3, 4})
			for(int TeamSize : {1, 2, 4, 16})
			{
				CContentWidths C;
				C.m_Independent = true;
				for(int Group = 0; Group < 2; ++Group)
				{
					for(int I = 0; I < Count / 2; ++I)
						C.m_Team[Group][I] = I % 5 ? I / TeamSize + 1 : 0;
					const auto Ranges = C.Partition(Group, Count / 2, Columns);
					int Seen = 0;
					for(int I = 0; I < Columns; ++I)
					{
						EXPECT_EQ(Ranges[I].m_Start, Seen);
						EXPECT_GT(Ranges[I].m_End, Seen);
						Seen = Ranges[I].m_End;
						const float Units = CGroupRows::Calculate(C.m_Team[Group], Ranges[I]).m_Units;
						CPanelMotion Motion;
						const LucentHud::CRect Target{20, 80, 280, ColumnHeadlines + ColumnBottom + Units * 15};
						Motion.Update(Target, 0, true);
						const auto Before = Motion.Update(Target, .02, true);
						const LucentHud::CRect Changed{20, 80, 300, Target.h + 22};
						const auto After = Motion.Update(Changed, .02, true);
						EXPECT_FLOAT_EQ(Before.h, After.h);
						Motion.Update(Target, .03, true);
						const auto Final = Motion.Update(Target, .5, true);
						EXPECT_FLOAT_EQ(Final.h, Target.h);
					}
					EXPECT_EQ(Seen, Count / 2);
				}
			}
}

TEST(LucentScoreboard, ProductionPlannerDoesNotCreateEmptyGameTeamColumns)
{
	for(int Red : {0, 1, 16})
		for(int Blue : {0, 1, 16})
		{
			CInput In{1066, 600, Red, Blue, true, 0, 0, 0};
			In.m_Content.m_Independent = true;
			In.m_Content.m_Chrome = CFields::ChromeWidth(0, 48, 28);
			In.m_Content.m_Points = 48;
			In.m_Content.m_Ping = 28;
			CPlanner Planner;
			const auto L = Planner.Update(In);
			if(Red + Blue)
				for(int I = 0; I < L.m_Columns; ++I)
					EXPECT_GT(L.m_Ranges[I].m_End, L.m_Ranges[I].m_Start);
			if(!Blue)
				EXPECT_EQ(L.m_BlueColumns, 0);
			if(!Red && Blue)
				EXPECT_EQ(L.m_RedColumns, 0);
		}
}
