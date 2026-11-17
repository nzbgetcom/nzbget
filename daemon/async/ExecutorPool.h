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

#ifndef ASYNC_EXECUTORPOOL_H
#define ASYNC_EXECUTORPOOL_H

#include "Types.h"

namespace async
{

/// Fixed set of single-threaded io_contexts; handlers on one executor never run concurrently.
/// Only the owner can Stop() it; users get Executor values.
class ExecutorPool final
{
public:
	/// @throws std::invalid_argument if @p threads is zero.
	explicit ExecutorPool(size_t threads);
	~ExecutorPool();
	ExecutorPool(const ExecutorPool&) = delete;
	ExecutorPool& operator=(const ExecutorPool&) = delete;
	ExecutorPool(ExecutorPool&&) = delete;
	ExecutorPool& operator=(ExecutorPool&&) = delete;

	/// @return The next executor, round-robin.
	Executor Next() noexcept
	{
		size_t idx = m_next.fetch_add(1, std::memory_order_relaxed);
		return m_contexts[idx % m_contexts.size()]->get_executor();
	}

	/// Stops all contexts and joins the threads; idempotent, not restartable.
	void Stop() noexcept;
	bool IsStopped() const noexcept { return m_stopped.load(std::memory_order_relaxed); }

private:
	void Run(size_t threadIndex);

	std::vector<std::unique_ptr<asio::io_context>> m_contexts;
	std::vector<ExecutorGuard> m_guards;
	std::vector<std::thread> m_threads;
	std::atomic<size_t> m_next{0};
	std::atomic<bool> m_stopped{false};
};

}

#endif // ASYNC_EXECUTORPOOL_H
