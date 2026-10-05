/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <set>
#include <thread>
#include "DonorHealth.h"

using Answer = DonorHealth::Answer;
using Health = DonorHealth::Health;

namespace
{

// a news server in the test process: which articles it has, how it answers
// for the others, how long a command takes, and what it counted
class FakeServer : public DonorHealth::Server
{
public:
	explicit FakeServer(int cap = 1) : Server(cap) {}

	std::set<std::string> has;
	bool hasAll = false;
	bool answers451 = false;		// "451" instead of "430" for an article it hasn't
	bool bodyGone = false;			// STAT says yes, the body is gone
	bool authFails = false;			// every exchange fails (bad credentials)
	int failFirst = 0;				// the first exchanges fail
	int delayMs = 0;				// per command
	bool dropOnFirstBody = false;	// the connection drops during the first BODY

	std::atomic<int> stats{0};
	std::atomic<int> bodies{0};
	std::atomic<int> pipelined{0};	// STATs sent in a batch of more than one
	std::atomic<int> sessions{0};
	std::atomic<int> exchanges{0};

protected:
	std::vector<Answer> Exchange(const std::vector<DonorHealth::Request>& batch) override
	{
		bool broken = authFails || exchanges++ < failFirst;
		if (broken || !sessions)
		{
			// a connection (re)opened
			sessions++;
		}
		if (broken)
		{
			throw DonorHealth::ExchangeError();
		}

		if (delayMs)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(delayMs * (int)batch.size()));
		}
		stats += (int)batch.size();
		if (batch.size() > 1)
		{
			pipelined += (int)batch.size();
		}

		std::vector<Answer> out;
		for (const DonorHealth::Request& request : batch)
		{
			bool present = hasAll || has.count(request.messageId);
			out.push_back(present ? Answer::Present : answers451 ? Answer::Error : Answer::Missing);
		}

		for (size_t i = 0; i < batch.size(); i++)
		{
			if (!batch[i].gate || out[i] != Answer::Present)
			{
				continue;
			}
			if (dropOnFirstBody && !bodies)
			{
				bodies++;
				sessions++;
				DonorHealth::ExchangeError error;
				error.answers = out;
				error.answers[i] = Answer::Error;
				throw error;
			}
			out[i] = BodyOnce(batch[i].gate, [&]()
				{
					bodies++;
					if (delayMs)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
					}
					return bodyGone ? Answer::BodyBad : Answer::Present;
				});
		}
		return out;
	}
};

std::vector<std::string> Ids(const char* prefix, int count)
{
	std::vector<std::string> ids;
	for (int i = 0; i < count; i++)
	{
		ids.push_back(std::string(prefix) + std::to_string(i) + "@x");
	}
	return ids;
}

std::shared_ptr<std::vector<std::string>> NoGroups()
{
	return std::make_shared<std::vector<std::string>>();
}

DonorHealth::Options Everything(int bodyPercent = 0)
{
	DonorHealth::Options options;
	options.sample.percent = 100;
	options.sample.minimum = 1;
	options.sample.maximum = 100000;
	options.sample.bodyPercent = bodyPercent;
	options.sample.maxBody = 1000;
	options.probe = 10;
	options.budgetMs = 10000;
	return options;
}

DonorHealth::ServerList List(std::initializer_list<std::shared_ptr<FakeServer>> servers)
{
	DonorHealth::ServerList list;
	for (const auto& server : servers)
	{
		list.push_back(server);
	}
	return list;
}

}

BOOST_AUTO_TEST_SUITE(DupeSearchTest)

BOOST_AUTO_TEST_CASE(DonorHealthSampleTest)
{
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 100), 2, 20, 300, 0).size(), 20U);
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 5000), 2, 20, 300, 0).size(), 100U);
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 100000), 2, 20, 300, 0).size(), 300U);
	// the limits of the options: 50 to 1000
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 20000), 5, 50, 1000, 0).size(), 1000U);
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 300), 5, 50, 1000, 0).size(), 50U);
	// never more than there are
	BOOST_CHECK_EQUAL(DonorHealth::Sample(Ids("a", 12), 5, 50, 1000, 0).size(), 12U);

	// deterministic, distinct, and from the ids
	std::vector<std::string> a = DonorHealth::Sample(Ids("a", 5000), 2, 20, 300, 7);
	BOOST_CHECK(a == DonorHealth::Sample(Ids("a", 5000), 2, 20, 300, 7));
	BOOST_CHECK(a != DonorHealth::Sample(Ids("a", 5000), 2, 20, 300, 8));
	BOOST_CHECK_EQUAL(std::set<std::string>(a.begin(), a.end()).size(), a.size());
}

