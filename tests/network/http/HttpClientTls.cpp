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

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_future.hpp>

#include <filesystem>
#include <fstream>
#include <thread>

#include "daemon/network/DnsCache.h"
#include "daemon/network/http/HttpClient.h"
#include "daemon/network/TlsContexts.h"

using namespace network;
using namespace network::http;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

// Self-signed, valid ~100 years. A: CN/SAN localhost + 127.0.0.1. B: SAN other.example only.
constexpr const char* CERT_A = R"PEM(-----BEGIN CERTIFICATE-----
MIIBejCCASCgAwIBAgIUYo1kV6gxncYMZM2tK/x1D85Is0cwCgYIKoZIzj0EAwIw
FDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MTExMTA2NDAwMloYDzIxMjYxMDE4
MDY0MDAyWjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQ8EwVMyjBgNsSZM+tF6vetIxJywZNpiiUy+qr56yHmt8ImxGWlRKlI
BkigrzjWNrevuD6wEtqrO5A2ZF0/DINDo04wTDAdBgNVHQ4EFgQUkrifYtDZaDgk
E8cUC/bC5ukLT/gwDwYDVR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgglsb2NhbGhv
c3SHBH8AAAEwCgYIKoZIzj0EAwIDSAAwRQIhAOd1DIoC90lugyee8A3VxTkuORvB
xx/J7ewas8BG5EBaAiA6/N60082yZkydR7KHq6LVO2T8cZP92wJOOQpZhcyLgg==
-----END CERTIFICATE-----
)PEM";

constexpr const char* KEY_A = R"PEM(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgYHaYG3QfKWuGGpYz
76EJUbMVVQga3qPNWjYeWJ1EqeGhRANCAAQ8EwVMyjBgNsSZM+tF6vetIxJywZNp
iiUy+qr56yHmt8ImxGWlRKlIBkigrzjWNrevuD6wEtqrO5A2ZF0/DIND
-----END PRIVATE KEY-----
)PEM";

constexpr const char* CERT_B = R"PEM(-----BEGIN CERTIFICATE-----
MIIBfzCCASagAwIBAgIUAeqs26iIrzjPOmByULyUGvC13UwwCgYIKoZIzj0EAwIw
GDEWMBQGA1UEAwwNb3RoZXIuZXhhbXBsZTAgFw0yNjExMTEwNjQwMDJaGA8yMTI2
MTAxODA2NDAwMlowGDEWMBQGA1UEAwwNb3RoZXIuZXhhbXBsZTBZMBMGByqGSM49
AgEGCCqGSM49AwEHA0IABNxxcnFcuEfOSefQUYE4Nf/rztAKjK2zkDEgmCDvhysM
nHEl69B/GOBrPCcEHMvCPFTZLj6oDHEiW5MZGbvEBY+jTDBKMB0GA1UdDgQWBBQx
6TllUQ8KX5jZUcoAdu5Kco1lZTAPBgNVHRMBAf8EBTADAQH/MBgGA1UdEQQRMA+C
DW90aGVyLmV4YW1wbGUwCgYIKoZIzj0EAwIDRwAwRAIgGD2LIDZpYfI3S9KNV9HA
opS6PqM9dmUnqdAjb6d9YccCIETekbB2nTjosWhhnpVoBw2US7lcLS3DXDEjQAUU
VYSH
-----END CERTIFICATE-----
)PEM";

constexpr const char* KEY_B = R"PEM(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgdif78qIv0UkHBUui
49w287x1hBw5kqjD9WmcT+4oOPehRANCAATccXJxXLhHzknn0FGBODX/687QCoyt
s5AxIJgg74crDJxxJevQfxjgazwnBBzLwjxU2S4+qAxxIluTGRm7xAWP
-----END PRIVATE KEY-----
)PEM";

template <typename Awaitable>
auto RunBlocking(asio::io_context& ctx, Awaitable&& awaitable)
{
	ctx.restart();
	auto fut = asio::co_spawn(ctx, std::forward<Awaitable>(awaitable), asio::use_future);
	std::thread stopper{[&ctx, &fut] { fut.wait(); ctx.stop(); }};
	ctx.run();
	stopper.join();
	return fut.get();
}

