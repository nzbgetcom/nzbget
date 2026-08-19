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

#include <utility>
#include <csignal>
#include "SignalHandler.h"

namespace Core
{

SignalHandler::SignalHandler(Executor executor, Callback onShutdown, Callback onReload)
	: m_signals{executor, SIGINT, SIGTERM}
	, m_onShutdown{std::move(onShutdown)}
	, m_onReload{std::move(onReload)}
{
#ifndef _WIN32
	m_signals.add(SIGHUP);
#ifdef SIGQUIT
	m_signals.add(SIGQUIT);
#endif
#endif
}

SignalHandler::~SignalHandler()
{
	Stop();
}

void SignalHandler::Start()
{
	auto self = shared_from_this();
	asio::co_spawn(m_signals.get_executor(), SignalLoop(self), asio::detached);
}

void SignalHandler::Stop() noexcept
{
	ErrorCode ec;
	std::ignore = m_signals.cancel(ec);
}

Awaitable<void> SignalHandler::SignalLoop(std::shared_ptr<Component> self)
{
	while (true)
	{
		auto [ec, signum] = co_await m_signals.async_wait(asio::as_tuple(asio::use_awaitable));
		if (ec)
		{
			break; 
		}

#ifndef _WIN32
		if (signum == SIGHUP)
		{
			m_onReload();
			continue; 
		}
#endif

		m_onShutdown();
		break; 
	}
}


}