BOOST_AUTO_TEST_CASE(DonorHealthPlanBodyTest)
{
	DonorHealth::SampleOptions options;
	options.percent = 30;
	options.minimum = 20;
	options.maximum = 300;
	options.bodyPercent = 20;
	options.maxBody = 5;
	std::vector<DonorHealth::Item> items = DonorHealth::Plan(Ids("a", 1000), NoGroups(), options);
	BOOST_CHECK_EQUAL(items.size(), 300U);
	// about a fifth of the sample would get a body check; the cap is exact
	BOOST_CHECK_EQUAL(std::count_if(items.begin(), items.end(), [](const DonorHealth::Item& item) { return item.body; }), 5);

	options.maxBody = 1000;
	items = DonorHealth::Plan(Ids("a", 1000), NoGroups(), options);
	long bodies = std::count_if(items.begin(), items.end(), [](const DonorHealth::Item& item) { return item.body; });
	BOOST_CHECK(bodies > 30 && bodies < 90);	// 20% of 300
}

BOOST_AUTO_TEST_CASE(DonorHealthVerdictTest)
{
	auto health = [](int checked, int present, int missing, int error)
	{
		Health h;
		h.checked = checked;
		h.present = present;
		h.missing = missing;
		h.error = error;
		return h;
	};
	// the share present of all answered, errors counting as not present
	BOOST_CHECK_EQUAL(health(300, 0, 1, 299).Alive(), 0.0);
	BOOST_CHECK_EQUAL(health(300, 150, 0, 150).Alive(), 0.5);
	BOOST_CHECK_EQUAL(health(300, 10, 0, 0).Alive(), 1.0);
	// fewer than 5 answered: no verdict
	BOOST_CHECK_EQUAL(health(300, 0, 2, 2).Alive(), -1.0);

	// a probe proves a posting dead with nothing found and at least 5 definitive misses
	BOOST_CHECK(DonorHealth::DeadProbe(health(10, 0, 5, 5)));
	BOOST_CHECK(DonorHealth::DeadProbe(health(10, 0, 10, 0)));
	BOOST_CHECK(!DonorHealth::DeadProbe(health(10, 0, 4, 6)));
	BOOST_CHECK(!DonorHealth::DeadProbe(health(10, 1, 9, 0)));
	// an outage: every answer an error
	BOOST_CHECK(!DonorHealth::DeadProbe(health(10, 0, 0, 10)));
}

BOOST_AUTO_TEST_CASE(DonorHealthAllServersTest)
{
	// the articles split between two servers: all found
	std::vector<std::string> ids = Ids("a", 40);
	auto a = std::make_shared<FakeServer>();
	auto b = std::make_shared<FakeServer>();
	for (size_t i = 0; i < ids.size(); i++)
	{
		(i < 20 ? a : b)->has.insert(ids[i]);
	}
	Health h = DonorHealth::CheckItems(List({ a, b }), DonorHealth::Plan(ids, NoGroups(), Everything().sample), 5000);
	BOOST_CHECK_EQUAL(h.checked, 40);
	BOOST_CHECK_EQUAL(h.present, 40);
}

BOOST_AUTO_TEST_CASE(DonorHealthDeadProbeAsksEveryServerTest)
{
	// 30 dead articles, probe only: 10 are checked, and missing needs every server
	auto a = std::make_shared<FakeServer>();
	auto b = std::make_shared<FakeServer>();
	Health h = DonorHealth::CheckPosting(List({ a, b }), Ids("dead", 30), NoGroups(), Everything(), false);
	BOOST_CHECK_EQUAL(h.checked, 10);
	BOOST_CHECK_EQUAL(h.missing, 10);
	BOOST_CHECK_EQUAL(h.present, 0);
	BOOST_CHECK_EQUAL(a->stats.load(), 10);
	BOOST_CHECK_EQUAL(b->stats.load(), 10);
	BOOST_CHECK(DonorHealth::DeadProbe(h));
}

BOOST_AUTO_TEST_CASE(DonorHealthSlowServerNeverHoldsUpTheOthersTest)
{
	std::vector<std::string> ids = Ids("a", 20);
	auto fast = std::make_shared<FakeServer>();
	fast->hasAll = true;
	auto slow = std::make_shared<FakeServer>();
	slow->hasAll = true;
	slow->delayMs = 500;

	long long start = DonorHealth::NowMs();
	Health h = DonorHealth::CheckItems(List({ fast, slow }), DonorHealth::Plan(ids, NoGroups(), Everything().sample), 10000);
	BOOST_CHECK_EQUAL(h.present, 20);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 3000);
	// the slow server skips what was found meanwhile
	BOOST_CHECK_LT(slow->stats.load(), 20);
}

