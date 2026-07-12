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
#include <functional>
#include <memory>
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

/*
 * Fetches an ordered batch of donor articles concurrently: each worker owns
 * its own ArticleFetcher (and therefore its own pool connection), while the
 * consumer receives results strictly IN SUBMISSION ORDER, so serial consumer
 * semantics (drift measurement, size caps, early exit) are preserved.
 *
 * Workers run ahead of the consumer under a bounded window: at most
 * MaxWindowParts claimed-but-undelivered requests AND MaxBufferedBytes of
 * buffered decoded bytes (worst case memory = MaxBufferedBytes + workers x
 * ArticleFetchLimits::MaxDecodedBytes still in flight; a typical article is
 * well under 1 MB). With worker count 0 the fetch happens lazily inside
 * Next() on the caller's thread - byte-for-byte the serial behavior; the
 * live repair pass uses that so it never competes with the active download
 * for pool connections beyond what the serial code already did.
 *
 * The consumer API (SetWorkerCount/Begin/Next/CancelRemaining) is
 * single-threaded: one consumer drives one batch at a time. Stop() may be
 * called from any thread. Requests own their group list (shared per donor
 * file), so worker fetches stay safe regardless of the caller's object
 * lifetimes - CancelRemaining does not quiesce in-flight workers.
 */
class ArticleBatchFetcher
{
public:
	struct Request
	{
		CString MessageId;
		std::shared_ptr<const std::vector<CString>> Groups;
	};
	using FetchFunc = std::function<ArticleFetcher::FetchedArticle(const Request&)>;

	static constexpr int MaxWindowParts = 8;
	static constexpr int64 MaxBufferedBytes = 128LL * 1024 * 1024;

	// defined in the .cpp, after Worker is complete: std::vector<unique_ptr<Worker>>
	// needs the full type available to generate exception-unwind cleanup here
	ArticleBatchFetcher();
	// test seam: a blocking FetchFunc is NOT interruptible by Stop() (unlike a
	// real ArticleFetcher), so tests must release their gates before destruction
	// - the destructor joins the workers
	explicit ArticleBatchFetcher(FetchFunc fetchFunc);
	~ArticleBatchFetcher();

	void SetWorkerCount(int workerCount) { m_workerCount = workerCount; }
	void Begin(std::vector<Request> requests);
	bool Next(ArticleFetcher::FetchedArticle& result);
	void CancelRemaining();
	void Stop();

private:
	class Worker;

	struct Slot
	{
		ArticleFetcher::FetchedArticle Result;
		bool Ready = false;
	};

	Mutex m_mutex;
	std::vector<Request> m_requests;
	std::vector<Slot> m_slots;
	size_t m_nextClaim = 0;
	size_t m_nextDeliver = 0;
	int64 m_bufferedBytes = 0;
	int m_batchId = 0;
	bool m_cancelled = false;
	std::atomic<bool> m_stopped{false};
	int m_workerCount = 0;
	std::vector<std::unique_ptr<Worker>> m_workers;
	FetchFunc m_fetchFunc;			// test seam; production uses real fetchers
	ArticleFetcher m_inlineFetcher;	// worker count 0 (live mode)

	void WorkerLoop(ArticleFetcher& fetcher);
	bool ClaimNext(int batchId, size_t& index, Request& request);
	void StoreResult(int batchId, size_t index, ArticleFetcher::FetchedArticle&& result);
	void EnsureWorkers();
	friend class Worker;
};

#endif
