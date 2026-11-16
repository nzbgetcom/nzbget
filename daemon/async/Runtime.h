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

#include <array>
#include <cassert>
#include <chrono>
#include <concepts>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "Component.h"
#include "ExecutorPool.h"

namespace async
{

template<class App>
class Runtime;

/// Interface Runtime requires from the application it drives.
template<class T>
concept Application = requires(T& app, bool reload, Runtime<T>& runtime) {
	{ app.Init(reload, runtime) } -> std::same_as<void>;
	{ app.Run() } -> std::same_as<void>;
	{ app.Stop(reload) } -> std::same_as<void>;
	{ app.GetReloading() } -> std::convertible_to<bool>;
};

/// Drives an application: signals, components, shutdown watchdog.
/// A stop slower than the grace period, or a second stop signal, ends the process with _Exit.
/// AddComponent() only inside App::Init(); GetComponent() is read-only afterwards.
template<class App>
class Runtime final
{
public:
	/// @param app    Caller-owned slot; Runtime (re)creates the application in it.
	/// @param grace  Time allowed for stopping the application and its components.
	explicit Runtime(std::unique_ptr<App>& app, std::chrono::seconds grace = std::chrono::seconds{3}) :
		m_app(app), m_grace(grace)
	{
		static_assert(Application<App>);
	}

	/// Stops leftover components, e.g. after Init() threw.
	~Runtime() { StopComponents(Clock::now()); }

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

	/// @return The registered T or nullptr; valid until components are stopped, do not cache.
	template<ComponentType T>
	T* GetComponent() const noexcept
	{
		return static_cast<T*>(m_components[Index<T>()].get());
	}

	/// Runs the application until it stops, recreating it on reload.
	/// The first Init() runs before any thread exists (it may fork); signals are handled only after it.
	/// @throws whatever App::Init() or AddComponent() throws.
	void Run()
	{
		m_app = std::make_unique<App>();
		m_app->Init(false, *this);

		m_supervisor = std::make_unique<Supervisor>(m_app, m_grace);
		for (;;)
		{
			std::thread worker([this]
			{
				m_app->Run();
				m_supervisor->Quit();
			});
			m_supervisor->Wait();
			worker.join();

			m_supervisor->BeginShutdown();
			StopComponents(Clock::now() + m_grace);
			m_supervisor->Reset();

			if (!m_app->GetReloading())
			{
				return;
			}
			m_app = std::make_unique<App>();
			m_app->Init(true, *this);
		}
	}

	/// Starts the shutdown countdown (idempotent, any thread); expiry ends the process with _Exit.
	/// App::Stop() must call it first: every stop path, signal or not, then has the same deadline.
	void BeginShutdown() noexcept
	{
		if (m_supervisor)
		{
			m_supervisor->BeginShutdown();
		}
	}

private:
	/// Maps SIGINT/SIGTERM (POSIX: SIGQUIT) to stop and POSIX SIGHUP to reload.
	/// The watchdog has its own thread: Stop() blocks the control thread that delivers signals.
	class Supervisor
	{
	public:
		Supervisor(std::unique_ptr<App>& app, std::chrono::seconds grace) :
			m_app(app), m_grace(grace), m_signals(m_control, SIGINT, SIGTERM)
		{
#ifndef WIN32
			m_signals.add(SIGQUIT);
			m_signals.add(SIGHUP);
#endif
			WaitForSignal();
		}

		/// Dispatches signals on the calling thread until Quit().
		void Wait() { m_control.run(); }

		/// Ends Wait(); called when App::Run() returns.
		void Quit() { m_control.stop(); }

		/// Starts the countdown unless already running; expiry calls _Exit(EXIT_FAILURE).
		void BeginShutdown() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_stopping)
			{
				return;
			}
			m_stopping = true;
			m_watchdog.expires_after(m_grace);
			m_watchdog.async_wait([](const ErrorCode& ec)
			{
				// A cancelled wait completes with an error: not an expiry.
				if (!ec)
				{
					std::_Exit(EXIT_FAILURE);
				}
			});
		}

		/// Cancels the countdown and readies the next run.
		void Reset()
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_watchdog.cancel();
				m_stopping = false;
			}
			m_control.restart();
		}

	private:
		/// Waits for one signal; every handler path waits again.
		void WaitForSignal()
		{
			m_signals.async_wait([this](const ErrorCode& ec, int signum)
			{
				if (ec)
				{
					return;
				}
#ifndef WIN32
				if (signum == SIGHUP)
				{
					// Reload is ignored while stopping.
					if (!IsStopping())
					{
						m_app->Stop(true);
					}
					WaitForSignal();
					return;
				}
#endif
				// A stop signal while already stopping is an impatient operator: die now.
				if (IsStopping())
				{
					std::_Exit(EXIT_FAILURE);
				}
				m_app->Stop(false);
				WaitForSignal();
			});
		}

		bool IsStopping()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_stopping;
		}

		std::unique_ptr<App>& m_app;
		std::chrono::seconds m_grace;
		asio::io_context m_control{1};
		asio::signal_set m_signals;
		ExecutorPool m_guard{1};
		asio::steady_timer m_watchdog{m_guard.Next()};
		std::mutex m_mutex;
		bool m_stopping = false;
	};

	/// Stops and destroys components in reverse slot order; the only caller of Component::Stop().
	void StopComponents(Clock::time_point deadline) noexcept
	{
		for (std::size_t i = m_components.size(); i-- > 0;)
		{
			if (m_components[i])
			{
				m_components[i]->Stop(deadline);
				m_components[i].reset();
			}
		}
	}

	template<ComponentType T>
	static constexpr std::size_t Index() noexcept
	{
		return static_cast<std::size_t>(ComponentTraits<T>::Id);
	}

	std::unique_ptr<App>& m_app;
	std::chrono::seconds m_grace;
	std::array<std::unique_ptr<Component>, static_cast<std::size_t>(ComponentId::Count)> m_components;
	std::unique_ptr<Supervisor> m_supervisor;
};

} // namespace async

#endif // ASYNC_RUNTIME_H