BOOST_AUTO_TEST_CASE(DonorHealthBadAuthServerTest)
{
	// a server that can't log in, and a good one: the misses stand, no errors
	auto bad = std::make_shared<FakeServer>();
	bad->authFails = true;
	auto good = std::make_shared<FakeServer>();
	Health h = DonorHealth::CheckItems(List({ bad, good }), DonorHealth::Plan(Ids("dead", 20), NoGroups(), Everything().sample), 5000);
	BOOST_CHECK_EQUAL(h.missing, 20);
	BOOST_CHECK_EQUAL(h.error, 0);

	// every server failing: errors, never missing
	auto bad2 = std::make_shared<FakeServer>();
	bad2->authFails = true;
	h = DonorHealth::CheckItems(List({ bad2 }), DonorHealth::Plan(Ids("a", 20), NoGroups(), Everything().sample), 5000);
	BOOST_CHECK_EQUAL(h.missing, 0);
	BOOST_CHECK_EQUAL(h.present, 0);
	BOOST_CHECK(!DonorHealth::DeadProbe(h));
}

BOOST_AUTO_TEST_CASE(DonorHealth451IsAnAnswerTest)
{
	// one server answers 451 for a missing article, the other 430: the misses are
	// definitive, the 451 server keeps its connection and is never paused
	auto a = std::make_shared<FakeServer>();
	a->answers451 = true;
	auto b = std::make_shared<FakeServer>();
	a->SetRetryAfterMs(30000);

	long long start = DonorHealth::NowMs();
	Health h = DonorHealth::CheckItems(List({ a, b }), DonorHealth::Plan(Ids("dead", 20), NoGroups(), Everything().sample), 10000);
	BOOST_CHECK_EQUAL(h.missing, 20);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 3000);
	BOOST_CHECK_EQUAL(a->sessions.load(), 1);
	BOOST_CHECK_EQUAL(a->stats.load(), 20);

	// only 451 answers: no definitive miss, so nothing is missing and the posting isn't dead
	auto c = std::make_shared<FakeServer>();
	c->answers451 = true;
	h = DonorHealth::CheckItems(List({ c }), DonorHealth::Plan(Ids("dead", 20), NoGroups(), Everything().sample), 5000);
	BOOST_CHECK_EQUAL(h.missing, 0);
	BOOST_CHECK_EQUAL(h.error, 20);
	BOOST_CHECK(!DonorHealth::DeadProbe(h));
}

BOOST_AUTO_TEST_CASE(DonorHealthPausedServerAbstainsTest)
{
	// a server failing in a row pauses (30 s) and abstains while another can answer
	auto bad = std::make_shared<FakeServer>();
	bad->authFails = true;
	bad->SetRetryAfterMs(30000);
	auto good = std::make_shared<FakeServer>();
	good->delayMs = 20;

	long long start = DonorHealth::NowMs();
	Health h = DonorHealth::CheckItems(List({ bad, good }), DonorHealth::Plan(Ids("dead", 40), NoGroups(), Everything().sample), 10000);
	BOOST_CHECK_EQUAL(h.missing, 40);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 5000);
}

// B5: a paused server's request ends at once, asking nothing, when its check is over
// or nzbget shuts down; shutdown waits for every walker of a finished check
BOOST_AUTO_TEST_CASE(DonorHealthPausedAskIsInterruptibleTest)
{
	auto paused = std::make_shared<FakeServer>();
	paused->authFails = true;
	paused->SetRetryAfterMs(30000);
	std::vector<DonorHealth::Request> batch(1);
	batch[0].messageId = "a@x";
	for (int i = 0; i < DonorHealth::ServerGiveUp; i++)
	{
		paused->Ask(batch);		// three errors in a row: a 30 s pause
	}
	BOOST_REQUIRE(paused->Paused());
	int before = paused->exchanges;

	long long start = DonorHealth::NowMs();
	std::vector<DonorHealth::Answer> answers = paused->Ask(batch, []() { return true; });
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 1000);
	BOOST_REQUIRE_EQUAL(answers.size(), 1u);
	BOOST_CHECK(answers[0] == DonorHealth::Answer::Error);
	BOOST_CHECK_EQUAL(paused->exchanges, before);

	DonorHealth::StopAll();
	start = DonorHealth::NowMs();
	paused->Ask(batch);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 1000);
	BOOST_CHECK_EQUAL(paused->exchanges, before);
	DonorHealth::Reset();

	// a check whose only server is paused ends by its budget; its walker, waiting out
	// the pause, ends with it and WaitAll doesn't hang
	auto good = std::make_shared<FakeServer>();
	good->hasAll = true;
	DonorHealth::CheckItems(List({ paused, good }), DonorHealth::Plan(Ids("w", 20), NoGroups(), Everything().sample), 200);
	start = DonorHealth::NowMs();
	DonorHealth::StopAll();
	DonorHealth::WaitAll();
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 2000);
	DonorHealth::Reset();
}

