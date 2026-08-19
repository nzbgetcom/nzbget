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

#include <algorithm>
#include "Engine.h"

namespace Core
{

Engine::Engine(Config conf)
	: m_control{1}
	, m_network{conf.netThreads}
	, m_disk{conf.diskThreads}
	, m_compute{conf.computeThreads}
{
}

Engine::~Engine()
{
	Stop();
}

void Engine::Start()
{
	std::call_once(m_started, [&]()
	{
		std::vector<Component*> startedComponents;
		startedComponents.reserve(m_components.size());

		for (auto& comp : m_components)
		{
			try
			{
				comp->Start();
				startedComponents.push_back(comp.get());
			}
			catch (const std::exception& ex)
			{
				for (auto it = startedComponents.rbegin(); it != startedComponents.rend(); ++it)
				{
					(*it)->Stop();
				}
				StopProcessors();
				throw std::runtime_error(std::string("'") + comp->Name() + "' component failed to start: " + ex.what());
			}
			catch (...)
			{
				for (auto it = startedComponents.rbegin(); it != startedComponents.rend(); ++it)
				{
					(*it)->Stop();
				}
				StopProcessors();
				throw std::runtime_error(std::string("'") + comp->Name() + "' component failed to start: unknown error");
			}
		}
	});
}

void Engine::Stop() noexcept
{
	std::call_once(m_stopped, [&]()
	{
		for (auto it = m_components.rbegin(); it != m_components.rend(); ++it)
		{
			try
			{
				(*it)->Stop();
			}
			catch (const std::exception& ex)
			{
				fprintf(stderr, "Failed to stop '%s' component: %s\n", (*it)->Name(), ex.what());
			}
			catch (...)
			{
				fprintf(stderr, "Failed to stop '%s' component: unknown error\n", (*it)->Name());
			}
		}

		StopProcessors();
	});
}

void Engine::StopProcessors() noexcept
{
	m_network.Stop();
	m_disk.Stop();
	m_compute.Stop();
	m_control.Stop();
}

}
