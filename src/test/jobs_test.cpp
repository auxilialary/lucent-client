#include "test.h"

#include <base/sphore.h>
#include <base/thread.h>

#include <engine/shared/host_lookup.h>
#include <engine/shared/jobs.h>

#include <gtest/gtest.h>

#include <functional>

static const int TEST_NUM_THREADS = 4;

class Jobs : public ::testing::Test // NOLINT(readability-identifier-naming)
{
protected:
	CJobPool m_Pool;

	void SetUp() override
	{
		m_Pool.Init(TEST_NUM_THREADS);
	}

	void TearDown() override
	{
		m_Pool.Shutdown();
	}

	void Add(std::shared_ptr<IJob> pJob)
	{
		m_Pool.Add(std::move(pJob));
	}
};

class CJob : public IJob
{
	std::function<void()> m_JobFunction;
	void Run() override { m_JobFunction(); }

public:
	CJob(std::function<void()> &&JobFunction) :
		m_JobFunction(JobFunction) {}

	void Abortable(bool Abortable)
	{
		IJob::Abortable(Abortable);
	}
};

TEST_F(Jobs, Constructor)
{
}

TEST_F(Jobs, Simple)
{
	Add(std::make_shared<CJob>([] {}));
}

TEST_F(Jobs, Wait)
{
	SEMAPHORE Sphore;
	sphore_init(&Sphore);
	Add(std::make_shared<CJob>([&] { sphore_signal(&Sphore); }));
	sphore_wait(&Sphore);
	sphore_destroy(&Sphore);
}

TEST_F(Jobs, AbortAbortable)
{
	auto pJob = std::make_shared<CJob>([&] {});
	pJob->Abortable(true);
	EXPECT_TRUE(pJob->IsAbortable());
	Add(pJob);
	EXPECT_TRUE(pJob->Abort());
	EXPECT_EQ(pJob->State(), IJob::STATE_ABORTED);
}

TEST_F(Jobs, AbortUnabortable)
{
	auto pJob = std::make_shared<CJob>([&] {});
	pJob->Abortable(false);
	EXPECT_FALSE(pJob->IsAbortable());
	Add(pJob);
	EXPECT_FALSE(pJob->Abort());
	EXPECT_NE(pJob->State(), IJob::STATE_ABORTED);
}

TEST_F(Jobs, LookupHost)
{
	static const char *const HOST = "example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);

	Add(pJob);
	while(pJob->State() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);
	if(pJob->Result() == 0)
	{
		EXPECT_EQ(pJob->Addr().type & NETTYPE, pJob->Addr().type);
	}
}

TEST_F(Jobs, LookupHostWebsocket)
{
	static const char *const HOST = "ws://example.com";
	static const int NETTYPE = NETTYPE_ALL;
	auto pJob = std::make_shared<CHostLookup>(HOST, NETTYPE);

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);

	Add(pJob);
	while(pJob->State() != IJob::STATE_DONE)
	{
		// yay, busy loop...
		thread_yield();
	}

	EXPECT_STREQ(pJob->Hostname(), HOST);
	EXPECT_EQ(pJob->Nettype(), NETTYPE);
	if(pJob->Result() == 0)
	{
		EXPECT_EQ(pJob->Addr().type & (NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6), pJob->Addr().type);
	}
}

TEST_F(Jobs, Many)
{
	std::atomic<int> ThreadsRunning(0);
	std::vector<std::shared_ptr<IJob>> vpJobs;
	SEMAPHORE Sphore;
	sphore_init(&Sphore);
	for(int i = 0; i < TEST_NUM_THREADS; i++)
	{
		std::shared_ptr<IJob> pJob = std::make_shared<CJob>([&] {
			int Prev = ThreadsRunning.fetch_add(1);
			if(Prev == TEST_NUM_THREADS - 1)
			{
				sphore_signal(&Sphore);
			}
		});
		EXPECT_EQ(pJob->State(), IJob::STATE_QUEUED);
		vpJobs.push_back(pJob);
	}
	for(auto &pJob : vpJobs)
	{
		Add(pJob);
	}
	sphore_wait(&Sphore);
	sphore_destroy(&Sphore);
	TearDown();
	for(auto &pJob : vpJobs)
	{
		EXPECT_EQ(pJob->State(), IJob::STATE_DONE);
	}
	SetUp();
}

class CDelayedHostLookup : public CHostLookup
{
	SEMAPHORE &m_Started;
	SEMAPHORE &m_Continue;
	void Run() override
	{
		sphore_signal(&m_Started);
		sphore_wait(&m_Continue);
		CHostLookup::Run();
	}
public:
	CDelayedHostLookup(SEMAPHORE &Started, SEMAPHORE &Continue) :
		CHostLookup("127.0.0.1:8303", NETTYPE_IPV4), m_Started(Started), m_Continue(Continue) {}
};

TEST_F(Jobs, CancelDelayedLookupAndReplace)
{
	SEMAPHORE Started, Continue;
	sphore_init(&Started);
	sphore_init(&Continue);
	auto pOld = std::make_shared<CDelayedHostLookup>(Started, Continue);
	std::weak_ptr<CHostLookup> OldLifetime = pOld;
	Add(pOld);
	sphore_wait(&Started);
	EXPECT_TRUE(pOld->Abort());
	EXPECT_EQ(pOld->State(), IJob::STATE_ABORTED);
	pOld.reset();
	// The pool keeps the old resolver alive, but the replacement can finish
	// independently. No result from an aborted resolver may be read.
	EXPECT_FALSE(OldLifetime.expired());
	auto pCurrent = std::make_shared<CHostLookup>("127.0.0.2:8304", NETTYPE_IPV4);
	Add(pCurrent);
	while(!pCurrent->Done())
		thread_yield();
	EXPECT_EQ(pCurrent->State(), IJob::STATE_DONE);
	EXPECT_EQ(pCurrent->Result(), 0);
	EXPECT_EQ(pCurrent->Addr().ip[3], 2);
	EXPECT_EQ(pCurrent->Addr().port, 8304);
	sphore_signal(&Continue);
	TearDown();
	EXPECT_TRUE(OldLifetime.expired());
	sphore_destroy(&Started);
	sphore_destroy(&Continue);
	SetUp();
}
