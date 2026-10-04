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


#ifndef HTTPGET_H
#define HTTPGET_H

#include <string>

/*
 * A blocking HTTP GET for the duplicate search (an indexer's search and its
 * nzb-files), built on the downloader nzbget uses for urls and feeds, so it
 * follows redirects and honors options <UrlTimeout> and the TLS settings.
 * The url is never logged: only the name the caller gives, which must not
 * contain the api key. Callable from any thread; StopAll() ends the requests
 * in flight at shutdown and makes later ones fail at once.
 */
class HttpGet
{
public:
	struct Reply
	{
		bool ok = false;		// 200 and the body was read
		int status = 0;			// the HTTP status code of the last response, 0 if none
		std::string body;
	};

	static Reply Fetch(const std::string& url, const std::string& infoName, size_t maxBytes);
	static void StopAll();
	/* allows requests again (nzbget reloads inside the same process) */
	static void Reset();
	static bool Stopped();
};

#endif
