
#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "Async.h"
#include "daemon/network/NetworkComponent.h"
#include "daemon/network/http/HttpClient.h"
#include "daemon/system/SystemInfo.h"

using namespace network;
using namespace std::chrono_literals;

namespace
{

// Accepts connections and sends `reply` (possibly partial), then stalls until the context is stopped.
class StallServer
{
public:
	explicit StallServer(std::string reply) :
		m_acceptor(m_context, {asio::ip::make_address("127.0.0.1"), 0})
	{
		asio::co_spawn(m_context, Accept(std::move(reply)), asio::detached);
		m_thread = std::thread{[this] { m_context.run(); }};
	}

	~StallServer()
	{
		m_context.stop();
		m_thread.join();
	}

	StallServer(const StallServer&) = delete;
	StallServer& operator=(const StallServer&) = delete;

	std::string Url(std::string_view scheme) const
	{
		return std::format("{}://127.0.0.1:{}/", scheme, m_acceptor.local_endpoint().port());
	}

	bool WaitAccepted(int count = 1) const
	{
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (m_accepted < count && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(1ms);
		}
		return m_accepted >= count;
	}

private:
	asio::awaitable<void> Accept(std::string reply)
	{
		for (;;)
		{
			auto [ec, socket] = co_await m_acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
			if (ec)
			{
				co_return;
			}
			++m_accepted;
			asio::co_spawn(m_context, Stall(std::move(socket), reply), asio::detached);
		}
	}

	static asio::awaitable<void> Stall(asio::ip::tcp::socket socket, std::string reply)
	{
		if (!reply.empty())
		{
			co_await asio::async_write(socket, asio::buffer(reply), asio::as_tuple(asio::use_awaitable));
		}
		asio::steady_timer hold(socket.get_executor(), 60s);
		co_await hold.async_wait(asio::as_tuple(asio::use_awaitable));
	}

	asio::io_context m_context;
	asio::ip::tcp::acceptor m_acceptor;
	std::atomic<int> m_accepted{0};
	std::thread m_thread;
};

struct ComponentFixture
{
	async::ExecutorPool pool{1};
	NetworkComponent component{NetworkComponent::Config{.certCheck = false}, pool.Next()};
};

// Stops the component on another thread; a Stop() stuck for 10 s fails the run instead of hanging it.
std::chrono::steady_clock::duration StopOrExit(NetworkComponent& component)
{
	std::promise<void> stopped;
	auto done = stopped.get_future();
	const auto start = std::chrono::steady_clock::now();
	std::thread stopper{[&] { component.Stop(); stopped.set_value(); }};
	if (done.wait_for(10s) != std::future_status::ready)
	{
		std::fprintf(stderr, "NetworkComponent::Stop() did not finish within 10 s\n");
		std::_Exit(EXIT_FAILURE);
	}
	stopper.join();
	return std::chrono::steady_clock::now() - start;
}

// A request stalled inside the client must end as Canceled, and Stop() must return promptly.
void CheckStopCancelsStalledRequest(NetworkComponent& component, StallServer& server, std::string_view scheme)
{
	auto client = component.GetHttpClient();
	Result<http::HttpResponse> result = Error::LocalFailure;
	std::thread fetch{[&] { result = client->SyncGet(server.Url(scheme), 30s); }};
	BOOST_REQUIRE(server.WaitAccepted());

	const auto elapsed = StopOrExit(component);
	fetch.join();
	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::Canceled);
	BOOST_CHECK_LT(elapsed, 5s);
}

// Application that keeps a request in flight across its own shutdown, as a real caller of the client would.
class FetchApp
{
public:
	static inline async::ExecutorPool* pool = nullptr;
	static inline async::Runtime<FetchApp>* runtime = nullptr;
	static inline std::string url;
	static inline std::thread fetch;
	static inline Result<http::HttpResponse> result = Error::LocalFailure;

	void Init(bool, async::Runtime<FetchApp>& app)
	{
		runtime = &app;
		app.AddComponent<NetworkComponent>(NetworkComponent::Config{.certCheck = false}, pool->Next());
	}

	void Run()
	{
		auto client = runtime->GetComponent<NetworkComponent>().GetHttpClient();
		fetch = std::thread{[client] { result = client->SyncGet(url, 30s); }};
		runtime->WaitForShutdown();
	}
};

}

BOOST_AUTO_TEST_SUITE(NetworkTest)

BOOST_AUTO_TEST_CASE(NetworkComponent_HasNetworkIdAndName)
{
	BOOST_CHECK(async::ComponentTraits<NetworkComponent>::Id == async::ComponentId::Network);
	BOOST_CHECK_EQUAL(async::ComponentTraits<NetworkComponent>::Name, "network");
}

