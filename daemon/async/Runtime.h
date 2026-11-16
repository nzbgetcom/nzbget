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

#ifndef ASYNC_RUNTIME_H
#define ASYNC_RUNTIME_H

#include <algorithm>
#include <array>
#include <concepts>
#include <condition_variable>
#include <csignal>
#include <format>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "Component.h"
#include "ExecutorPool.h"

namespace async
{

template<class App>
class Runtime;

/// What the application must do next. A stop outranks a reload, so a reload never cancels a stop.
enum class ShutdownRequest { None, Reload, Stop };

/// Interface Runtime requires from the application it drives.
template<class T>
concept Application = requires(T& app, bool reload, Runtime<T>& runtime) {
	{ app.Init(reload, runtime) } -> std::same_as<void>;
	{ app.Run() } -> std::same_as<void>;
};

/// Drives an application: signals, components, shutdown requests.
/// Signals are handled on their own thread, so a second stop signal still gets through a hung teardown.
/// AddComponent() only inside App::Init(); GetComponent() is read-only afterwards.
template<class App>
class Runtime final
{
public:
	Runtime()
	{
		static_assert(Application<App>);
	}

	/// Stops leftover components, e.g. after Init() or App::Run() threw.
	~Runtime()
	{
		StopComponents();
		m_app.reset();
	}

	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	/// Constructs T in its slot; a throwing constructor becomes "'<name>' component failed to start: <reason>".
	template<ComponentType T, class... Args>
	T& AddComponent(Args&&... args)
	{
		std::unique_ptr<Component>& slot = m_components[Index<T>()];
		try
		{
			slot = std::make_unique<T>(std::forward<Args>(args)...);
		}
		catch (const std::exception& e)
		{
			throw std::runtime_error(std::format("'{}' component failed to start: {}", ComponentTraits<T>::Name, e.what()));
		}
		catch (...)
		{
			throw std::runtime_error(std::format("'{}' component failed to start: unknown error", ComponentTraits<T>::Name));
		}
		return static_cast<T&>(*slot);
	}

	/// @return The registered T; call only after Init() registered it, do not cache.
	template<ComponentType T>
	T& GetComponent() const noexcept
	{
		return static_cast<T&>(*m_components[Index<T>()]);
	}

	/// Records a stop or reload request; any thread. The application reacts on its own thread.
	void Stop(ShutdownRequest request) noexcept
	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_request = std::max(m_request, request);
		m_requestCond.notify_all();
	}

	/// Blocks until a stop or reload is requested.
	void WaitForShutdown()
	{
		std::unique_lock<std::mutex> lock(m_requestMutex);
		m_requestCond.wait(lock, [this] { return m_request != ShutdownRequest::None; });
	}

	ShutdownRequest GetShutdownRequest()
	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		return m_request;
	}

	bool HasShutdownRequest() { return GetShutdownRequest() != ShutdownRequest::None; }

	/// Runs the application until it stops, recreating it on reload.
	/// The first Init() runs before the signal thread exists, because it may fork (Daemonize).
	/// @throws whatever App::Init(), App::Run() or AddComponent() throws.
	void Run()
	{
		CreateApp(false);

		ExecutorPool signalThread{1};
		asio::signal_set signals(signalThread.Next(), SIGINT, SIGTERM);
#ifndef WIN32
		signals.add(SIGQUIT);
		signals.add(SIGHUP);
#endif
		AwaitSignal(signals);

		try
		{
			for (;;)
			{
				m_app->Run();
				StopComponents();
				m_app.reset();
				if (!TakeReload())
				{
					break;
				}
				CreateApp(true);
			}
		}
		catch (...)
		{
			signalThread.Stop();
			throw;
		}
		// a handler already queued must not run after signals is destroyed
		signalThread.Stop();
	}

private:
	/// Maps stop signals to a stop and SIGHUP to a reload. A stop signal received while a stop is
	/// already pending restores the default action and re-raises it, which ends the process.
	void AwaitSignal(asio::signal_set& signals)
	{
		signals.async_wait([this, &signals](const ErrorCode& ec, int signum)
		{
			if (ec)
			{
				return;
			}
			// re-arm first: a signal arriving while this one is handled must not be lost
			AwaitSignal(signals);
#ifndef WIN32
			if (signum == SIGHUP)
			{
				Stop(ShutdownRequest::Reload);
				return;
			}
#endif
			if (GetShutdownRequest() == ShutdownRequest::Stop)
			{
				std::signal(signum, SIG_DFL);
				std::raise(signum);
			}
			Stop(ShutdownRequest::Stop);
		});
	}

	/// Consumes a pending reload; a stop stays pending.
	/// @return true if the application must be recreated.
	bool TakeReload()
	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		if (m_request != ShutdownRequest::Reload)
		{
			return false;
		}
		m_request = ShutdownRequest::None;
		return true;
	}

	void CreateApp(bool reload)
	{
		m_app = std::make_unique<App>();
		m_app->Init(reload, *this);
	}

	/// Stops and destroys components in reverse slot order.
	void StopComponents() noexcept
	{
		for (size_t i = m_components.size(); i-- > 0;)
		{
			if (m_components[i])
			{
				m_components[i]->Stop();
				m_components[i].reset();
			}
		}
	}

	template<ComponentType T>
	static constexpr size_t Index() noexcept
	{
		return static_cast<size_t>(ComponentTraits<T>::Id);
	}

	std::array<std::unique_ptr<Component>, static_cast<size_t>(ComponentId::Count)> m_components;
	std::unique_ptr<App> m_app;
	std::mutex m_requestMutex;
	std::condition_variable m_requestCond;
	ShutdownRequest m_request = ShutdownRequest::None;
};

} // namespace async

#endif // ASYNC_RUNTIME_H
