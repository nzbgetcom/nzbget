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

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <thread>
#include "DonorHealth.h"

using Answer = DonorHealth::Answer;
using Health = DonorHealth::Health;

long long DonorHealth::NowMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

double DonorHealth::Health::Alive() const
{
	return Answered() >= MinKnown ? (double)present / (double)Answered() : -1.0;
}

DonorHealth::Health& DonorHealth::Health::operator+=(const Health& other)
{
	checked += other.checked;
	present += other.present;
	missing += other.missing;
	error += other.error;
	bodyChecked += other.bodyChecked;
	bodyBad += other.bodyBad;
	return *this;
}

bool DonorHealth::DeadProbe(const Health& health)
{
	return health.present == 0 && health.missing >= MinKnown;
}

std::vector<std::string> DonorHealth::Sample(const std::vector<std::string>& ids, int percent, int minimum,
	int maximum, unsigned seed)
{
	std::vector<std::string> sorted = ids;
	std::sort(sorted.begin(), sorted.end());

	size_t wanted = (size_t)std::ceil((double)sorted.size() * percent / 100.0);
	size_t count = std::min(sorted.size(), std::max((size_t)std::max(minimum, 0),
		std::min((size_t)std::max(maximum, 0), wanted)));

	// a deterministic random pick: a partial shuffle
	std::mt19937 random(seed);
	for (size_t i = 0; i < count; i++)
	{
		size_t j = i + random() % (sorted.size() - i);
		std::swap(sorted[i], sorted[j]);
	}
	sorted.resize(count);
	return sorted;
}

std::vector<DonorHealth::Item> DonorHealth::Plan(const std::vector<std::string>& ids,
	const std::shared_ptr<std::vector<std::string>>& groups, const SampleOptions& options)
{
	std::mt19937 random(options.seed + 1);
	std::uniform_real_distribution<double> percent(0.0, 100.0);
	int bodies = 0;
	std::vector<Item> items;
	for (const std::string& id : Sample(ids, options.percent, options.minimum, options.maximum, options.seed))
	{
		Item item;
		item.messageId = id;
		item.groups = groups;
		item.body = bodies < options.maxBody && percent(random) < options.bodyPercent;
		bodies += item.body;
		items.push_back(std::move(item));
	}
	return items;
}

bool DonorHealth::Server::Paused() const
{
	return m_downUntilMs > NowMs();
}

DonorHealth::Answer DonorHealth::Server::BodyOnce(BodyGate* gate, const std::function<Answer()>& download)
{
	std::lock_guard<std::mutex> guard(gate->lock);
	if (gate->Done())
	{
		// another server delivered valid data meanwhile
		return Answer::Present;
	}
	Answer answer = download();
	gate->ok = answer == Answer::Present;
	return answer;
}

namespace
{
	std::atomic<bool> g_healthStopping{false};
	std::atomic<int> g_walkers{0};
}

void DonorHealth::StopAll() { g_healthStopping = true; }
void DonorHealth::Reset() { g_healthStopping = false; }
bool DonorHealth::Stopping() { return g_healthStopping; }

void DonorHealth::WaitAll()
{
	while (g_walkers > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

std::vector<DonorHealth::Answer> DonorHealth::Server::Ask(const std::vector<Request>& batch,
	const std::function<bool()>& cancelled)
{
	auto over = [&cancelled]() { return g_healthStopping || (cancelled && cancelled()); };

	// a server that kept failing gets a pause, then another try
	while (m_downUntilMs - NowMs() > 0 && !over())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(
			std::min<long long>(50, m_downUntilMs - NowMs())));
	}

	{
		std::unique_lock<std::mutex> lock(m_slotMutex);
		while (m_active >= m_cap && !over())
		{
			m_slotCond.wait_for(lock, std::chrono::milliseconds(50));
		}
		if (over())
		{
			// nothing asked: the check is over, or nzbget shuts down
			return std::vector<Answer>(batch.size(), Answer::Error);
		}
		m_active++;
		if (m_active > m_maxActive)
		{
			m_maxActive = m_active;
		}
	}

	std::vector<Answer> answers;
	long long start = NowMs();
	try
	{
		answers = Exchange(batch);
		// latency-bound servers gain from deeper batches, busy ones only hold a batch past the budget
		long long took = NowMs() - start;
		if (took < 1000)
		{
			m_depth = std::min(PipelineMax, m_depth * 2);
		}
		else if (took > 2000)
		{
			m_depth = std::max(1, m_depth / 2);
		}
		m_errors = 0;
	}
	catch (ExchangeError& error)
	{
		// a busy pool says nothing about the server: it doesn't count toward the pause
		// that keeps a failing server out of the checks (F12: starved checks paused
		// every server, and the next fleet measured nothing)
		m_errors += error.busy ? 0 : 1;
		if (m_errors >= ServerGiveUp)
		{
			m_downUntilMs = NowMs() + m_retryAfterMs;
		}
		// the answers read before the connection broke stay
		answers = std::move(error.answers);
	}
	if (m_errors >= ServerGiveUp)
	{
		m_errors = 0;
	}
	answers.resize(batch.size(), Answer::Error);
	for (Answer& answer : answers)
	{
		if (answer == Answer::None)
		{
			answer = Answer::Error;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_slotMutex);
		m_active--;
	}
	m_slotCond.notify_one();
	return answers;
}