BOOST_AUTO_TEST_CASE(DonorHealthSingleServerRecoversTest)
{
	// the only server fails three times, pauses briefly and recovers
	auto server = std::make_shared<FakeServer>();
	server->hasAll = true;
	server->failFirst = 3;
	server->SetRetryAfterMs(50);
	Health h = DonorHealth::CheckItems(List({ server }), DonorHealth::Plan(Ids("a", 60), NoGroups(), Everything().sample), 10000);
	// the articles of the failed batches are lost (errors), the rest are found
	BOOST_CHECK_GE(h.present, 60 - 3 * 4);
	BOOST_CHECK_EQUAL(h.present + h.error, 60);
}

BOOST_AUTO_TEST_CASE(DonorHealthBudgetEndBiasTest)
{
	// a has 10 of 20, b none, and c is slow: when the budget ends, an unsettled
	// article that two of three servers missed counts as missing, so the share
	// alive isn't inflated by the slow answers that never came
	std::vector<std::string> ids = Ids("a", 20);
	auto a = std::make_shared<FakeServer>();
	auto b = std::make_shared<FakeServer>();
	auto c = std::make_shared<FakeServer>();
	for (int i = 0; i < 10; i++)
	{
		a->has.insert(ids[i]);
	}
	c->hasAll = true;
	c->delayMs = 3000;

	long long start = DonorHealth::NowMs();
	Health h = DonorHealth::CheckItems(List({ a, b, c }), DonorHealth::Plan(ids, NoGroups(), Everything().sample), 1000);
	BOOST_CHECK_EQUAL(h.present, 10);
	BOOST_CHECK_EQUAL(h.missing, 10);
	BOOST_CHECK_EQUAL(h.Alive(), 0.5);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 3000);
}

BOOST_AUTO_TEST_CASE(DonorHealthSlowServerBudgetTest)
{
	// one slow server: the check ends with its budget, unanswered articles are left out
	auto slow = std::make_shared<FakeServer>();
	slow->hasAll = true;
	slow->delayMs = 1000;
	long long start = DonorHealth::NowMs();
	Health h = DonorHealth::CheckItems(List({ slow }), DonorHealth::Plan(Ids("a", 50), NoGroups(), Everything().sample), 1000);
	BOOST_CHECK_LT(DonorHealth::NowMs() - start, 3000);
	BOOST_CHECK_LT(h.Answered(), 50);
	BOOST_CHECK_EQUAL(h.checked, 50);
}

BOOST_AUTO_TEST_CASE(DonorHealthBodyChecksTest)
{
	std::vector<std::string> ids = Ids("a", 30);

	// STAT says yes but the data is gone: only a body check sees it
	auto soft = std::make_shared<FakeServer>();
	soft->hasAll = true;
	soft->bodyGone = true;
	Health h = DonorHealth::CheckItems(List({ soft }), DonorHealth::Plan(ids, NoGroups(), Everything(0).sample), 5000);
	BOOST_CHECK_EQUAL(h.Alive(), 1.0);
	h = DonorHealth::CheckItems(List({ soft }), DonorHealth::Plan(ids, NoGroups(), Everything(100).sample), 5000);
	BOOST_CHECK_EQUAL(h.Alive(), 0.0);
	BOOST_CHECK_EQUAL(h.bodyChecked, 30);
	BOOST_CHECK_EQUAL(h.bodyBad, 30);

	// a soft-dead server and a good one: the good one delivers every body
	auto soft2 = std::make_shared<FakeServer>();
	soft2->hasAll = true;
	soft2->bodyGone = true;
	auto good = std::make_shared<FakeServer>();
	good->hasAll = true;
	h = DonorHealth::CheckItems(List({ soft2, good }), DonorHealth::Plan(ids, NoGroups(), Everything(100).sample), 5000);
	BOOST_CHECK_EQUAL(h.Alive(), 1.0);
	BOOST_CHECK_EQUAL(good->bodies.load(), 30);
}

