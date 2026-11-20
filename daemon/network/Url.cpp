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

#include "Url.h"
#include "Util.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace network
{

namespace
{

constexpr Error BadUrl = Error::LocalFailure;

bool HasIllegalChar(std::string_view s)
{
	return std::any_of(s.begin(), s.end(), [](unsigned char c) { return c <= 0x20 || c == 0x7F; });
}

/** RFC 3986 section 5.2.4: strips "." and ".." segments from an absolute path. */
std::string RemoveDotSegments(std::string_view input)
{
	std::string out;
	const auto dropLastSegment = [](std::string& path)
	{
		const auto slash = path.rfind('/');
		path.resize(slash == std::string::npos ? 0 : slash);
	};

	while (!input.empty())
	{
		if (input.starts_with("../"))
		{
			input.remove_prefix(3);
		}
		else if (input.starts_with("./"))
		{
			input.remove_prefix(2);
		}
		else if (input.starts_with("/./"))
		{
			input.remove_prefix(2);
		}
		else if (input == "/.")
		{
			input = "/";
		}
		else if (input.starts_with("/../"))
		{
			input.remove_prefix(3);
			dropLastSegment(out);
		}
		else if (input == "/..")
		{
			input = "/";
			dropLastSegment(out);
		}
		else if (input == "." || input == "..")
		{
			input = {};
		}
		else
		{
			const auto next = input.find('/', 1);
			out.append(input.substr(0, next));
			input = next == std::string_view::npos ? std::string_view{} : input.substr(next);
		}
	}
	return out;
}

/** Normalizes the path part and keeps the query untouched. */
void SetTarget(Url& url, std::string_view pathAndQuery)
{
	const auto query = pathAndQuery.find('?');
	url.target = RemoveDotSegments(pathAndQuery.substr(0, query));
	if (query != std::string_view::npos)
	{
		url.target.append(pathAndQuery.substr(query));
	}
}

Result<Url> ParseAuthority(bool tls, std::string_view authority, std::string_view rest)
{
	Url url;
	url.tls = tls;
	url.port = tls ? 443 : 80;

	std::string_view portText;
	if (!authority.empty() && authority.front() == '[')
	{
		const auto close = authority.find(']');
		if (close == std::string_view::npos || close == 1)
		{
			return BadUrl;
		}
		url.host = authority.substr(1, close - 1);
		authority.remove_prefix(close + 1);
		if (!authority.empty())
		{
			if (authority.front() != ':')
			{
				return BadUrl;
			}
			portText = authority.substr(1);
			if (portText.empty())
			{
				return BadUrl;
			}
		}
	}
	else
	{
		const auto colon = authority.find(':');
		url.host = authority.substr(0, colon);
		if (url.host.find_first_of("[]") != std::string::npos)
		{
			return BadUrl;
		}
		if (colon != std::string_view::npos)
		{
			portText = authority.substr(colon + 1);
			if (portText.empty())
			{
				return BadUrl;
			}
		}
	}

	if (url.host.empty())
	{
		return BadUrl;
	}
	if (!portText.empty())
	{
		int port = 0;
		const auto [end, ec] = std::from_chars(portText.data(), portText.data() + portText.size(), port);
		if (ec != std::errc{} || end != portText.data() + portText.size() || port < 1 || port > 65535)
		{
			return BadUrl;
		}
		url.port = port;
	}

	std::transform(url.host.begin(), url.host.end(), url.host.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	rest = rest.substr(0, rest.find('#'));
	std::string target{rest};
	if (target.empty() || target.front() == '?')
	{
		target.insert(0, 1, '/');
	}
	SetTarget(url, target);
	return url;
}

}

std::string Url::HostHeader() const
{
	std::string out = host.find(':') != std::string::npos ? "[" + host + "]" : host;
	if (port != (tls ? 443 : 80))
	{
		out += ':' + std::to_string(port);
	}
	return out;
}

Result<Url> ParseUrl(std::string_view text)
{
	if (HasIllegalChar(text))
	{
		return BadUrl;
	}

	bool tls;
	if (text.size() > 8 && Util::StrCaseCmp(text.substr(0, 8), "https://"))
	{
		tls = true;
		text.remove_prefix(8);
	}
	else if (text.size() > 7 && Util::StrCaseCmp(text.substr(0, 7), "http://"))
	{
		tls = false;
		text.remove_prefix(7);
	}
	else
	{
		return BadUrl;
	}

	const auto end = text.find_first_of("/?#");
	const auto authority = text.substr(0, end);
	if (authority.find('@') != std::string_view::npos)
	{
		return BadUrl;
	}
	return ParseAuthority(tls, authority, end == std::string_view::npos ? std::string_view{} : text.substr(end));
}

Result<Url> ResolveUrl(const Url& base, std::string_view ref)
{
	if (ref.empty() || HasIllegalChar(ref))
	{
		return BadUrl;
	}
	const auto colon = ref.find(':');
	const auto firstDelim = ref.find_first_of("/?#");
	const bool hasScheme = colon != std::string_view::npos && colon > 0
		&& (firstDelim == std::string_view::npos || colon < firstDelim)
		&& std::isalpha(static_cast<unsigned char>(ref.front()));
	if (hasScheme)
	{
		return ParseUrl(ref); // rejects every scheme except http(s)
	}
	if (ref.starts_with("//"))
	{
		return ParseUrl(std::string(base.tls ? "https:" : "http:") + std::string(ref));
	}

	Url url = base;
	ref = ref.substr(0, ref.find('#'));
	if (ref.empty())
	{
		return url;
	}
	if (ref.front() == '/')
	{
		SetTarget(url, ref);
	}
	else if (ref.front() == '?')
	{
		url.target = base.target.substr(0, base.target.find('?')) + std::string(ref);
	}
	else
	{
		const auto path = base.target.substr(0, base.target.find('?'));
		const std::string merged = path.substr(0, path.rfind('/') + 1) + std::string(ref);
		SetTarget(url, merged);
	}
	return url;
}

}
