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

#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include <memory>
#include <mutex>
#include <vector>
#include <type_traits>
#include "Component.h"
#include "TaskProcessor.h"

namespace Core
{

class Engine final
{
public:
	struct Config
	{
		size_t netThreads{1};
		size_t diskThreads{1};
		size_t computeThreads{1};
	};

	explicit Engine(Config conf);
	~Engine();

	Engine() = delete;
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	Engine(Engine&&) = delete;
	Engine& operator=(Engine&&) = delete;

	template <typename T, typename... Args>
	std::shared_ptr<T> AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Core::Component");
		auto comp = std::make_shared<T>(std::forward<Args>(args)...);
		m_components.push_back(comp);
		return comp;
	}

	void Start();
	void Stop() noexcept;

	Executor GetControlExecutor() noexcept { return m_control.GetExecutor(); }
	Executor GetNetworkExecutor() noexcept { return m_network.GetExecutor(); }
	Executor GetDiskExecutor()    noexcept { return m_disk.GetExecutor(); }
	Executor GetComputeExecutor() noexcept { return m_compute.GetExecutor(); }

private:
	void StopProcessors() noexcept;
	TaskProcessor<asio::io_context> m_control;
	TaskProcessor<asio::io_context> m_network;
	TaskProcessor<asio::io_context> m_disk;
	TaskProcessor<asio::thread_pool> m_compute;

	std::vector<std::shared_ptr<Component>> m_components;

	std::once_flag m_started;
	std::once_flag m_stopped;
};

}

#endif
