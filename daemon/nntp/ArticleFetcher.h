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


#ifndef ARTICLEFETCHER_H
#define ARTICLEFETCHER_H

#include <atomic>
#include <vector>
#include "NString.h"
#include "Thread.h"

/*
 * Fetches and decodes a single yEnc article body into memory, outside the
 * regular download pipeline. Used by post-processing stream repair to pull
 * byte ranges from duplicate postings (see StreamRepairController and
 * option <DupeArticleFallback> value "stream").
 *
 * Connections come from the global server pool; server levels are walked
 * like the regular downloader's, but without its per-server retry rounds -
 * any failure just reports "this article is not available", which stream
 * repair treats as "the donor cannot supply this range".
 */
class NntpConnection;

class ArticleFetchLimits
{
public:
	// Usenet articles are normally well below this. The cap is deliberately
	// generous while still preventing an untrusted BODY response from growing
	// memory without bound.
	static constexpr int64 MaxDecodedBytes = 64LL * 1024 * 1024;
	static constexpr int64 MaxHeaderBytes = 64LL * 1024;
	static constexpr int64 MaxRawBytes = MaxDecodedBytes * 3 + MaxHeaderBytes;

	bool AddRawBytes(int64 bytes);
	bool HeaderWithinLimit(bool declaredRangeKnown) const;
	bool AddDecodedBytes(int64 bytes, int64 begin, int64 end, int64 fileSize);

private:
	int64 m_rawBytes = 0;
	int64 m_decodedBytes = 0;
};

class ArticleFetcher
{
public:
	struct FetchedArticle
	{
		std::vector<char> Data;	// decoded bytes
		int64 Offset = 0;		// decoded-stream offset (yEnc part begin - 1)
		int64 FileSize = 0;		// total decoded file size from "=ybegin size="
		bool Success = false;
		bool Retry = false;		// interrupted by quota; retry without blaming source
	};

	/* messageId must include the angle brackets (as stored in ArticleInfo) */
	FetchedArticle Fetch(const char* messageId, const std::vector<CString>& groups);

	void Stop();

private:
	std::atomic<bool> m_stopped{false};
	Mutex m_connectionMutex;
	NntpConnection* m_connection = nullptr;

	FetchedArticle FetchFromConnection(NntpConnection* connection,
		const char* messageId, const std::vector<CString>& groups);
	void ReleaseConnection(NntpConnection* connection, bool keepConnected);
	void AddServerStats(NntpConnection* connection);
};

#endif
