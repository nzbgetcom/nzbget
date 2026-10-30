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
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Log.h"
#include "Options.h"
#include "ServerPool.h"
#include "StatMeter.h"
#include "WorkState.h"
#include "YEncDecryptor.h"
#include "ArticleWriter.h"
#include "YEncTestHelpers.h"
#include "DirectRenamer.h"

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <functional>
#include <chrono>

// ArticleWriter.h (already included) declares ArticleCache and the
// g_ArticleCache global. The cache-backed scenario below installs a real
// ArticleCache so the commit path exercises the segment-cache sink.

namespace stdfs = std::filesystem;

#include <sodium.h>

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif // !WIN32

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(CorruptedLine1BootstrapFailClosedTest)
{
	YEncDecryptor decryptor("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);

	// Corrupted line 1 that does not begin with =ybegin and cannot be decrypted as FF1 control line
	std::string corruptedArticle =
		"4b376d5839704c32715238764e34775a99999999CORRUPT_LINE_ONE_DATA\r\n"
		"=ypart line=1 size=10\r\n"
		"data\r\n"
		"=yend size=4\r\n.\r\n";

	decoder.DecodeBuffer(corruptedArticle.data(), static_cast<int>(corruptedArticle.size()));
	Decoder::EStatus checkStatus = decoder.Check();
	BOOST_CHECK_EQUAL(static_cast<int>(checkStatus), static_cast<int>(Decoder::dsAuthFailed));
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

#ifndef WIN32

// Real local NNTP server driving ArticleDownloader::Run() over loopback
// sockets. Proves: corrupt-primary/valid-backup provider failover over real
// NNTP, DirectWrite on/off, allocated article cache, and that a failed
// provider's bytes never reach the article cache or final file (zero-output
// at every sink). Replaces the former decoder-only failover assertion.
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace
{

class LocalNntpServer
{
public:
	using RequestHandler = std::function<void(int clientFd, const std::string& cmd, int requestNum)>;

	LocalNntpServer(std::string responseArticle, std::atomic<int>* requestCounter) :
		m_responseArticle(std::move(responseArticle)),
		m_requestCounter(requestCounter)
	{
		Init();
	}

	LocalNntpServer(RequestHandler handler, std::atomic<int>* requestCounter) :
		m_requestCounter(requestCounter),
		m_handler(std::move(handler))
	{
		Init();
	}

	~LocalNntpServer()
	{
		m_stop = true;
		if (m_listenFd >= 0)
		{
			close(m_listenFd);
		}
		if (m_thread.joinable())
		{
			m_thread.join();
		}
	}

	int GetPort() const { return m_port; }

private:
	int m_listenFd = -1;
	int m_port = 0;
	std::string m_responseArticle;
	std::atomic<int>* m_requestCounter;
	RequestHandler m_handler;
	std::atomic<bool> m_stop{false};
	std::thread m_thread;

	void Init()
	{
		m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
		int opt = 1;
		setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0;
		if (bind(m_listenFd, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(m_listenFd, 5) != 0)
		{
			// Test infrastructure failure: cannot start local NNTP server
			m_port = -1;
			m_thread = std::thread([this]() { Run(); });
			return;
		}
		socklen_t len = sizeof(addr);
		getsockname(m_listenFd, (sockaddr*)&addr, &len);
		m_port = ntohs(addr.sin_port);
		m_thread = std::thread([this]() { Run(); });
	}

	void Run()
	{
		while (!m_stop)
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(m_listenFd, &fds);
			timeval tv = {0, 50000};
			if (select(m_listenFd + 1, &fds, nullptr, nullptr, &tv) <= 0)
			{
				continue;
			}
			int clientFd = accept(m_listenFd, nullptr, nullptr);
			if (clientFd < 0)
			{
				continue;
			}
			const char welcome[] = "200 Welcome test NNTP\r\n";
			send(clientFd, welcome, sizeof(welcome) - 1, MSG_NOSIGNAL);
			char buf[4096];
			std::string lineBuf;
			while (!m_stop)
			{
				int r = recv(clientFd, buf, sizeof(buf), 0);
				if (r <= 0) break;
				lineBuf.append(buf, r);
				size_t nl;
				while ((nl = lineBuf.find('\n')) != std::string::npos)
				{
					std::string cmd = lineBuf.substr(0, nl);
					lineBuf.erase(0, nl + 1);
					while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == ' '))
					{
						cmd.pop_back();
					}
					if (cmd.rfind("GROUP", 0) == 0)
					{
						const char resp[] = "211 0 0 0 test.group\r\n";
						send(clientFd, resp, sizeof(resp) - 1, MSG_NOSIGNAL);
					}
					else if (cmd.rfind("BODY", 0) == 0 || cmd.rfind("ARTICLE", 0) == 0)
					{
						int reqNum = ++(*m_requestCounter);
						if (m_handler)
						{
							m_handler(clientFd, cmd, reqNum);
						}
						else
						{
							std::string resp = "222 1 " + cmd.substr(cmd.find(' ') + 1) + "\r\n";
							resp += m_responseArticle;
							send(clientFd, resp.data(), resp.size(), MSG_NOSIGNAL);
						}
					}
					else if (cmd == "QUIT")
					{
						const char resp[] = "205 Bye\r\n";
						send(clientFd, resp, sizeof(resp) - 1, MSG_NOSIGNAL);
						break;
					}
					else
					{
						const char resp[] = "500 Command unrecognized\r\n";
						send(clientFd, resp, sizeof(resp) - 1, MSG_NOSIGNAL);
					}
				}
			}
			close(clientFd);
		}
	}
};

struct DownloaderEnv
{
	Options::CmdOptList cmdOpts;
	std::unique_ptr<Options> options;
	Options* oldOptions = nullptr;
	std::unique_ptr<StatMeter> statMeter;
	StatMeter* oldStatMeter = nullptr;
	std::unique_ptr<ArticleCache> articleCache;
	// Captured at construction; the destructor restores it so the framework
	// teardown never sees the test-local pool (which dies with the test).
	ServerPool* oldServerPool = nullptr;
	ArticleCache* oldArticleCache = nullptr;

	explicit DownloaderEnv(const std::string& tempDir, bool directWrite = false, int articleCache = 0,
		bool rawArticle = false)
	{
		oldServerPool = g_ServerPool;
		oldOptions = g_Options;
		oldArticleCache = g_ArticleCache;
		std::string tempDirOpt = "TempDir=" + tempDir;
		// cmdOpts stores raw const char* pointers: every string backing them
		// must outlive the Options constructor (a cacheOpt scoped inside this
		// if-block made Options read freed memory → the cache-backed leg
		// silently ran with ArticleCache=0).
		std::string cacheOpt;
		std::string rawArticleOpt;
		// Pin the read-chunk size so the PAR2-leg Append pattern (first chunk,
		// then remainder) is deterministic across hosts.
		cmdOpts.push_back("ArticleReadChunkSize=4");
		cmdOpts.push_back("WriteLog=none");
		cmdOpts.push_back(tempDirOpt.c_str());
		if (directWrite)
		{
			cmdOpts.push_back("DirectWrite=yes");
		}
		if (articleCache > 0)
		{
			cacheOpt = "ArticleCache=" + std::to_string(articleCache);
			cmdOpts.push_back(cacheOpt.c_str());
		}
		if (rawArticle)
		{
			rawArticleOpt = "RawArticle=yes";
			cmdOpts.push_back(rawArticleOpt.c_str());
		}
		options = std::make_unique<Options>(&cmdOpts, nullptr);
		g_Options = options.get();
		oldStatMeter = g_StatMeter;
		statMeter = std::make_unique<StatMeter>();
		g_StatMeter = statMeter.get();
		// WorkState defaults to TempPauseDownload=true; the downloader would
		// break with adRetry before touching the network.
		g_WorkState->SetTempPauseDownload(false);
		// The production binary owns the ArticleCache in nzbget.cpp; the test
		// binary has no global cache, so a cache-backed scenario must install
		// one — otherwise the writer silently falls back to disk staging and
		// the "segment lands in ArticleCache" leg would be a tautology.
		if (articleCache > 0)
		{
			this->articleCache = std::make_unique<ArticleCache>();
			g_ArticleCache = this->articleCache.get();
		}
		// StatMeter::Init sizes the per-server volume table from g_ServerPool;
		// the test must set g_ServerPool BEFORE constructing this env.
		statMeter->Init();
	}

	~DownloaderEnv()
	{
		// Options::~Options() nulls g_Options unconditionally, so the test
		// Options instance must be destroyed BEFORE restoring the global —
		// otherwise the restored pointer is nulled right after restoration.
		options.reset();
		statMeter.reset();
		articleCache.reset();
		g_ArticleCache = oldArticleCache;
		g_Options = oldOptions;
		g_StatMeter = oldStatMeter;
		// g_ServerPool is restored to the framework's global pool; the test
		// pool is a test-local stack object and must never outlive the test.
		g_ServerPool = oldServerPool;
		g_WorkState->SetTempPauseDownload(true);
	}
};

// Shared test helpers : one hex/yEnc helper set for all NNTP test TUs.
using namespace YEncTest;

// Builds a valid combined-mode encrypted wire article for a single-part segment.
// password/salt/segmentIndex/nonce/key all come from YEncDecryptor's normal
// derivation so the resulting article decrypts with the same inputs.
std::string BuildValidEncryptedWireArticle(const std::string& password,
	const std::vector<uint8_t>& salt, uint32_t segmentIndex,
	const std::string& filename, const std::vector<uint8_t>& plaintext)
{
	YEncDecryptor encDec(password);
	BOOST_REQUIRE(encDec.EnsureMasterKey(salt.data()));

	uint8_t nonce[24];
	BOOST_REQUIRE(encDec.DeriveBodyNonce(segmentIndex, nonce));

	std::vector<uint8_t> aad = YEncDecryptor::BuildAad(filename, 1, 1, plaintext.size());
	std::vector<uint8_t> sealed(plaintext.size() + 16);
	unsigned long long sealedLen = 0;
	BOOST_REQUIRE(crypto_aead_xchacha20poly1305_ietf_encrypt(
		sealed.data(), &sealedLen, plaintext.data(), plaintext.size(),
		aad.data(), aad.size(), nullptr, nonce,
		encDec.GetCachedMasterKey().data()) == 0);
	sealed.resize(sealedLen);

	std::vector<uint8_t> ct(sealed.begin(), sealed.end() - 16);
	std::vector<uint8_t> tag(sealed.end() - 16, sealed.end());

	std::string line1Plain = "=ybegin line=128 size=" + std::to_string(plaintext.size()) + " name=" + filename;
	std::vector<uint8_t> wire1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wire1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	char tagHex[33] = {0};
	for (size_t i = 0; i < 16; ++i)
	{
		snprintf(tagHex + 2 * i, 3, "%02x", tag[i]);
	}
	std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=" + YEncTest::BinToHex(salt.data(), 16) +
		" index=" + YEncTest::FormatHex8(segmentIndex) + " tag=" + tagHex;
	std::vector<uint8_t> wire2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wire2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	// yEnc-encode the ciphertext (with full '='-escape handling)
	std::string encLine;
	for (uint8_t c : ct)
	{
		uint8_t e = (uint8_t)((c + 42) % 256);
		if (e == 0x00 || e == 0x0A || e == 0x0D || e == 0x3D)
		{
			encLine += '=';
			e = (uint8_t)((e + 64) % 256);
		}
		encLine += (char)e;
	}

	Crc32 crc;
	crc.Append(ct.data(), static_cast<uint32>(ct.size()));
	std::string line4Plain = "=yend size=" + std::to_string(plaintext.size()) +
		" crc32=" + YEncTest::FormatHex8(crc.Finish());
	std::vector<uint8_t> wire4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
		segmentIndex, 4, false, salt.data(), wire4
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string fullWire;
	fullWire.append(reinterpret_cast<const char*>(wire1.data()), wire1.size()); fullWire += "\r\n";
	fullWire.append(reinterpret_cast<const char*>(wire2.data()), wire2.size()); fullWire += "\r\n";
	fullWire += encLine; fullWire += "\r\n";
	fullWire.append(reinterpret_cast<const char*>(wire4.data()), wire4.size()); fullWire += "\r\n.\r\n";
	return fullWire;
}

// Builds an ordinary (unencrypted) yEnc wire article for a single-part segment.
std::string BuildValidOrdinaryYencWireArticle(const std::string& filename,
	const std::vector<uint8_t>& plaintext)
{
	std::string line1 = "=ybegin line=128 size=" + std::to_string(plaintext.size()) + " name=" + filename;
	std::string encLine;
	for (uint8_t c : plaintext)
	{
		uint8_t e = (uint8_t)((c + 42) % 256);
		if (e == 0x00 || e == 0x0A || e == 0x0D || e == 0x3D)
		{
			encLine += '=';
			e = (uint8_t)((e + 64) % 256);
		}
		encLine += (char)e;
	}
	Crc32 crc;
	crc.Append(const_cast<uint8_t*>(plaintext.data()), static_cast<uint32>(plaintext.size()));
	std::string line4 = "=yend size=" + std::to_string(plaintext.size()) +
		" crc32=" + YEncTest::FormatHex8(crc.Finish());
	return line1 + "\r\n" + encLine + "\r\n" + line4 + "\r\n.\r\n";
}

// Recording analyzer capturing every Append() so tests can assert exactly
// which bytes reach the content analyzer (the DirectRenamer feed seam).
class RecordingContentAnalyzer final : public ArticleContentAnalyzer
{
public:
	void Reset() override
	{
		m_recorded.clear();
		m_resetCount++;
	}

	void Append(const void* buffer, int len) override
	{
		const char* bytes = static_cast<const char*>(buffer);
		m_recorded.insert(m_recorded.end(), bytes, bytes + len);
		m_appendCount++;
	}

	const std::vector<uint8_t>& GetRecorded() const { return m_recorded; }
	int GetAppendCount() const { return m_appendCount; }
	int GetResetCount() const { return m_resetCount; }

private:
	std::vector<uint8_t> m_recorded;
	int m_appendCount = 0;
	int m_resetCount = 0;
};

// Full corrupt-primary → valid-backup scenario over real loopback NNTP.
// directWrite / articleCache parameterize the writer path under test.
void RunEncryptedFailoverScenario(const char* filename, bool directWrite, int articleCache)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_failover_nntp_test";
	stdfs::create_directories(tempDir);
	stdfs::path tempDirOpt = stdfs::temp_directory_path() / "nzbget_failover_nntp_temp";
	stdfs::create_directories(tempDirOpt);

	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> plaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	const uint32_t segmentIndex = 1;

	std::string validWire = BuildValidEncryptedWireArticle(password, salt, segmentIndex, filename, plaintext);
	std::string corruptWire = validWire;
	// Flip one byte in the yEnc-encoded body so the AEAD tag no longer matches.
	size_t bodyStart = corruptWire.find("\r\n", corruptWire.find("\r\n") + 2) + 2;
	corruptWire[bodyStart] = (char)(corruptWire[bodyStart] == 'A' ? 'B' : 'A');

	std::atomic<int> primaryRequests{0};
	std::atomic<int> backupRequests{0};
	LocalNntpServer primary(corruptWire, &primaryRequests);
	LocalNntpServer backup(validWire, &backupRequests);

	// Two servers: primary at level 0 (fails), backup at level 1 (succeeds)
	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", primary.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	auto server2 = std::make_unique<NewsServer>(2, true, "backup", "127.0.0.1", backup.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 1, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.AddServer(std::move(server2));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt.string(), directWrite, articleCache);
	g_ServerPool = &pool;
	// StatMeter::Init sizes the per-server volume table from g_ServerPool; the
	// test pool must be installed first.
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", password.c_str());

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename(filename);
	fileInfo.SetTime(Util::CurrentTime());

	fileInfo.GetArticles()->resize(1);
	(*fileInfo.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo.GetArticles())[0]->SetMessageId("<failover-msg@test>");
	(*fileInfo.GetArticles())[0]->SetSize(static_cast<int>(plaintext.size()));
	(*fileInfo.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader;
	downloader.SetFileInfo(&fileInfo);
	downloader.SetArticleInfo((*fileInfo.GetArticles())[0].get());
	downloader.SetInfoName("failover_article");
	downloader.SetConnection(pool.GetConnection(0, nullptr, nullptr));

	auto recordingAnalyzer = std::make_unique<RecordingContentAnalyzer>();
	RecordingContentAnalyzer* analyzerPtr = recordingAnalyzer.get();
	downloader.SetContentAnalyzer(std::move(recordingAnalyzer));

	downloader.Run();

	// Both providers must have been contacted (failover happened over real NNTP)
	BOOST_CHECK_GE(primaryRequests.load(), 1);
	BOOST_CHECK_GE(backupRequests.load(), 1);

	// The downloader must succeed (valid article fetched from backup provider)
	BOOST_CHECK_EQUAL(static_cast<int>(downloader.GetStatus()), static_cast<int>(ArticleDownloader::adFinished));

	// The article CRC is the CRC of the authenticated plaintext (not the
	// ciphertext), so PAR2 quick verification can use it.
	Crc32 plaintextCrc;
	plaintextCrc.Append(const_cast<uint8_t*>(plaintext.data()), static_cast<uint32>(plaintext.size()));
	BOOST_CHECK_EQUAL((*fileInfo.GetArticles())[0]->GetCrc(), plaintextCrc.Finish());

	// The committed segment must be the authenticated plaintext (from backup),
	// never the corrupted primary's bytes. With ArticleCache=0 the plaintext is
	// written to the per-article result file; with a cache allocation it lands
	// in the segment cache. DirectWrite must be bypassed for encrypted releases.
	const char* content = (*fileInfo.GetArticles())[0]->GetSegmentContent();
	if (articleCache > 0)
	{
		// The cache-backed leg must genuinely use the segment-cache sink: the
		// env installs a real ArticleCache, so Alloc() must have succeeded.
		BOOST_REQUIRE(content != nullptr);
		BOOST_CHECK_EQUAL(memcmp(content, plaintext.data(), plaintext.size()), 0);
	}
	else if (content != nullptr)
	{
		BOOST_CHECK_EQUAL(memcmp(content, plaintext.data(), plaintext.size()), 0);
	}
	else
	{
		const char* resultFilename = (*fileInfo.GetArticles())[0]->GetResultFilename();
		BOOST_REQUIRE(resultFilename != nullptr);
		std::ifstream resultFile(resultFilename, std::ios::binary);
		BOOST_REQUIRE(resultFile);
		std::vector<char> fileBytes((std::istreambuf_iterator<char>(resultFile)),
			std::istreambuf_iterator<char>());
		BOOST_REQUIRE_EQUAL(fileBytes.size(), plaintext.size());
		BOOST_CHECK_EQUAL(memcmp(fileBytes.data(), plaintext.data(), plaintext.size()), 0);
	}

	// DirectWrite gating: with DirectWrite=on and an encrypted release, the
	// final destination file must never have been created by the writer.
	if (directWrite)
	{
		BOOST_CHECK(!stdfs::exists(tempDir / filename));
	}

	// T05 DL-2 regression: corrupt-primary/valid-backup run must record exactly
	// the valid plaintext once; the failed corrupt-primary attempt feeds zero bytes.
	BOOST_CHECK_GE(analyzerPtr->GetAppendCount(), 1);
	BOOST_REQUIRE_EQUAL(analyzerPtr->GetRecorded().size(), plaintext.size());
	BOOST_CHECK_EQUAL(memcmp(analyzerPtr->GetRecorded().data(), plaintext.data(), plaintext.size()), 0);

	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
}

// T05 DL-2 native repro result of one PAR2-first-article leg.
struct Par2FirstArticleResult
{
	bool hasHash16k = false;
	std::string hash16k;
	bool parFile = false;
	bool hasParSetId = false;
	std::string parSetId;
	size_t fedBytes = 0;
	bool fedMatchesPlaintext = false;
};

// DownloadQueue mock for DirectRenamer::ArticleDownloaded (SaveChanged only).
class Par2RenameDownloadQueueMock final : public DownloadQueue
{
public:
	Par2RenameDownloadQueueMock()
	{
		Init(this);
		Loaded();
	}
	~Par2RenameDownloadQueueMock()
	{
		Final();
	}
	bool EditEntry(int ID, EEditAction action, const char* args) override { return false; }
	bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) override { return false; }
	void HistoryChanged() override {}
	void Save() override {}
	void SaveChanged() override {}
};

