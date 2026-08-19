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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "nzbget.h"

#include <string>
#include <stdexcept>
#include "TaskProcessor.h"

namespace Core
{

namespace
{

constexpr size_t MIN_THREADS = 1;

size_t ValidateThreads(size_t threads)
{
	if (threads < MIN_THREADS)
	{
		throw std::invalid_argument(
			"Thread count must be at least " + std::to_string(MIN_THREADS) +
			", got " + std::to_string(threads));
	}
	return threads;
}

}

TaskProcessor<asio::io_context>::TaskProcessor(size_t threads)
{
	ValidateThreads(threads);

	m_contextPool.reserve(threads);
	m_guards.reserve(threads);
	m_threads.reserve(threads);

	try
	{
		for (size_t i = 0; i < threads; ++i)
		{
			m_contextPool.emplace_back(std::make_unique<asio::io_context>(1));
		}

		for (size_t i = 0; i < threads; ++i)
		{
			m_guards.emplace_back(m_contextPool[i]->get_executor());
		}

		for (size_t i = 0; i < threads; ++i)
		{
			m_threads.emplace_back([this, i]() { m_contextPool[i]->run(); });
		}
	}
	catch (...)
	{
		Stop();
		throw;
	}
}

TaskProcessor<asio::io_context>::~TaskProcessor()
{
	Stop();
}

void TaskProcessor<asio::io_context>::Stop() noexcept
{
	m_guards.clear();

	for (auto& ctx : m_contextPool)
	{
		ctx->stop();
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

TaskProcessor<asio::thread_pool>::TaskProcessor(size_t threads)
	: m_pool{ValidateThreads(threads)}
{
}

TaskProcessor<asio::thread_pool>::~TaskProcessor()
{
	Stop();
}

void TaskProcessor<asio::thread_pool>::Stop() noexcept
{
	m_pool.stop();
	m_pool.join();
}

}
