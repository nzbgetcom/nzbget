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
#include "Options.h"
#include "Scanner.h"
#include "NzbFile.h"
#include "DownloadInfo.h"
#include "FileSystem.h"
#include "Log.h"
#include "QueueCoordinator.h"

#include <memory>
#include "Util.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

namespace
{
	// AddFileToQueue hands accepted releases to QueueCoordinator::
	// AddNzbFileToQueue, which guards the global DownloadQueue. The test
	// provides the minimal mock queue the coordinator needs (same pattern as
	// DirectUnpackDownloadQueueMock in tests/postprocess/DirectUnpack.cpp).
	//
	// The mock also replicates the production download-queue listener chain
	// (PrePostProcessor::NzbAdded → NzbCompleted → HistoryCoordinator::
	// AddToHistory): a release NOT added to the download queue (Scanner sets a
	// delete status) is moved into history, which takes ownership. Upstream
	// AddNzbFileToQueue relies on that listener — with no listener, its
	// dsScan-cleanup would destroy the NzbInfo while its id is still read.
	// Static storage (not heap): the framework globals keep pointers to it for
	// the process lifetime, and static storage keeps LeakSanitizer quiet.
	class ScannerDownloadQueueMock final : public DownloadQueue, public Observer
	{
	public:
		ScannerDownloadQueueMock()
		{
			Init(this);
			Loaded();
			Attach(this);
		}
		~ScannerDownloadQueueMock()
		{
			Final();
		}
		bool EditEntry(int ID, EEditAction action, const char* args) override { return false; }
		bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
			EEditAction action, const char* args) override { return false; }
		void HistoryChanged() override {}
		void Save() override {}
		void SaveChanged() override {}

		void Update(Subject* /*caller*/, void* aspect) override
		{
			DownloadQueue::Aspect* queueAspect = static_cast<DownloadQueue::Aspect*>(aspect);
			if (queueAspect->action == DownloadQueue::eaNzbAdded && queueAspect->nzbInfo &&
				queueAspect->nzbInfo->GetDeleteStatus() != NzbInfo::dsNone)
			{
				std::unique_ptr<NzbInfo> nzbInfo = GetQueue()->Remove(queueAspect->nzbInfo);
				if (nzbInfo)
				{
					std::unique_ptr<HistoryInfo> historyInfo = std::make_unique<HistoryInfo>(std::move(nzbInfo));
					historyInfo->SetTime(Util::CurrentTime());
					GetHistory()->Add(std::move(historyInfo), true);
				}
			}
		}
	};

	// Minimal encrypted NZB: yenc_encrypted marker, no password meta, no
	// filename token — the password must come from the caller parameters
	// (or nowhere).
	void WriteEncryptedNzb(const fs::path& filePath, bool encrypted = true)
	{
		std::ofstream out(filePath.string());
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			<< "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n";
		if (encrypted)
		{
			out << "<head><meta type=\"yenc_encrypted\">true</meta></head>\n";
		}
		out << "<file poster=\"p\" date=\"100\" subject=\"gate.bin\">\n"
			<< "<groups><group>a.b.t</group></groups>\n"
			<< "<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
			<< "</file>\n"
			<< "</nzb>\n";
	}
}

