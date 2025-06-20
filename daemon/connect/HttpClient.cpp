/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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

#include "HttpClient.h"
#include "Util.h"

namespace Network
{
	namespace asio = boost::asio;
	using tcp = boost::asio::ip::tcp;

#ifndef DISABLE_TLS
	namespace ssl = boost::asio::ssl;
#endif

	HttpClient::HttpClient() noexcept(false)
		: m_context{}
		, m_resolver{ m_context }
	{
#ifndef DISABLE_TLS
		m_sslContext.set_default_verify_paths();
#endif
	}

	const std::string HttpClient::GetLocalIP() const
	{
		return m_localIP;
	}

	std::future<Response> HttpClient::GET(std::string host_)
	{
		return std::async(std::launch::async, [this, host = std::move(host_)]
			{
				auto endpoints = m_resolver.resolve(host, GetProtocol());
				auto socket = GetSocket();

				Connect(socket, endpoints, host);
				Write(socket, "GET", host);

#ifndef DISABLE_TLS
				OpenSSL::StopSSLThread();
#endif
				return MakeResponse(socket);
			}
		);
	}

	std::string HttpClient::GetProtocol() const
	{
#ifndef DISABLE_TLS
		return "https";
#else
		return "http";
#endif
	}

	Response HttpClient::MakeResponse(Socket& socket)
	{
		Response response;

		asio::streambuf buf;
		response.statusCode = ReadStatusCode(socket, buf);
		response.headers = ReadHeaders(socket, buf);
		response.body = ReadBody(socket, buf);

		return response;
	}

	void HttpClient::Connect(Socket& socket, const Endpoints& endpoints, const std::string& host)
	{
		asio::connect(socket.lowest_layer(), endpoints);
#ifndef DISABLE_TLS
		DoHandshake(socket, host);
#endif
		SaveLocalIP(socket);
	}

	void HttpClient::Write(Socket& socket, const std::string& method, const std::string& host)
	{
		asio::write(socket, asio::buffer(GetHeaders(method, host)));
	}

	std::string HttpClient::ReadBody(Socket& socket, boost::asio::streambuf& buf)
	{
		boost::system::error_code ec;
		asio::read_until(socket, buf, "\0", ec);
		if (ec)
		{
			throw std::runtime_error("Failed to read the response body: " + ec.message());
		}

		auto bodyBuf = buf.data();
		std::string body(asio::buffers_begin(bodyBuf), asio::buffers_end(bodyBuf));
		Util::Trim(body);

		return body;
	}

	Headers HttpClient::ReadHeaders(Socket& socket, boost::asio::streambuf& buf)
	{
		asio::read_until(socket, buf, "\r\n\r\n");

		std::istream stream(&buf);

		Headers headers;
		std::string line;
		while (std::getline(stream, line) && line != "\r")
		{
			size_t colonPos = line.find(":");
			if (colonPos != std::string::npos)
			{
				std::string key = line.substr(0, colonPos);
				std::string value = line.substr(colonPos + 1);
				Util::Trim(value);
				headers.emplace(std::move(key), std::move(value));
			}
		}

		return headers;
	}

	unsigned HttpClient::ReadStatusCode(Socket& socket, boost::asio::streambuf& buf)
	{
		asio::read_until(socket, buf, "\r\n");

		std::istream stream(&buf);

		std::string httpVersion;
		unsigned statusCode;

		stream >> httpVersion;
		stream >> statusCode;

		if (!stream || httpVersion.find("HTTP/") == std::string::npos)
		{
			throw std::runtime_error("Invalid response");
		}

		return statusCode;
	}

	std::string HttpClient::GetHeaders(const std::string& method, const std::string& host) const
	{
		std::string headers = method + " / HTTP/1.1\r\n";
		headers += "Host: " + host + "\r\n";
		headers += std::string("User-Agent: nzbget/") + Util::VersionRevision() + "\r\n";
#ifndef DISABLE_GZIP
		headers += "Accept-Encoding: gzip\r\n";
#endif
		headers += "Accept: */*\r\n";
		headers += "Connection: close\r\n\r\n";

		return headers;
	}

	void HttpClient::SaveLocalIP(Socket& socket)
	{
		m_localIP = socket.lowest_layer().local_endpoint().address().to_string();
	}

#ifndef DISABLE_TLS
	Socket HttpClient::GetSocket()
	{
		return ssl::stream<tcp::socket>{ m_context, m_sslContext };
	}

	void HttpClient::DoHandshake(Socket& socket, const std::string& host)
	{
		if (!SSL_set_tlsext_host_name(socket.native_handle(), host.c_str()))
		{
			throw std::runtime_error("Failed to configure SNI TLS extension.");
		}

		socket.handshake(ssl::stream_base::handshake_type::client);
	}
#else
	Socket HttpClient::GetSocket()
	{
		return tcp::socket{ m_context };
	}
#endif

}
