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

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "Component.h"
#include "Engine.h"
#include "TaskProcessor.h"
#include "SignalHandler.h"
#include "Types.h"

namespace
{

struct TestComponent : public Core::Component
{
	const char* Name() const noexcept override { return "TestComponent"; }
	void Start() override { started = true; }
	void Stop() noexcept override { stopped = true; }
	bool started{false};
	bool stopped{false};
};

struct StartThrowingComponent : public Core::Component
{
	const char* Name() const noexcept override { return "StartThrowingComponent"; }
	void Start() override { throw std::runtime_error("simulated failure"); }
	void Stop() noexcept override {}
};

}

BOOST_AUTO_TEST_SUITE(CoreTest)

BOOST_AUTO_TEST_CASE(EngineLifecycleTest)
{
	Core::Engine::Config conf{};
	Core::Engine engine(conf);

	auto comp = engine.AddComponent<TestComponent>();
	BOOST_CHECK_EQUAL(std::string_view(comp->Name()), "TestComponent");

	engine.Start();
	engine.Start();
	BOOST_CHECK(comp->started);

	engine.Stop();
	engine.Stop();
	BOOST_CHECK(comp->stopped);
}

BOOST_AUTO_TEST_CASE(EngineStartFailurePropagationTest)
{
	Core::Engine::Config conf{};
	Core::Engine engine(conf);

	engine.AddComponent<TestComponent>();
	engine.AddComponent<StartThrowingComponent>();

	BOOST_CHECK_THROW(engine.Start(), std::runtime_error);

	try
	{
		engine.Start();
		BOOST_FAIL("Engine::Start() should have rethrown");
	}
	catch (const std::exception& e)
	{
		std::string message = e.what();
		BOOST_CHECK(message.find("StartThrowingComponent") != std::string::npos);
		BOOST_CHECK(message.find("failed to start") != std::string::npos);
	}

	engine.Stop();
}

BOOST_AUTO_TEST_CASE(EngineConcurrentStopTest)
{
	Core::Engine::Config conf{};
	Core::Engine engine(conf);
	engine.Start();

	std::vector<std::thread> threads;
	for (int i = 0; i < 8; ++i)
	{
		threads.emplace_back([&engine]() { engine.Stop(); });
	}

	for (auto& t : threads)
	{
		t.join();
	}
}

BOOST_AUTO_TEST_CASE(TaskProcessorRoundRobinDistributionTest)
{
	Core::TaskProcessor<Core::asio::io_context> pool(3);

	std::vector<std::future<std::thread::id>> futures;
	for (int i = 0; i < 6; ++i)
	{
		auto prom = std::make_shared<std::promise<std::thread::id>>();
		futures.push_back(prom->get_future());
		Core::asio::post(pool.GetExecutor(), [prom] {
			prom->set_value(std::this_thread::get_id());
		});
	}

	std::set<std::thread::id> threadIds;
	for (auto& f : futures)
	{
		threadIds.insert(f.get());
	}

	BOOST_CHECK_EQUAL(threadIds.size(), 3);
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(EngineWithIoComponentLifecycleTest)
{
	Core::Engine::Config conf{};
	Core::Engine engine(conf);

	std::atomic<bool> shutdownCalled{false};
	auto _ = engine.AddComponent<Core::SignalHandler>(
		engine.GetControlExecutor(),
		[&shutdownCalled]() { shutdownCalled.store(true); },
		[]() {}
	);

	engine.Start();
	engine.Stop();

	BOOST_CHECK(!shutdownCalled.load());
}

BOOST_AUTO_TEST_CASE(SignalHandlerStartStopTest)
{
	Core::TaskProcessor<Core::asio::io_context> executor(1);
	std::atomic<bool> shutdownCalled{false};
	std::atomic<bool> reloadCalled{false};

	auto handler = std::make_shared<Core::SignalHandler>(
		executor.GetExecutor(),
		[&shutdownCalled]() { shutdownCalled.store(true); },
		[&reloadCalled]() { reloadCalled.store(true); }
	);

	BOOST_CHECK_EQUAL(std::string_view(handler->Name()), "SignalHandler");
	handler->Start();
	handler->Stop();
	executor.Stop();

	BOOST_CHECK(!shutdownCalled.load());
	BOOST_CHECK(!reloadCalled.load());
}
#endif

BOOST_AUTO_TEST_SUITE_END()
