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

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>

#ifndef WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "Async.h"

using namespace std::chrono_literals;

namespace
{

std::atomic<int> g_componentStops{0};

class ProbeComponent final : public ::async::Component
{
public:
	void Stop() noexcept override { ++g_componentStops; }
};

class BrokenComponent final : public ::async::Component
{
public:
	BrokenComponent() { throw std::runtime_error("boom"); }
	void Stop() noexcept override {}
};

}

namespace async
{
template<> struct ComponentTraits<ProbeComponent>
{
	static constexpr ComponentId Id = ComponentId::Network;
	static constexpr std::string_view Name = "probe";
};
template<> struct ComponentTraits<BrokenComponent>
{
	static constexpr ComponentId Id = ComponentId::Network;
	static constexpr std::string_view Name = "broken";
};
}

namespace
{

class FakeApp
{
public:
	void Init(bool reload, ::async::Runtime<FakeApp>& runtime)
	{
		++inits;
		m_runtime = &runtime;
		reloadFlags.push_back(reload);
		if (failInit)
		{
			runtime.AddComponent<BrokenComponent>();
		}
		ProbeComponent& added = runtime.AddComponent<ProbeComponent>();
		lookupMatches = runtime.GetComponent<ProbeComponent>() == &added;
	}

	void Run()
	{
		++runs;
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, [this] { return m_stopped; });
	}

	void Stop(bool reload)
	{
		m_runtime->BeginShutdown();
		if (hangStop)
		{
			std::this_thread::sleep_for(30s);
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		m_reloading = reload;
		stopArgs.push_back(reload);
		m_stopped = true;
		m_cond.notify_all();
	}

	bool GetReloading() const { return m_reloading; }

	static inline std::atomic<int> runs{0};
	static inline int inits{0};
	static inline bool failInit{false};
	static inline bool hangStop{false};
	static inline bool lookupMatches{false};
	static inline std::vector<bool> reloadFlags;
	static inline std::vector<bool> stopArgs;

	static void Reset()
	{
		runs = 0;
		inits = 0;
		failInit = false;
		hangStop = false;
		lookupMatches = false;
		reloadFlags.clear();
		stopArgs.clear();
		g_componentStops = 0;
	}

private:
	::async::Runtime<FakeApp>* m_runtime = nullptr;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	bool m_stopped = false;
	bool m_reloading = false;
};

void WaitForRuns(int count)
{
	while (FakeApp::runs < count)
	{
		std::this_thread::sleep_for(1ms);
	}
}

}

#ifndef WIN32
// Runs `body` in a forked child; returns its exit status (-1 if it did not exit normally).
template<class Body>
int RunInChild(Body body)
{
	pid_t pid = fork();
	if (pid == 0)
	{
		body();
		_exit(0);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Hung App::Stop() must end the process with EXIT_FAILURE once the grace expires.
template<class Trigger>
int HungStopExitStatus(Trigger trigger)
{
	return RunInChild([&]
	{
		FakeApp::Reset();
		FakeApp::hangStop = true;
		std::unique_ptr<FakeApp> app;
		::async::Runtime<FakeApp> runtime(app, 1s);
		std::thread signaller([&] { WaitForRuns(1); trigger(app.get()); });
		runtime.Run();
		signaller.join();
	});
}
#endif

BOOST_AUTO_TEST_SUITE(AsyncTest)

BOOST_AUTO_TEST_CASE(Runtime_SigtermStopsAppAndComponents)
{
	FakeApp::Reset();
	std::unique_ptr<FakeApp> app;
	::async::Runtime<FakeApp> runtime(app, 1s);

	std::thread signaller([] { WaitForRuns(1); std::raise(SIGTERM); });
	runtime.Run();
	signaller.join();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK(FakeApp::stopArgs == std::vector<bool>{false});
	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
	BOOST_CHECK(runtime.GetComponent<ProbeComponent>() == nullptr);
	BOOST_CHECK(FakeApp::lookupMatches);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(Runtime_SighupRecreatesAppAndComponents)
{
	FakeApp::Reset();
	std::unique_ptr<FakeApp> app;
	::async::Runtime<FakeApp> runtime(app, 1s);

	std::thread signaller([]
	{
		WaitForRuns(1);
		std::raise(SIGHUP);
		WaitForRuns(2);
		std::raise(SIGTERM);
	});
	runtime.Run();
	signaller.join();

	BOOST_CHECK_EQUAL(FakeApp::inits, 2);
	BOOST_CHECK((FakeApp::reloadFlags == std::vector<bool>{false, true}));
	BOOST_CHECK((FakeApp::stopArgs == std::vector<bool>{true, false}));
	BOOST_CHECK_EQUAL(g_componentStops.load(), 2);
}
#endif

BOOST_AUTO_TEST_CASE(Runtime_StopFromAnyThreadStopsComponents)
{
	FakeApp::Reset();
	std::unique_ptr<FakeApp> app;
	::async::Runtime<FakeApp> runtime(app, 1s);

	std::thread stopper([&] { WaitForRuns(1); app->Stop(false); });
	runtime.Run();
	stopper.join();

	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(Runtime_HungStopFromSignalExitsWithFailure)
{
	BOOST_CHECK_EQUAL(HungStopExitStatus([](FakeApp*) { std::raise(SIGTERM); }), EXIT_FAILURE);
}

BOOST_AUTO_TEST_CASE(Runtime_HungStopFromRpcExitsWithFailure)
{
	BOOST_CHECK_EQUAL(HungStopExitStatus([](FakeApp* app) { app->Stop(false); }), EXIT_FAILURE);
}

BOOST_AUTO_TEST_CASE(Runtime_SecondSignalWhileStoppingExitsWithFailure)
{
	BOOST_CHECK_EQUAL(HungStopExitStatus([](FakeApp* app)
	{
		std::thread first([app] { app->Stop(false); });
		std::this_thread::sleep_for(50ms);
		std::raise(SIGTERM);
		first.join();
	}), EXIT_FAILURE);
}
#endif

BOOST_AUTO_TEST_CASE(Runtime_ComponentFailureIsReportedWithItsName)
{
	FakeApp::Reset();
	FakeApp::failInit = true;
	std::unique_ptr<FakeApp> app;
	::async::Runtime<FakeApp> runtime(app, 1s);

	try
	{
		runtime.Run();
		BOOST_FAIL("Run() must throw");
	}
	catch (const std::runtime_error& e)
	{
		BOOST_CHECK_EQUAL(std::string(e.what()), "'broken' component failed to start: boom");
	}
	BOOST_CHECK_EQUAL(FakeApp::runs.load(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
