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

#ifndef CORE_TASK_PROCESSOR_H
#define CORE_TASK_PROCESSOR_H

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "Types.h"

namespace Core
{

template <typename Context>
class TaskProcessor;

template <>
class TaskProcessor<asio::io_context> final
{
public:
	explicit TaskProcessor(size_t threads);
	~TaskProcessor();
	TaskProcessor(const TaskProcessor&) = delete;
	TaskProcessor& operator=(const TaskProcessor&) = delete;
	TaskProcessor(TaskProcessor&&) = delete;
	TaskProcessor& operator=(TaskProcessor&&) = delete;

	Executor GetExecutor() noexcept
	{
		size_t idx = m_idx.fetch_add(1, std::memory_order_relaxed);
		return m_contextPool[idx % m_contextPool.size()]->get_executor();
	}

	void Stop() noexcept;

private:
	std::vector<std::unique_ptr<asio::io_context>> m_contextPool;
	std::vector<ExecutorGuard> m_guards;
	std::vector<std::thread> m_threads;
	std::atomic<size_t> m_idx{0};
};

template <>
class TaskProcessor<asio::thread_pool> final
{
public:
	explicit TaskProcessor(size_t threads);
	~TaskProcessor();
	TaskProcessor(const TaskProcessor&) = delete;
	TaskProcessor& operator=(const TaskProcessor&) = delete;
	TaskProcessor(TaskProcessor&&) = delete;
	TaskProcessor& operator=(TaskProcessor&&) = delete;

	Executor GetExecutor() noexcept
	{
		return m_pool.get_executor();
	}

	void Stop() noexcept;

private:
	asio::thread_pool m_pool;
};

}

#endif