namespace
{

enum Final { fUnsettled = 0, fPresent = 1, fMissing = 2, fError = 3 };

struct CheckState
{
	std::vector<DonorHealth::Item> items;
	DonorHealth::ServerList servers;
	std::unique_ptr<std::atomic<int>[]> final;
	std::vector<std::vector<Answer>> votes;			// [article][server]
	std::vector<char> soft;							// a server had the article but its data was bad
	std::vector<std::unique_ptr<DonorHealth::BodyGate>> gates;
	std::mutex mutex;
	std::condition_variable cond;
	int left = 0;
	std::atomic<bool> stop{false};
};

// with state->mutex held
void Settle(CheckState& state, size_t index, int verdict)
{
	int expected = fUnsettled;
	if (state.final[index].compare_exchange_strong(expected, verdict))
	{
		if (--state.left == 0)
		{
			state.cond.notify_all();
		}
	}
}

// one server's pass over the articles, skipping the ones already found
void Walk(std::shared_ptr<CheckState> state, size_t serverIndex)
{
	// counted from its start (CheckItems) until it ends: shutdown waits for every
	// walker (DonorHealth::WaitAll)
	struct Counted
	{
		~Counted() { g_walkers--; }
	} counted;
	DonorHealth::Server& server = *state->servers[serverIndex];
	size_t count = state->items.size();
	size_t cursor = 0;
	while (cursor < count)
	{
		{
			std::lock_guard<std::mutex> guard(state->mutex);
			if (state->stop || DonorHealth::Stopping())
			{
				return;
			}
		}

		// the next articles nobody has found yet
		std::vector<size_t> todo;
		while (cursor < count && (int)todo.size() < server.Depth())
		{
			if (state->final[cursor] == fUnsettled)
			{
				todo.push_back(cursor);
			}
			cursor++;
		}
		if (todo.empty())
		{
			continue;
		}

		std::vector<Answer> answers;
		bool othersCanAnswer = false;
		for (size_t s = 0; s < state->servers.size(); s++)
		{
			othersCanAnswer |= s != serverIndex && !state->servers[s]->Paused();
		}
		if (server.Paused() && othersCanAnswer)
		{
			// a server in its pause abstains while others can answer
			answers.assign(todo.size(), Answer::Error);
		}
		else
		{
			std::vector<DonorHealth::Request> batch;
			for (size_t index : todo)
			{
				DonorHealth::Request request;
				request.messageId = state->items[index].messageId;
				request.gate = state->gates[index].get();
				request.groups = state->items[index].groups;
				batch.push_back(std::move(request));
			}
			answers = server.Ask(batch, [&state]() { return state->stop.load(); });
		}

		std::lock_guard<std::mutex> guard(state->mutex);
		if (state->stop)
		{
			return;
		}
		for (size_t k = 0; k < todo.size(); k++)
		{
			size_t index = todo[k];
			state->votes[index][serverIndex] = answers[k];
			state->soft[index] = state->soft[index] || answers[k] == Answer::BodyBad;
			if (answers[k] == Answer::Present)
			{
				Settle(*state, index, fPresent);
				continue;
			}
			// nobody had it: missing if any server said so definitively
			bool all = true;
			bool onlyErrors = true;
			for (Answer vote : state->votes[index])
			{
				all &= vote != Answer::None;
				onlyErrors &= vote == Answer::Error;
			}
			if (all)
			{
				Settle(*state, index, onlyErrors ? fError : fMissing);
			}
		}
	}
}

}