BOOST_AUTO_TEST_CASE(NetworkComponent_CertStoreFollowsCertCheck)
{
	const fs::path dir = fs::temp_directory_path() / "nzbget_network_store_test";
	fs::create_directories(dir);
	async::ExecutorPool pool{1};
	NetworkComponent checked{NetworkComponent::Config{.certStore = dir, .certCheck = true}, pool.Next()};
	NetworkComponent unchecked{NetworkComponent::Config{.certCheck = false}, pool.Next()};

	BOOST_CHECK(checked.GetCertStore());
	BOOST_CHECK(!unchecked.GetCertStore());
	fs::remove_all(dir);
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_StopReleasesClientAndIsIdempotent, ComponentFixture)
{
	BOOST_REQUIRE(component.GetHttpClient());

	component.Stop();
	component.Stop();

	BOOST_CHECK(!component.GetHttpClient());
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_StopCancelsRequestStalledBeforeResponse, ComponentFixture)
{
	StallServer server("");
	CheckStopCancelsStalledRequest(component, server, "http");
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_StopCancelsRequestStalledMidBody, ComponentFixture)
{
	StallServer server("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nabc");
	CheckStopCancelsStalledRequest(component, server, "http");
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_StopCancelsRequestStalledInTlsHandshake, ComponentFixture)
{
	StallServer server("");
	CheckStopCancelsStalledRequest(component, server, "https");
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_StopCancelsAllConcurrentRequests, ComponentFixture)
{
	constexpr int requests = 5;
	StallServer server("");
	auto client = component.GetHttpClient();

	std::vector<Result<http::HttpResponse>> results(requests, Error::LocalFailure);
	std::vector<std::thread> fetches;
	for (int i = 0; i < requests; ++i)
	{
		fetches.emplace_back([&, i] { results[i] = client->SyncGet(server.Url("http"), 30s); });
	}
	BOOST_REQUIRE(server.WaitAccepted(requests));

	StopOrExit(component);
	for (auto& fetch : fetches)
	{
		fetch.join();
	}
	for (const auto& result : results)
	{
		BOOST_REQUIRE(result.has_error());
		BOOST_CHECK(result.error() == Error::Canceled);
	}
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_RequestAfterStopIsCanceledPromptly, ComponentFixture)
{
	auto client = component.GetHttpClient();
	component.Stop();

	const auto start = std::chrono::steady_clock::now();
	auto result = client->SyncGet("http://127.0.0.1:1/", 30s);
	const auto elapsed = std::chrono::steady_clock::now() - start;

	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::Canceled);
	BOOST_CHECK_LT(elapsed, 1s);
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_SyncGetTimeoutReturnsTimeoutPromptly, ComponentFixture)
{
	StallServer server("");

	const auto start = std::chrono::steady_clock::now();
	auto result = component.GetHttpClient()->SyncGet(server.Url("http"), 300ms);
	const auto elapsed = std::chrono::steady_clock::now() - start;

	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::Timeout);
	BOOST_CHECK_LT(elapsed, 3s);
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_NetworkInfoAfterStopReturnsEmptyPromptly, ComponentFixture)
{
	StallServer server("");
	const auto client = component.GetHttpClient();
	const std::string url = server.Url("http");
	component.Stop();

	const auto start = std::chrono::steady_clock::now();
	const System::Network network = System::FetchNetworkInfo(*client, url);
	const auto elapsed = std::chrono::steady_clock::now() - start;

	BOOST_CHECK(network.publicIP.empty());
	BOOST_CHECK(network.privateIP.empty());
	BOOST_CHECK_LT(elapsed, 1s);
}

BOOST_FIXTURE_TEST_CASE(NetworkComponent_MalformedUrlFailsWithoutHanging, ComponentFixture)
{
	const auto start = std::chrono::steady_clock::now();
	auto result = component.GetHttpClient()->SyncGet("http://[::1", 30s);
	const auto elapsed = std::chrono::steady_clock::now() - start;

	BOOST_CHECK(result.has_error());
	BOOST_CHECK_LT(elapsed, 1s);
}

BOOST_AUTO_TEST_CASE(NetworkComponent_RuntimeStopCancelsInFlightRequest)
{
	StallServer server("");
	async::ExecutorPool pool{1};
	FetchApp::pool = &pool;
	FetchApp::url = server.Url("http");
	FetchApp::result = Error::LocalFailure;

	async::Runtime<FetchApp> runtime;
	FetchApp::runtime = &runtime;
	std::promise<void> finished;
	auto finishedFuture = finished.get_future();
	std::thread runner{[&] { runtime.Run(); finished.set_value(); }};

	BOOST_REQUIRE(server.WaitAccepted());
	const auto start = std::chrono::steady_clock::now();
	runtime.Stop(async::ShutdownRequest::Stop);
	if (finishedFuture.wait_for(10s) != std::future_status::ready)
	{
		std::fprintf(stderr, "Runtime::Run() after Stop did not finish within 10 s\n");
		std::_Exit(EXIT_FAILURE);
	}
	const auto elapsed = std::chrono::steady_clock::now() - start;

	runner.join();
	FetchApp::fetch.join();
	BOOST_REQUIRE(FetchApp::result.has_error());
	BOOST_CHECK(FetchApp::result.error() == Error::Canceled);
	BOOST_CHECK_LT(elapsed, 5s);
}

BOOST_AUTO_TEST_SUITE_END()
