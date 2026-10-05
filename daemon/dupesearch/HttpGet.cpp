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

#include <atomic>
#include <fstream>
#include <set>
#include "HttpGet.h"
#include "WebDownloader.h"
#include "FileSystem.h"
#include "Options.h"

namespace
{
	Mutex g_mutex;
	std::set<WebDownloader*> g_active;
	std::atomic<bool> g_stopped{false};
	std::atomic<int> g_counter{0};
}

HttpGet::Reply HttpGet::Fetch(const std::string& url, const std::string& infoName, size_t maxBytes)
{
	Reply reply;
	if (g_stopped)
	{
		return reply;
	}

	std::string path = std::string(g_Options->GetTempDir()) + PATH_SEPARATOR + "dupesearch-" +
		std::to_string((int)Util::CurrentTime()) + "-" + std::to_string(++g_counter) + ".tmp";

	WebDownloader downloader;
	downloader.SetUrl(url.c_str());
	downloader.SetInfoName(infoName.c_str());
	downloader.SetOutputFilename(path.c_str());
	downloader.SetForce(true);
	downloader.SetRetry(false);

	{
		Guard guard(g_mutex);
		if (g_stopped)
		{
			return reply;
		}
		g_active.insert(&downloader);
	}
	WebDownloader::EStatus status = downloader.DownloadWithRedirects(5);
	{
		Guard guard(g_mutex);
		g_active.erase(&downloader);
	}

	reply.status = downloader.GetHttpStatus();
	if (status == WebDownloader::adFinished)
	{
		int64 size = FileSystem::FileSize(path.c_str());
		if (size >= 0 && (size_t)size <= maxBytes)
		{
			std::ifstream file(fs::u8path(path), std::ios::binary);
			reply.body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			reply.ok = file.good() || file.eof();
		}
	}
	FileSystem::DeleteFile(path.c_str());
	return reply;
}

bool HttpGet::Stopped()
{
	return g_stopped;
}

void HttpGet::Reset()
{
	g_stopped = false;
}

void HttpGet::StopAll()
{
	Guard guard(g_mutex);
	g_stopped = true;
	for (WebDownloader* downloader : g_active)
	{
		downloader->Stop();
	}
}
