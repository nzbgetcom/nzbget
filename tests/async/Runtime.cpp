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
#include <csignal>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#ifndef WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "Async.h"

using namespace std::chrono_literals;
using ::async::ShutdownRequest;

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

// Run() blocks until a request, then records it as its teardown; the teardown may be slow or hang.
class FakeApp
{
public:
	void Init(bool reload, ::async::Runtime<FakeApp>& runtime)
	{
		++inits;
		m_runtime = &runtime;
		reloadFlags.push_back(reload);
		if (reload && failReloadInit)
		{
			throw std::runtime_error("reload boom");
		}
		if (failInit)
		{
			runtime.AddComponent<BrokenComponent>();
		}
		ProbeComponent& added = runtime.AddComponent<ProbeComponent>();
		lookupMatches = &runtime.GetComponent<ProbeComponent>() == &added;
	}

	void Run()
	{
		++runs;
		if (failRun)
		{
			throw std::runtime_error("run boom");
		}
		if (finishWithoutRequest)
		{
			return;
		}
		m_runtime->WaitForShutdown();
		requests.push_back(m_runtime->GetShutdownRequest());
		teardownStarted = true;
		std::this_thread::sleep_for(teardownDelay);
		if (hangStop)
		{
			std::this_thread::sleep_for(30s);
		}
	}

	static inline std::atomic<int> runs{0};
	static inline std::atomic<bool> teardownStarted{false};
	static inline int inits{0};
	static inline bool failInit{false};
	static inline bool hangStop{false};
	static inline bool failReloadInit{false};
	static inline bool failRun{false};
	static inline bool finishWithoutRequest{false};
	static inline std::chrono::milliseconds teardownDelay{0};
	static inline bool lookupMatches{false};
	static inline std::vector<bool> reloadFlags;
	static inline std::vector<ShutdownRequest> requests;

	static void Reset()
	{
		runs = 0;
		teardownStarted = false;
		inits = 0;
		failInit = false;
		hangStop = false;
		failReloadInit = false;
		failRun = false;
		finishWithoutRequest = false;
		teardownDelay = std::chrono::milliseconds{0};
		lookupMatches = false;
		reloadFlags.clear();
		requests.clear();
		g_componentStops = 0;
	}

private:
	::async::Runtime<FakeApp>* m_runtime = nullptr;
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
// Runs `body` in a forked child; returns its exit code, or 128 + signal if it was killed by a signal.
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
	return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : WEXITSTATUS(status);
}
#endif

BOOST_AUTO_TEST_SUITE(AsyncTest)