BOOST_AUTO_TEST_CASE(ScannerEffectivePasswordGateTest)
{
	// Scanner::AddFileToQueue resolves ALL password sources (NZB meta,
	// filename token, caller *Unpack:Password parameters) before the
	// missing-password validation. These tests drive the real
	// Scanner::AddFileToQueue (via the test hook) through the
	// merge-then-check sequence.
	Options::CmdOptList cmdOpts;
	fs::path tempDir = fs::temp_directory_path() / "nzbget_scanner_gate_test";
	fs::create_directories(tempDir);
	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optNzbDir = "NzbDir=" + tempDir.string();
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optNzbDir.c_str());
	Options* oldOptions = g_Options;
	Scanner* oldScanner = g_Scanner;
	QueueCoordinator* oldQueueCoordinator = g_QueueCoordinator;
	{
		// Options as unique_ptr so it can be destroyed BEFORE the g_Options
		// restore (Options::~Options nulls the global unconditionally — same
		// handling as DownloaderEnv in tests/nntp/ArticleDownloader.cpp).
		std::unique_ptr<Options> options = std::make_unique<Options>(&cmdOpts, nullptr);
		g_Options = options.get();

		// Construction order matters: QueueCoordinator's constructor re-points
		// the DownloadQueue::g_DownloadQueue global at its own inner queue, so
		// the mock queue must be constructed AFTER it to become the global
		// queue the Scanner talks to. All three are static (never detached
		// from the framework globals; static storage keeps LeakSanitizer quiet).
		static QueueCoordinator* coordinator = new QueueCoordinator();
		g_QueueCoordinator = coordinator;

		static ScannerDownloadQueueMock* queueMock = nullptr;
		if (!queueMock)
		{
			queueMock = new ScannerDownloadQueueMock();
		}

		static Scanner* scanner = new Scanner();
		g_Scanner = scanner;

		// (a) *Unpack:Password supplied by the caller and no meta/filename
		// password: the parameter merge satisfies the effective-password
		// requirement and the release is accepted into the queue.
		{
			const fs::path nzbFile = tempDir / "scan_gate_param.nzb";
			WriteEncryptedNzb(nzbFile);

			NzbParameterList parameters;
			parameters.SetParameter("*Unpack:Password", "api_password");
			int nzbId = 0;
			bool added = scanner->TestAddFileToQueue(nzbFile.string().c_str(),
				"scan_gate_param", "", true, 0, "", 0, dmScore, &parameters, false, false, nullptr, &nzbId);
			BOOST_CHECK(added);
			BOOST_CHECK(nzbId > 0);
		}

		// (b) no password from ANY source (no meta, no filename token, no
		// parameters): rejected with METADATA_VALIDATION and not added to the
		// download queue. This is the real gate coverage for conformance
		// vector metadata-03-missing-password (DynamicMalformedInputsTestVectorsTest
		// only pins that vector's structural classification). The release
		// lands in history (delete status dsScan),
		// matching the production flow; nzbId is the history record's id.
		{
			const fs::path nzbFile = tempDir / "scan_gate_nopwd.nzb";
			WriteEncryptedNzb(nzbFile);

			NzbParameterList parameters;
			int nzbId = 0;
			bool added = scanner->TestAddFileToQueue(nzbFile.string().c_str(),
				"scan_gate_nopwd", "", true, 0, "", 0, dmScore, &parameters, false, false, nullptr, &nzbId);
			BOOST_CHECK(!added);

			// A rejected encrypted release must get the same backup suffix as
			// every other parse failure (.error), not .queued: the effective-
			// password gate runs before the backup rename.
			BOOST_CHECK(!fs::exists(tempDir / "scan_gate_nopwd.nzb.queued"));
			BOOST_CHECK(fs::exists(tempDir / "scan_gate_nopwd.nzb.error"));
		}

		// (c) sanity: an UNencrypted release without any password is still
		// accepted — the gate applies to encrypted releases only.
		{
			const fs::path nzbFile = tempDir / "scan_gate_plain.nzb";
			WriteEncryptedNzb(nzbFile, false);

			NzbParameterList parameters;
			int nzbId = 0;
			bool added = scanner->TestAddFileToQueue(nzbFile.string().c_str(),
				"scan_gate_plain", "", true, 0, "", 0, dmScore, &parameters, false, false, nullptr, &nzbId);
			BOOST_CHECK(added);
			BOOST_CHECK(nzbId > 0);
		}

		// Heap-allocated coordinator/scanner above are intentionally leaked;
		// restore the framework globals so later suites never observe
		// test-owned objects. The Options instance is destroyed BEFORE the
		// g_Options restore (its destructor nulls the global).
		g_Scanner = oldScanner;
		g_QueueCoordinator = oldQueueCoordinator;
		options.reset();
	}
	g_Options = oldOptions;
	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_SUITE_END()