// Minimal concrete DirectRenamer (RenameCompleted is pure virtual).
class Par2TestDirectRenamer final : public DirectRenamer
{
protected:
	void RenameCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo) override {}
};

// Runs one PAR2-first-article leg through the real ArticleDownloader plus the
// real RenameContentAnalyzer/DirectRenamer::ArticleDownloaded path (exactly the
// QueueCoordinator::StartArticleDownload/ArticleDownloaded wiring) and returns
// the resulting classification.
Par2FirstArticleResult RunPar2FirstArticleScenario(const char* filename, bool encrypted,
	const std::string& password, const std::vector<uint8_t>& salt, uint32_t segmentIndex,
	const std::vector<uint8_t>& plaintext)
{
	std::string tempDir = (stdfs::temp_directory_path() /
		(encrypted ? "nzbget_par2_enc_test" : "nzbget_par2_plain_test")).string();
	std::string tempDirOpt = (stdfs::temp_directory_path() /
		(encrypted ? "nzbget_par2_enc_temp" : "nzbget_par2_plain_temp")).string();
	stdfs::create_directories(tempDir);
	stdfs::create_directories(tempDirOpt);

	std::string wire;
	if (encrypted)
	{
		wire = BuildValidEncryptedWireArticle(password, salt, segmentIndex, filename, plaintext);
	}
	else
	{
		wire = BuildValidOrdinaryYencWireArticle(filename, plaintext);
	}

	std::atomic<int> requests{0};
	LocalNntpServer server(wire, &requests);

	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", server.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt, false, 0);
	g_ServerPool = &pool;
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetId(100);
	nzbInfo.SetDestDir(tempDir.c_str());
	nzbInfo.SetName(filename);
	nzbInfo.SetYEncEncrypted(encrypted);
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", password.c_str());

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetId(101);
	fileInfo.SetFilename(filename);
	fileInfo.SetTime(Util::CurrentTime());

	fileInfo.GetArticles()->resize(1);
	(*fileInfo.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo.GetArticles())[0]->SetMessageId(encrypted ? "<par2enc-msg@test>" : "<par2plain-msg@test>");
	(*fileInfo.GetArticles())[0]->SetSize(static_cast<int>(plaintext.size()));
	(*fileInfo.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader;
	downloader.SetFileInfo(&fileInfo);
	downloader.SetArticleInfo((*fileInfo.GetArticles())[0].get());
	downloader.SetInfoName("par2_first_article");
	downloader.SetConnection(pool.GetConnection(0, nullptr, nullptr));

	// Real rename analyzer, exactly like QueueCoordinator::StartArticleDownload.
	// A recording mirror wraps the real analyzer so the test can assert the
	// exact fed bytes; the real analyzer still drives ArticleDownloaded.
	Par2TestDirectRenamer renamer;
	auto renameAnalyzer = renamer.MakeArticleContentAnalyzer();
	auto mirrorAnalyzer = std::make_unique<RecordingContentAnalyzer>();
	RecordingContentAnalyzer* mirrorPtr = mirrorAnalyzer.get();
	class MirrorAnalyzer final : public ArticleContentAnalyzer
	{
	public:
		MirrorAnalyzer(ArticleContentAnalyzer* real, RecordingContentAnalyzer* mirror) :
			m_real(real), m_mirror(mirror) {}
		void Reset() override
		{
			m_real->Reset();
			m_mirror->Reset();
		}
		void Append(const void* buffer, int len) override
		{
			m_real->Append(buffer, len);
			m_mirror->Append(buffer, len);
		}
	private:
		ArticleContentAnalyzer* m_real;
		RecordingContentAnalyzer* m_mirror;
	};
	downloader.SetContentAnalyzer(std::make_unique<MirrorAnalyzer>(renameAnalyzer.get(), mirrorPtr));

	downloader.Run();

	BOOST_CHECK_EQUAL(static_cast<int>(downloader.GetStatus()), static_cast<int>(ArticleDownloader::adFinished));

	// The article CRC is the CRC of the authenticated plaintext: equality with
	// the fixture proves the committed/decrypted bytes are the real content.
	Crc32 fixtureCrc;
	fixtureCrc.Append(const_cast<uint8_t*>(plaintext.data()), static_cast<uint32>(plaintext.size()));
	BOOST_CHECK_EQUAL((*fileInfo.GetArticles())[0]->GetCrc(), fixtureCrc.Finish());

	// Drive the real ArticleDownloaded consumer. DirectRenameStatus is set to
	// tsFailure so CheckState returns early and never spins up the
	// DirectParLoader thread against this mock queue: the leg asserts only the
	// analyzer-driven classification (parFile/setId/hash16k).
	nzbInfo.SetDirectRenameStatus(NzbInfo::tsFailure);
	Par2RenameDownloadQueueMock downloadQueue;
	renamer.ArticleDownloaded(&downloadQueue, &fileInfo,
		(*fileInfo.GetArticles())[0].get(), renameAnalyzer.get());

	Par2FirstArticleResult out;
	out.hasHash16k = !Util::EmptyStr(fileInfo.GetHash16k());
	out.hash16k = out.hasHash16k ? fileInfo.GetHash16k() : "";
	out.parFile = fileInfo.GetParFile();
	out.hasParSetId = fileInfo.GetParFile() && !Util::EmptyStr(fileInfo.GetParSetId());
	out.parSetId = out.hasParSetId ? fileInfo.GetParSetId() : "";
	out.fedBytes = mirrorPtr->GetRecorded().size();
	out.fedMatchesPlaintext = mirrorPtr->GetRecorded().size() == plaintext.size() &&
		memcmp(mirrorPtr->GetRecorded().data(), plaintext.data(), plaintext.size()) == 0;

	// Both legs must receive the exact authenticated plaintext.
	BOOST_CHECK_EQUAL(out.fedBytes, plaintext.size());
	BOOST_CHECK(out.fedMatchesPlaintext);

	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
	return out;
}

} // anonymous namespace