BOOST_AUTO_TEST_CASE(Runtime_SigtermStopsAppAndComponents)
{
	FakeApp::Reset();
	::async::Runtime<FakeApp> runtime;

	std::thread signaller([] { WaitForRuns(1); std::raise(SIGTERM); });
	runtime.Run();
	signaller.join();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK((FakeApp::requests == std::vector<ShutdownRequest>{ShutdownRequest::Stop}));
	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
	BOOST_CHECK(FakeApp::lookupMatches);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(Runtime_SighupRecreatesAppAndComponents)
{
	FakeApp::Reset();
	::async::Runtime<FakeApp> runtime;

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
	BOOST_CHECK((FakeApp::requests == std::vector<ShutdownRequest>{ShutdownRequest::Reload, ShutdownRequest::Stop}));
	BOOST_CHECK_EQUAL(g_componentStops.load(), 2);
}
#endif

BOOST_AUTO_TEST_CASE(Runtime_StopFromAnyThreadStopsComponents)
{
	FakeApp::Reset();
	::async::Runtime<FakeApp> runtime;

	std::thread stopper([&] { WaitForRuns(1); runtime.Stop(ShutdownRequest::Stop); });
	runtime.Run();
	stopper.join();

	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
}

BOOST_AUTO_TEST_CASE(Runtime_StopBeforeRunIsNotLost)
{
	FakeApp::Reset();
	::async::Runtime<FakeApp> runtime;

	runtime.Stop(ShutdownRequest::Stop);
	runtime.Run();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK((FakeApp::requests == std::vector<ShutdownRequest>{ShutdownRequest::Stop}));
}

BOOST_AUTO_TEST_CASE(Runtime_ReloadAfterStopIsIgnored)
{
	FakeApp::Reset();
	::async::Runtime<FakeApp> runtime;

	runtime.Stop(ShutdownRequest::Stop);
	runtime.Stop(ShutdownRequest::Reload);
	runtime.Run();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK((FakeApp::requests == std::vector<ShutdownRequest>{ShutdownRequest::Stop}));
}

BOOST_AUTO_TEST_CASE(Runtime_StopOutranksReloadDuringTeardown)
{
	FakeApp::Reset();
	FakeApp::teardownDelay = 300ms;
	::async::Runtime<FakeApp> runtime;

	std::thread runner([&] { runtime.Run(); });
	WaitForRuns(1);
	runtime.Stop(ShutdownRequest::Reload);
	while (!FakeApp::teardownStarted)
	{
		std::this_thread::sleep_for(1ms);
	}
	runtime.Stop(ShutdownRequest::Stop);
	runner.join();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK((FakeApp::requests == std::vector<ShutdownRequest>{ShutdownRequest::Reload}));
	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
}

BOOST_AUTO_TEST_CASE(Runtime_AppFinishingWithoutRequestEndsRun)
{
	FakeApp::Reset();
	FakeApp::finishWithoutRequest = true;
	::async::Runtime<FakeApp> runtime;

	runtime.Run();

	BOOST_CHECK_EQUAL(FakeApp::inits, 1);
	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
}

#ifndef WIN32
// A hung teardown ignores the first stop; a second stop signal kills the process, as in develop.
BOOST_AUTO_TEST_CASE(Runtime_SecondSignalWhileStoppingKillsProcess)
{
	BOOST_CHECK_EQUAL(RunInChild([]
	{
		FakeApp::Reset();
		FakeApp::hangStop = true;
		::async::Runtime<FakeApp> runtime;
		std::thread signaller([&]
		{
			WaitForRuns(1);
			runtime.Stop(ShutdownRequest::Stop);
			std::this_thread::sleep_for(100ms);
			std::raise(SIGTERM);
		});
		runtime.Run();
		signaller.join();
	}), 128 + SIGTERM);
}
#endif

BOOST_AUTO_TEST_CASE(Runtime_ReloadFailurePropagatesToCaller)
{
	FakeApp::Reset();
	FakeApp::failReloadInit = true;
	::async::Runtime<FakeApp> runtime;

	std::thread stopper([&] { WaitForRuns(1); runtime.Stop(ShutdownRequest::Reload); });
	std::string error;
	try
	{
		runtime.Run();
	}
	catch (const std::runtime_error& e)
	{
		error = e.what();
	}
	stopper.join();

	BOOST_CHECK_EQUAL(error, "reload boom");
	BOOST_CHECK_EQUAL(FakeApp::inits, 2);
}

BOOST_AUTO_TEST_CASE(Runtime_RunExceptionStopsComponentsOnDestruction)
{
	FakeApp::Reset();
	FakeApp::failRun = true;
	std::string error;
	{
		::async::Runtime<FakeApp> runtime;
		try
		{
			runtime.Run();
		}
		catch (const std::runtime_error& e)
		{
			error = e.what();
		}
		BOOST_CHECK_EQUAL(g_componentStops.load(), 0);
	}

	BOOST_CHECK_EQUAL(error, "run boom");
	BOOST_CHECK_EQUAL(g_componentStops.load(), 1);
}

BOOST_AUTO_TEST_CASE(Runtime_ComponentFailureIsReportedWithItsName)
{
	FakeApp::Reset();
	FakeApp::failInit = true;
	::async::Runtime<FakeApp> runtime;

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
