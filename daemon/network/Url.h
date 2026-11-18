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

#ifndef NETWORK_URL_H
#define NETWORK_URL_H

#include <string>
#include <string_view>

#include "Error.h"

namespace network
{

/** An absolute http(s) URL split into what a client needs to dial and send a request. */
struct Url
{
	std::string host;	// lower-case; IPv6 literals without brackets
	std::string target;	// path + query, never empty, no fragment
	int port{0};
	bool tls{false};

	/** Value for the HTTP Host header: brackets around IPv6, port only when non-default. */
	std::string HostHeader() const;
};

/** Accepts http/https only; rejects userinfo, whitespace/control characters and bad ports. */
Result<Url> ParseUrl(std::string_view text);

/** Resolves a redirect Location (absolute, scheme-relative, absolute-path, relative or query-only) against base. */
Result<Url> ResolveUrl(const Url& base, std::string_view reference);

}

#endif // NETWORK_URL_H