std::string WriteTemp(const char* name, const char* pem)
{
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream(path) << pem;
	return path.string();
}

/// Accepts one TLS client, answers one request with a fixed 200, ignores handshake failures.
Awaitable<void> ServeTls(tcp::acceptor& acceptor, ssl::context& sslCtx, std::string reply)
{
	try
	{
		ssl::stream<tcp::socket> stream(co_await acceptor.async_accept(asio::use_awaitable), sslCtx);
		co_await stream.async_handshake(ssl::stream_base::server, asio::use_awaitable);
		std::string request;
		co_await asio::async_read_until(stream, asio::dynamic_buffer(request), "\r\n\r\n", asio::use_awaitable);
		co_await asio::async_write(stream, asio::buffer(reply), asio::use_awaitable);
		boost::system::error_code ec;
		co_await stream.async_shutdown(asio::redirect_error(asio::use_awaitable, ec));
	}
	catch (const std::exception&)
	{
	}
}

struct TlsFixture
{
	asio::io_context ctx;
	ssl::context serverCtx{ssl::context::tls_server};
	tcp::acceptor acceptor{ctx, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)};

	TlsFixture(const char* serverCert, const char* serverKey)
	{
		serverCtx.use_certificate_chain(asio::buffer(std::string_view(serverCert)));
		serverCtx.use_private_key(asio::buffer(std::string_view(serverKey)), ssl::context::pem);
	}

	Result<HttpResponse> Get(bool certCheck, const std::string& certStore, bool redirectToHttp = false)
	{
		auto tls = std::make_shared<TlsContexts>(TlsContexts::Config{.certStore = certStore, .certCheck = certCheck});
		auto client = std::make_shared<HttpClient>(ctx.get_executor(), tls, std::make_shared<DnsCache>());
		const std::string reply = redirectToHttp
			? "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1/\r\nContent-Length: 0\r\n\r\n"
			: "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
		asio::co_spawn(ctx, ServeTls(acceptor, serverCtx, reply), asio::detached);
		return RunBlocking(ctx, client->Get(
			"https://127.0.0.1:" + std::to_string(acceptor.local_endpoint().port()) + "/", 3s));
	}
};

bool IsTlsFailure(Error e)
{
	return e == Error::TlsVerifyFailed || e == Error::TlsHandshakeFailed;
}

}

BOOST_AUTO_TEST_CASE(HttpClientTls_TrustedCertificateAndMatchingIp_Succeeds)
{
	TlsFixture f(CERT_A, KEY_A);
	auto r = f.Get(true, WriteTemp("nzbget_test_a.pem", CERT_A));
	BOOST_REQUIRE_MESSAGE(r.has_value(), ToString(r.error()));
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_AUTO_TEST_CASE(HttpClientTls_UntrustedCertificate_Fails)
{
	TlsFixture f(CERT_A, KEY_A);
	auto r = f.Get(true, WriteTemp("nzbget_test_b.pem", CERT_B));
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(IsTlsFailure(r.error()));
}

BOOST_AUTO_TEST_CASE(HttpClientTls_TrustedCertificateForOtherHostname_Fails)
{
	TlsFixture f(CERT_B, KEY_B);
	auto r = f.Get(true, WriteTemp("nzbget_test_b.pem", CERT_B));
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(IsTlsFailure(r.error()));
}

BOOST_AUTO_TEST_CASE(HttpClientTls_CertCheckDisabled_AcceptsUntrusted)
{
	TlsFixture f(CERT_B, KEY_B);
	auto r = f.Get(false, "");
	BOOST_REQUIRE_MESSAGE(r.has_value(), ToString(r.error()));
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_AUTO_TEST_CASE(HttpClient_RedirectHttpsToHttpRejected)
{
	TlsFixture f(CERT_A, KEY_A);
	auto r = f.Get(false, "", /*redirectToHttp=*/true);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::HttpUnexpectedResponse);
}

BOOST_AUTO_TEST_SUITE_END()