BOOST_AUTO_TEST_CASE(EncryptedProviderFailoverOverRealNntpTest)
{
	// Real local NNTP: corrupt primary → valid backup failover. The primary
	// provider serves an article with a corrupted body (auth failure); the
	// backup serves the valid article. ArticleDownloader::Run() must finish
	// with adFinished and the committed plaintext must come only from the
	// backup provider.
	//
	// Runs the full Run() loop three times, covering the integration matrix:
	//   1. DirectWrite=off,  ArticleCache=0 (disk staging via result file)
	//   2. DirectWrite=on,   ArticleCache=0 (direct-write must be bypassed for
	//      encrypted releases; staged to result file instead)
	//   3. DirectWrite=off,  ArticleCache=250 (segment lands in the article cache)
	RunEncryptedFailoverScenario("failover_disk.dat", false, 0);
	RunEncryptedFailoverScenario("failover_directwrite.dat", true, 0);
	RunEncryptedFailoverScenario("failover_cached.dat", false, 250);
}

BOOST_AUTO_TEST_CASE(EncryptedPar2FirstArticleRenameAnalyzerTest)
{
	// T05 DL-2 native repro: an encrypted PAR2 first article through the real
	// rename analyzer/DirectRenamer path must classify identically to the
	// ordinary yEnc path: PAR2 flag, set ID and first-16-KiB hash.
	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const uint32_t segmentIndex = 1;

	// Real PAR2 file content (vendored test fixture, also copied for the
	// postprocess tests).
	stdfs::path par2Source;
	const std::vector<stdfs::path> candidates = {
		stdfs::path(__FILE__).parent_path() / ".." / "testdata" / "parchecker" / "testfile.par2",
		stdfs::current_path() / "parchecker" / "testfile.par2",
		stdfs::current_path() / "tests" / "testdata" / "parchecker" / "testfile.par2",
		stdfs::current_path() / "testdata" / "parchecker" / "testfile.par2"
	};
	for (const auto& candidate : candidates)
	{
		if (stdfs::exists(candidate))
		{
			par2Source = candidate;
			break;
		}
	}
	BOOST_REQUIRE(stdfs::exists(par2Source));
	std::ifstream par2In(par2Source, std::ios::binary);
	BOOST_REQUIRE(par2In);
	std::vector<uint8_t> plaintext((std::istreambuf_iterator<char>(par2In)),
		std::istreambuf_iterator<char>());
	BOOST_REQUIRE(!plaintext.empty());

	Par2FirstArticleResult plain = RunPar2FirstArticleScenario("par2_plain.par2", false,
		password, salt, segmentIndex, plaintext);
	Par2FirstArticleResult enc = RunPar2FirstArticleScenario("par2_enc.par2", true,
		password, salt, segmentIndex, plaintext);

	// Ordinary-path control: the ordinary yEnc leg must classify as PAR2 with
	// set ID and hash (proves the seam actually classifies).
	BOOST_CHECK(plain.parFile);
	BOOST_CHECK(plain.hasParSetId);
	BOOST_CHECK(plain.hasHash16k);

	// The encrypted leg must produce the identical classification: PAR2 flag,
	// set ID and first-16-KiB hash all equal the ordinary path.
	BOOST_CHECK_EQUAL(enc.parFile, plain.parFile);
	BOOST_CHECK_EQUAL(enc.hasParSetId, plain.hasParSetId);
	BOOST_CHECK_EQUAL(enc.parSetId, plain.parSetId);
	BOOST_CHECK_EQUAL(enc.hasHash16k, plain.hasHash16k);
	BOOST_CHECK_EQUAL(enc.hash16k, plain.hash16k);
}

