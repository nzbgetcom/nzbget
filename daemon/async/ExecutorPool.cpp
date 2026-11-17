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

#include "ExecutorPool.h"

using namespace async;

ExecutorPool::ExecutorPool(size_t threads)
{
	if (threads == 0)
		throw std::invalid_argument("ExecutorPool requires at least 1 thread");

	m_contexts.reserve(threads);
	m_guards.reserve(threads);
	m_threads.reserve(threads);

	try
	{
		for (size_t i = 0; i < threads; ++i)
		{
			m_contexts.emplace_back(std::make_unique<asio::io_context>(1));
			m_guards.emplace_back(m_contexts[i]->get_executor());
		}
		for (size_t i = 0; i < threads; ++i)
		{
			m_threads.emplace_back([this, i]() { Run(i); });
		}
	}
	catch (...)
	{
		Stop();
		throw;
	}
}

ExecutorPool::~ExecutorPool()
{
	Stop();
}

void ExecutorPool::Stop() noexcept
{
	if (m_stopped.exchange(true)) return;

	m_guards.clear();

	for (auto& context : m_contexts)
	{
		context->stop();
	}

	for (auto& t : m_threads)
	{
		if (t.joinable())
		{
			t.join();
		}
	}

	m_threads.clear();
}

void ExecutorPool::Run(size_t threadIndex)
{
	m_contexts[threadIndex]->run();
}