BOOST_AUTO_TEST_CASE(DonorHealthOneServerPerBodyTest)
{
	// three servers with every article, a body check for all 10: servers take
	// turns, 10 bodies are downloaded in total
	auto a = std::make_shared<FakeServer>();
	auto b = std::make_shared<FakeServer>();
	auto c = std::make_shared<FakeServer>();
	for (auto server : { a, b, c })
	{
		server->hasAll = true;
	}
	Health h = DonorHealth::CheckItems(List({ a, b, c }), DonorHealth::Plan(Ids("a", 10), NoGroups(), Everything(100).sample), 5000);
	BOOST_CHECK_EQUAL(h.present, 10);
	BOOST_CHECK_EQUAL(a->bodies + b->bodies + c->bodies, 10);
}

BOOST_AUTO_TEST_CASE(DonorHealthConnectionDropsDuringBodyTest)
{
	// the connection breaks during the first BODY of a batch: the STAT answers
	// already read stay, and the check goes on
	auto server = std::make_shared<FakeServer>();
	server->hasAll = true;
	server->dropOnFirstBody = true;
	Health h = DonorHealth::CheckItems(List({ server }), DonorHealth::Plan(Ids("a", 40), NoGroups(), Everything(100).sample), 10000);
	BOOST_CHECK_GE(h.present, 40 - 4 * 2);
	BOOST_CHECK_EQUAL(h.present + h.error, 40);
}

BOOST_AUTO_TEST_CASE(DonorHealthPipeliningTest)
{
	// 64 articles over one connection: STATs go out in batches that deepen
	// while the server answers fast (4, 8, 16, ...)
	auto server = std::make_shared<FakeServer>();
	server->hasAll = true;
	Health h = DonorHealth::CheckItems(List({ server }), DonorHealth::Plan(Ids("a", 64), NoGroups(), Everything().sample), 5000);
	BOOST_CHECK_EQUAL(h.present, 64);
	BOOST_CHECK_EQUAL(server->sessions.load(), 1);
	BOOST_CHECK_GE(server->pipelined.load(), 32);
	BOOST_CHECK_EQUAL(server->Depth(), DonorHealth::PipelineMax);
}

BOOST_AUTO_TEST_CASE(DonorHealthConnectionCapTest)
{
	// 30 postings of 20 articles over a server that allows 4 connections: the
	// cap holds, however many checks run
	auto server = std::make_shared<FakeServer>(4);
	server->hasAll = true;
	server->delayMs = 5;

	std::vector<DonorHealth::Posting> postings;
	for (int i = 0; i < 30; i++)
	{
		DonorHealth::Posting posting;
		posting.key = "p" + std::to_string(i);
		posting.ids = Ids(("p" + std::to_string(i) + "-").c_str(), 20);
		posting.groups = NoGroups();
		postings.push_back(std::move(posting));
	}
	std::atomic<int> done{0};
	std::atomic<int> alive{0};
	DonorHealth::CheckPostings(List({ server }), postings, Everything(), true, 10,
		[&](const std::string&, const Health& h) { done++; alive += h.present == 20; });
	BOOST_CHECK_EQUAL(done.load(), 30);
	BOOST_CHECK_EQUAL(alive.load(), 30);
	BOOST_CHECK_GE(server->MaxActive(), 2);
	BOOST_CHECK_LE(server->MaxActive(), 4);
}

BOOST_AUTO_TEST_CASE(DonorHealthProbeEndsDeadPostingTest)
{
	// nothing found and enough definitive misses: the rest of the sample isn't checked
	auto server = std::make_shared<FakeServer>();
	Health h = DonorHealth::CheckPosting(List({ server }), Ids("dead", 500), NoGroups(), Everything(), true);
	BOOST_CHECK_EQUAL(h.checked, 10);
	BOOST_CHECK(DonorHealth::DeadProbe(h));
	BOOST_CHECK_EQUAL(server->stats.load(), 10);

	// a live posting gets its full sample
	auto live = std::make_shared<FakeServer>();
	live->hasAll = true;
	h = DonorHealth::CheckPosting(List({ live }), Ids("a", 500), NoGroups(), Everything(), true);
	BOOST_CHECK_EQUAL(h.checked, 500);
	BOOST_CHECK_EQUAL(h.present, 500);
}

BOOST_AUTO_TEST_SUITE_END()