BOOST_AUTO_TEST_CASE(EncryptedRawArticleRejectedBeforeRequestTest)
{
	// RawArticle + encrypted job must be rejected BEFORE the
	// BODY/ARTICLE request — no bytes fetched, nothing committed, raw status
	// adFatalError (Run() breaks immediately and normalizes to failure).
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_rawart_test";
	stdfs::path tempDirOpt = stdfs::temp_directory_path() / "nzbget_rawart_temp";
	stdfs::create_directories(tempDir);
	stdfs::create_directories(tempDirOpt);

	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> plaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string filename = "rawarticle.dat";

	std::string validWire = BuildValidEncryptedWireArticle(password, salt, 1, filename, plaintext);

	std::atomic<int> requests{0};
	LocalNntpServer server(validWire, &requests);

	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", server.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt.string(), false, 0, /*rawArticle=*/true);
	g_ServerPool = &pool;
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", password.c_str());

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename(filename);
	fileInfo.SetTime(Util::CurrentTime());

	fileInfo.GetArticles()->resize(1);
	(*fileInfo.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo.GetArticles())[0]->SetMessageId("<rawarticle-msg@test>");
	(*fileInfo.GetArticles())[0]->SetSize(static_cast<int>(plaintext.size()));
	(*fileInfo.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader;
	downloader.SetFileInfo(&fileInfo);
	downloader.SetArticleInfo((*fileInfo.GetArticles())[0].get());
	downloader.SetInfoName("rawarticle_article");
	downloader.SetConnection(pool.GetConnection(0, nullptr, nullptr));

	BOOST_REQUIRE(g_Options->GetRawArticle());

	// Drive Download() directly: Run() normalizes adFatalError to adFailed,
	// but the reviewer-required assertion is the raw pre-normalization status.
	ArticleDownloader::EStatus status = downloader.TestDownload();

	BOOST_CHECK_EQUAL(static_cast<int>(status), static_cast<int>(ArticleDownloader::adFatalError));

	// The rejection must precede the article request: no BODY/ARTICLE was sent.
	BOOST_CHECK_EQUAL(requests.load(), 0);

	// Nothing committed: no segment content, no result file, no destination file.
	BOOST_CHECK((*fileInfo.GetArticles())[0]->GetSegmentContent() == nullptr);
	BOOST_CHECK((*fileInfo.GetArticles())[0]->GetResultFilename() == nullptr ||
		!stdfs::exists((*fileInfo.GetArticles())[0]->GetResultFilename()));
	BOOST_CHECK(!stdfs::exists(tempDir / filename));

	g_ServerPool = env.oldServerPool;
	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
}

BOOST_AUTO_TEST_CASE(EncryptedEmptyPasswordFailsStructurallyTest)
{
	// Body Std v1.2 §5/§8: "no available password" is METADATA_VALIDATION — a
	// structural failure, never provider corruption. If the password of a
	// queued encrypted job becomes empty later (e.g. API editqueue of
	// *Unpack:Pass" "word), the article fetch must fail with adFatalError BEFORE
	// contacting any provider: empty password ⇒ no decryptor ⇒ the wire path
	// cannot even recognize the article. Retrying other providers is futile
	// (each would fail identically) and must not happen.
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_emptypwd_test";
	stdfs::path tempDirOpt = stdfs::temp_directory_path() / "nzbget_emptypwd_temp";
	stdfs::create_directories(tempDir);
	stdfs::create_directories(tempDirOpt);

	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> plaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string filename = "emptypwd.dat";

	std::string validWire = BuildValidEncryptedWireArticle(password, salt, 1, filename, plaintext);

	std::atomic<int> requests{0};
	LocalNntpServer server(validWire, &requests);

	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", server.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	auto server2 = std::make_unique<NewsServer>(2, true, "backup", "127.0.0.1", server.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 1, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.AddServer(std::move(server2));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt.string(), false, 0);
	g_ServerPool = &pool;
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);
	// The password was emptied after queueing (API edit); the encrypted flag
	// stays set — this is the exact scenario from the finding.
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", "");

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename(filename);
	fileInfo.SetTime(Util::CurrentTime());

	fileInfo.GetArticles()->resize(1);
	(*fileInfo.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo.GetArticles())[0]->SetMessageId("<emptypwd-msg@test>");
	(*fileInfo.GetArticles())[0]->SetSize(static_cast<int>(plaintext.size()));
	(*fileInfo.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader;
	downloader.SetFileInfo(&fileInfo);
	downloader.SetArticleInfo((*fileInfo.GetArticles())[0].get());
	downloader.SetInfoName("emptypwd_article");
	downloader.SetConnection(pool.GetConnection(0, nullptr, nullptr));

	// Drive Download() directly to observe the raw pre-normalization status.
	ArticleDownloader::EStatus status = downloader.TestDownload();

	// Structural failure: the job-level loop breaks on adFatalError without
	// trying the next provider (Run() later normalizes it to adFailed).
	BOOST_CHECK_EQUAL(static_cast<int>(status), static_cast<int>(ArticleDownloader::adFatalError));

	// The rejection must precede the article request: no BODY/ARTICLE was sent.
	BOOST_CHECK_EQUAL(requests.load(), 0);

	// Zero output: nothing committed to any sink.
	BOOST_CHECK((*fileInfo.GetArticles())[0]->GetSegmentContent() == nullptr);
	BOOST_CHECK(!stdfs::exists(tempDir / filename));

	g_ServerPool = env.oldServerPool;
	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
}

BOOST_AUTO_TEST_CASE(EncryptedAllProvidersExhaustedTest)
{
	// Exhaustion leg: BOTH providers serve the corrupted article. The
	// downloader must exhaust all levels/servers and end in failure
	// (adFailed/adNotFound after Run() normalization), with ZERO bytes
	// committed to any sink — cache, result file, or final destination.
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_exhaust_test";
	stdfs::path tempDirOpt = stdfs::temp_directory_path() / "nzbget_exhaust_temp";
	stdfs::create_directories(tempDir);
	stdfs::create_directories(tempDirOpt);

	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> plaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string filename = "exhaust.dat";

	std::string validWire = BuildValidEncryptedWireArticle(password, salt, 1, filename, plaintext);
	std::string corruptWire = validWire;
	size_t bodyStart = corruptWire.find("\r\n", corruptWire.find("\r\n") + 2) + 2;
	corruptWire[bodyStart] = (char)(corruptWire[bodyStart] == 'A' ? 'B' : 'A');

	std::atomic<int> primaryRequests{0};
	std::atomic<int> backupRequests{0};
	LocalNntpServer primary(corruptWire, &primaryRequests);
	LocalNntpServer backup(corruptWire, &backupRequests);

	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", primary.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	auto server2 = std::make_unique<NewsServer>(2, true, "backup", "127.0.0.1", backup.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 1, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.AddServer(std::move(server2));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt.string(), false, 0);
	g_ServerPool = &pool;
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", password.c_str());

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename(filename);
	fileInfo.SetTime(Util::CurrentTime());

	fileInfo.GetArticles()->resize(1);
	(*fileInfo.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo.GetArticles())[0]->SetMessageId("<exhaust-msg@test>");
	(*fileInfo.GetArticles())[0]->SetSize(static_cast<int>(plaintext.size()));
	(*fileInfo.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader;
	downloader.SetFileInfo(&fileInfo);
	downloader.SetArticleInfo((*fileInfo.GetArticles())[0].get());
	downloader.SetInfoName("exhaust_article");
	downloader.SetConnection(pool.GetConnection(0, nullptr, nullptr));

	auto recordingAnalyzer = std::make_unique<RecordingContentAnalyzer>();
	RecordingContentAnalyzer* analyzerPtr = recordingAnalyzer.get();
	downloader.SetContentAnalyzer(std::move(recordingAnalyzer));

	downloader.Run();

	// Both providers were contacted (each served the corrupted article) and
	// the loop exhausted all levels: final status is failure.
	BOOST_CHECK_GE(primaryRequests.load(), 1);
	BOOST_CHECK_GE(backupRequests.load(), 1);
	int finalStatus = static_cast<int>(downloader.GetStatus());
	BOOST_CHECK(finalStatus == static_cast<int>(ArticleDownloader::adFailed) ||
		finalStatus == static_cast<int>(ArticleDownloader::adNotFound));

	// Zero bytes committed to ANY sink:
	// - no segment in the article cache
	BOOST_CHECK((*fileInfo.GetArticles())[0]->GetSegmentContent() == nullptr);
	BOOST_CHECK_EQUAL((*fileInfo.GetArticles())[0]->GetSegmentSize(), 0);
	// - no result file / temp file on disk
	BOOST_CHECK(!stdfs::exists(tempDirOpt / "1.001"));
	BOOST_CHECK(!stdfs::exists(tempDirOpt / "1.001.tmp"));
	// - no final destination file
	BOOST_CHECK(!stdfs::exists(tempDir / filename));
	// - T05: content analyzer receives nothing from failed attempts
	BOOST_CHECK_EQUAL(analyzerPtr->GetAppendCount(), 0);
	BOOST_CHECK(analyzerPtr->GetRecorded().empty());

	g_ServerPool = env.oldServerPool;
	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
}

BOOST_AUTO_TEST_CASE(EncryptedConnectionReuseAfterEarlyRejectionTest)
{
	// NZ-NEXT-1 / T05 regression:
	// A primary provider delivers an invalid first line separately, followed by a
	// delayed remainder and dot terminator. The decoder encounters early rejection
	// and exits the download loop before consuming the transport terminator.
	// If the connection is freed without being disconnected, a subsequent article
	// reusing this connection reads the leftover body bytes instead of the expected
	// NNTP command response (desynchronization).
	//
	// In the correct behavior:
	// 1. The undrained connection is disconnected rather than kept connected.
	// 2. Article 1 fails over to the backup provider and succeeds.
	// 3. Article 2 is downloaded and succeeds on primary (re-connecting cleanly).
	// 4. All corrupt attempts commit and analyze zero bytes.
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_reuse_nntp_test";
	stdfs::create_directories(tempDir);
	stdfs::path tempDirOpt = stdfs::temp_directory_path() / "nzbget_reuse_nntp_temp";
	stdfs::create_directories(tempDirOpt);

	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> plaintext1 = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::vector<uint8_t> plaintext2 = YEncTest::HexToBin("5365636f6e642056616c696420417274");
	const std::string filename1 = "reuse_art1.dat";
	const std::string filename2 = "reuse_art2.dat";

	std::string validWire1 = BuildValidEncryptedWireArticle(password, salt, 1, filename1, plaintext1);
	std::string validWire2 = BuildValidEncryptedWireArticle(password, salt, 2, filename2, plaintext2);

	std::atomic<int> primaryRequests{0};
	std::atomic<int> backupRequests{0};

	auto primaryHandler = [&](int clientFd, const std::string& cmd, int reqNum) {
		if (cmd.find("<art1-msg@test>") != std::string::npos)
		{
			// First invalid line delivered separately
			std::string initial = "222 1 <art1-msg@test>\r\n"
				"4b376d5839704c32715238764e34775a99999999CORRUPT_LINE_ONE_DATA\r\n";
			send(clientFd, initial.data(), initial.size(), MSG_NOSIGNAL);

			// Delayed remainder and transport terminator
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			std::string remainder = "=ypart line=1 size=100\r\ndata\r\n=yend size=100\r\n.\r\n";
			send(clientFd, remainder.data(), remainder.size(), MSG_NOSIGNAL);
		}
		else
		{
			// Second queued valid article
			std::string resp = "222 1 <art2-msg@test>\r\n" + validWire2;
			send(clientFd, resp.data(), resp.size(), MSG_NOSIGNAL);
		}
	};

	LocalNntpServer primary(primaryHandler, &primaryRequests);
	LocalNntpServer backup(validWire1, &backupRequests);

	ServerPool pool;
	auto server1 = std::make_unique<NewsServer>(1, true, "primary", "127.0.0.1", primary.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvStrict);
	auto server2 = std::make_unique<NewsServer>(2, true, "backup", "127.0.0.1", backup.GetPort(), 0,
		"", "", false, false, nullptr, 1, 0, 1, 0, false, Options::cvStrict);
	pool.AddServer(std::move(server1));
	pool.AddServer(std::move(server2));
	pool.InitConnections();
	pool.SetRetryInterval(60);

	DownloaderEnv env(tempDirOpt.string(), false, 0);
	g_ServerPool = &pool;
	env.statMeter->Init();

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", password.c_str());

	// --- Article 1 ---
	FileInfo fileInfo1;
	fileInfo1.SetNzbInfo(&nzbInfo);
	fileInfo1.SetFilename(filename1.c_str());
	fileInfo1.SetTime(Util::CurrentTime());
	fileInfo1.GetArticles()->resize(1);
	(*fileInfo1.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo1.GetArticles())[0]->SetMessageId("<art1-msg@test>");
	(*fileInfo1.GetArticles())[0]->SetSize(static_cast<int>(plaintext1.size()));
	(*fileInfo1.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader1;
	downloader1.SetFileInfo(&fileInfo1);
	downloader1.SetArticleInfo((*fileInfo1.GetArticles())[0].get());
	downloader1.SetInfoName("reuse_article1");

	auto recordingAnalyzer1 = std::make_unique<RecordingContentAnalyzer>();
	RecordingContentAnalyzer* analyzerPtr1 = recordingAnalyzer1.get();
	downloader1.SetContentAnalyzer(std::move(recordingAnalyzer1));

	downloader1.Run();

	// Article 1 must succeed via backup provider failover
	BOOST_CHECK_EQUAL(static_cast<int>(downloader1.GetStatus()), static_cast<int>(ArticleDownloader::adFinished));
	BOOST_CHECK_GE(primaryRequests.load(), 1);
	BOOST_CHECK_GE(backupRequests.load(), 1);

	// Plaintext 1 committed from backup
	Crc32 plaintextCrc1;
	plaintextCrc1.Append(const_cast<uint8_t*>(plaintext1.data()), static_cast<uint32>(plaintext1.size()));
	BOOST_CHECK_EQUAL((*fileInfo1.GetArticles())[0]->GetCrc(), plaintextCrc1.Finish());

	const char* resultFilename1 = (*fileInfo1.GetArticles())[0]->GetResultFilename();
	BOOST_REQUIRE(resultFilename1 != nullptr);
	std::ifstream resultFile1(resultFilename1, std::ios::binary);
	BOOST_REQUIRE(resultFile1);
	std::vector<char> fileBytes1((std::istreambuf_iterator<char>(resultFile1)),
		std::istreambuf_iterator<char>());
	BOOST_REQUIRE_EQUAL(fileBytes1.size(), plaintext1.size());
	BOOST_CHECK_EQUAL(memcmp(fileBytes1.data(), plaintext1.data(), plaintext1.size()), 0);

	// Corrupted attempt from primary committed zero bytes to analyzer
	BOOST_REQUIRE_EQUAL(analyzerPtr1->GetRecorded().size(), plaintext1.size());
	BOOST_CHECK_EQUAL(memcmp(analyzerPtr1->GetRecorded().data(), plaintext1.data(), plaintext1.size()), 0);

	// --- Article 2 ---
	FileInfo fileInfo2;
	fileInfo2.SetNzbInfo(&nzbInfo);
	fileInfo2.SetFilename(filename2.c_str());
	fileInfo2.SetTime(Util::CurrentTime());
	fileInfo2.GetArticles()->resize(1);
	(*fileInfo2.GetArticles())[0] = std::make_unique<ArticleInfo>();
	(*fileInfo2.GetArticles())[0]->SetMessageId("<art2-msg@test>");
	(*fileInfo2.GetArticles())[0]->SetSize(static_cast<int>(plaintext2.size()));
	(*fileInfo2.GetArticles())[0]->SetPartNumber(1);

	ArticleDownloader downloader2;
	downloader2.SetFileInfo(&fileInfo2);
	downloader2.SetArticleInfo((*fileInfo2.GetArticles())[0].get());
	downloader2.SetInfoName("reuse_article2");

	auto recordingAnalyzer2 = std::make_unique<RecordingContentAnalyzer>();
	RecordingContentAnalyzer* analyzerPtr2 = recordingAnalyzer2.get();
	downloader2.SetContentAnalyzer(std::move(recordingAnalyzer2));

	downloader2.Run();

	// Article 2 must succeed on primary without needing backup failover
	BOOST_CHECK_EQUAL(static_cast<int>(downloader2.GetStatus()), static_cast<int>(ArticleDownloader::adFinished));
	BOOST_CHECK_GE(primaryRequests.load(), 2);
	BOOST_CHECK_EQUAL(backupRequests.load(), 1);

	// Plaintext 2 committed
	Crc32 plaintextCrc2;
	plaintextCrc2.Append(const_cast<uint8_t*>(plaintext2.data()), static_cast<uint32>(plaintext2.size()));
	BOOST_CHECK_EQUAL((*fileInfo2.GetArticles())[0]->GetCrc(), plaintextCrc2.Finish());

	const char* resultFilename2 = (*fileInfo2.GetArticles())[0]->GetResultFilename();
	BOOST_REQUIRE(resultFilename2 != nullptr);
	std::ifstream resultFile2(resultFilename2, std::ios::binary);
	BOOST_REQUIRE(resultFile2);
	std::vector<char> fileBytes2((std::istreambuf_iterator<char>(resultFile2)),
		std::istreambuf_iterator<char>());
	BOOST_REQUIRE_EQUAL(fileBytes2.size(), plaintext2.size());
	BOOST_CHECK_EQUAL(memcmp(fileBytes2.data(), plaintext2.data(), plaintext2.size()), 0);

	BOOST_REQUIRE_EQUAL(analyzerPtr2->GetRecorded().size(), plaintext2.size());
	BOOST_CHECK_EQUAL(memcmp(analyzerPtr2->GetRecorded().data(), plaintext2.data(), plaintext2.size()), 0);

	BOOST_CHECK(!stdfs::exists(tempDir / filename1));
	BOOST_CHECK(!stdfs::exists(tempDir / filename2));

	g_ServerPool = env.oldServerPool;
	stdfs::remove_all(tempDir);
	stdfs::remove_all(tempDirOpt);
}

#endif // !WIN32

BOOST_AUTO_TEST_SUITE_END()
