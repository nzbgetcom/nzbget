/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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

#include "Network.h"
#include "Util.h"
#include "Log.h"
#include "HttpClient.h"

namespace SystemInfo
{
	static const char* IP_SERVICE = "ip.nzbget.com";

	Network GetNetwork()
	{
		Network network{};

		try
		{
			auto httpClient = std::make_unique<HttpClient::HttpClient>();
			auto result = httpClient->GET(IP_SERVICE).get();
			if (result.statusCode == 200)
			{
				network.publicIP = std::move(result.body);
				network.privateIP = httpClient->GetLocalIP();
			}
		}
		catch (const std::exception& e)
		{
			warn("Failed to get public and private IP: %s", e.what());
		}

		return network;
	}
}