Health DonorHealth::CheckItems(const ServerList& servers, const std::vector<Item>& items, int budgetMs)
{
	Health health;
	size_t count = items.size();
	health.checked = (int)count;
	if (!count)
	{
		return health;
	}

	auto state = std::make_shared<CheckState>();
	state->items = items;
	state->servers = servers;
	state->final.reset(new std::atomic<int>[count]);
	state->votes.assign(count, std::vector<Answer>(servers.size(), Answer::None));
	state->soft.assign(count, 0);
	state->left = (int)count;
	CheckState* raw = state.get();
	for (size_t i = 0; i < count; i++)
	{
		state->final[i] = fUnsettled;
		std::unique_ptr<BodyGate> gate;
		if (items[i].body)
		{
			gate = std::make_unique<BodyGate>();
			gate->settled = [raw, i]() { return raw->final[i] != fUnsettled; };
			health.bodyChecked++;
		}
		state->gates.push_back(std::move(gate));
	}

	// every server walks the articles at its own pace; a request still in
	// flight when the check ends is for an article already settled (or the
	// budget ran out), and its answer is ignored
	for (size_t s = 0; s < servers.size(); s++)
	{
		g_walkers++;
		std::thread(Walk, state, s).detach();
	}

	{
		std::unique_lock<std::mutex> lock(state->mutex);
		// until every article is settled or the budget is over - or nzbget shuts
		// down (StopAll doesn't know this check's condition: polled)
		auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(10, budgetMs));
		while (state->left > 0 && !DonorHealth::Stopping() && std::chrono::steady_clock::now() < end)
		{
			state->cond.wait_until(lock, std::min(end, std::chrono::steady_clock::now() + std::chrono::milliseconds(100)));
		}
		state->stop = true;

		// the budget is over: an unsettled article is missing where at least
		// half of the servers said so (the ones that exist settle at once, so
		// leaving these out would inflate the share alive)
		for (size_t i = 0; i < count; i++)
		{
			if (state->final[i] != fUnsettled)
			{
				continue;
			}
			int misses = 0;
			bool present = false;
			for (Answer vote : state->votes[i])
			{
				misses += vote == Answer::Missing;
				present |= vote == Answer::Present;
			}
			if (!present && 2 * misses >= (int)servers.size())
			{
				state->final[i] = fMissing;
			}
		}

		for (size_t i = 0; i < count; i++)
		{
			int verdict = state->final[i];
			health.present += verdict == fPresent;
			health.missing += verdict == fMissing;
			health.error += verdict == fError;
			health.bodyBad += state->soft[i] && verdict != fPresent;
		}
	}
	return health;
}

Health DonorHealth::CheckPosting(const ServerList& servers, const std::vector<std::string>& ids,
	const std::shared_ptr<std::vector<std::string>>& groups, const Options& options, bool full)
{
	std::vector<Item> items = Plan(ids, groups, options.sample);
	std::vector<Item> first;
	if ((int)items.size() >= options.probe)
	{
		first.assign(items.begin(), items.begin() + options.probe);
	}
	else
	{
		// ("small" is a macro on Windows)
		SampleOptions probeSample = options.sample;
		probeSample.percent = 0;
		probeSample.minimum = options.probe;
		probeSample.maximum = options.probe;
		first = Plan(ids, groups, probeSample);
	}

	long long deadline = NowMs() + options.budgetMs;
	Health health = CheckItems(servers, first, options.budgetMs);
	if (full && !DeadProbe(health))
	{
		// nothing found and enough definitive misses ends it: otherwise the rest of the sample
		std::set<std::string> probed;
		for (const Item& item : first)
		{
			probed.insert(item.messageId);
		}
		std::vector<Item> rest;
		for (const Item& item : items)
		{
			if (!probed.count(item.messageId))
			{
				rest.push_back(item);
			}
		}
		health += CheckItems(servers, rest, (int)std::max(10LL, deadline - NowMs()));
	}
	return health;
}

void DonorHealth::CheckPostings(const ServerList& servers, const std::vector<Posting>& postings,
	const Options& options, bool full, int concurrency,
	const std::function<void(const std::string&, const Health&)>& emit)
{
	std::atomic<size_t> next{0};
	auto worker = [&]()
	{
		for (size_t index = next++; index < postings.size(); index = next++)
		{
			const Posting& posting = postings[index];
			emit(posting.key, CheckPosting(servers, posting.ids, posting.groups, options, full));
		}
	};

	std::vector<std::thread> threads;
	size_t count = std::min(postings.size(), (size_t)std::max(1, concurrency));
	for (size_t i = 0; i < count; i++)
	{
		threads.emplace_back(worker);
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
}
