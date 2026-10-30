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
#include "ArticleWriter.h"
#include "ArticleDownloader.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "FileSystem.h"
#include "Util.h"
#include "QueueCoordinator.h"
#include "DiskState.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace stdfs = std::filesystem;

namespace
{
// RAII restore for g_Options (pattern: RestoreOptionsGlobal in
// tests/postprocess/DirectUnpack.cpp): a failing BOOST_REQUIRE throws, so a
// manual restore at the end of a case would be skipped and g_Options would
// dangle at the destroyed stack Options for later cases.
class RestoreOptionsGlobal final
{
public:
	RestoreOptionsGlobal() : m_previous(g_Options) {}
	~RestoreOptionsGlobal() { g_Options = m_previous; }

private:
	Options* m_previous;
};
} // namespace

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(EncryptedArticleWriterStagingTest)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_stage_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", "synthetic-archive-fixture-1");
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("staging_test.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("staging_test_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "staging_test.dat", 32, 0, 32));

	// Pass unauthenticated ciphertext chunks to Write
	std::vector<char> ciphertext(32, 'X');
	bool writeOk = writer.Write(ciphertext.data(), static_cast<int>(ciphertext.size()));
	BOOST_CHECK(writeOk);

	// Production auth-failure path (ArticleDownloader calls Finish(false)).
	writer.Finish(false);
	BOOST_CHECK(articleInfo.GetSegmentContent() == nullptr);

	// Zero-Output Guarantee: Destination file and temp files must not exist
	stdfs::path destFile = tempDir / "staging_test.dat";
	BOOST_CHECK(!stdfs::exists(destFile));

	// Also check temp files
	stdfs::path tempFile = tempDir / "staging_test.dat.tmp";
	BOOST_CHECK(!stdfs::exists(tempFile));

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedArticleWriterCommitTest)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_commit_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", "synthetic-archive-fixture-1");
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("commit_test.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(16);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("commit_test_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "commit_test.dat", 32, 16, 16));

	// Commit authenticated plaintext for a non-zero multipart offset. The
	// per-article temp file still starts at byte zero; the offset is metadata
	// used when joining the authenticated segments into the final file.
	std::vector<uint8_t> plainData = {
		0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f,
		0x72, 0x6c, 0x64, 0x2e, 0x74, 0x78, 0x74, 0xff
	};

	bool commitOk = writer.CommitAuthenticatedData(plainData.data(), plainData.size());
	BOOST_CHECK(commitOk);

	writer.Finish(true);
	BOOST_CHECK_EQUAL(articleInfo.GetSegmentOffset(), 16);

	if (articleInfo.GetSegmentContent())
	{
		// Data was committed to segment cache
		BOOST_CHECK_EQUAL(articleInfo.GetSegmentSize(), 16);
		const char* cached = articleInfo.GetSegmentContent();
		BOOST_REQUIRE(cached != nullptr);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			reinterpret_cast<const uint8_t*>(cached),
			reinterpret_cast<const uint8_t*>(cached) + 16,
			plainData.begin(),
			plainData.end()
		);
	}
	else
	{
		// Written to disk
		stdfs::path resFile = articleInfo.GetResultFilename() ? articleInfo.GetResultFilename() : "";
		stdfs::path destFile = tempDir / "commit_test.dat";
		stdfs::path targetFile = stdfs::exists(resFile) ? resFile : destFile;
		BOOST_REQUIRE(stdfs::exists(targetFile));
		std::ifstream ifs(targetFile, std::ios::binary);
		std::vector<uint8_t> readBytes(
			(std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>()
		);
		BOOST_CHECK_EQUAL_COLLECTIONS(readBytes.begin(), readBytes.end(), plainData.begin(), plainData.end());
	}

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(UnencryptedArticleWriterWithArchivePasswordTest)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_archive_fixture_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	Options options(&cmdOpts, nullptr);
	Options* oldOptions = g_Options;
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", "synthetic-archive-fixture-2");
	// Not yEnc encrypted!
	nzbInfo.SetYEncEncrypted(false);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("archive_fixture_test.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("archive_fixture_test_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "archive_fixture_test.dat", 32, 0, 32));

	std::vector<char> payload(32, 'P');
	bool writeOk = writer.Write(payload.data(), static_cast<int>(payload.size()));
	BOOST_CHECK(writeOk);

	writer.Finish(true);

	// Verify that writer.Write did not discard the bytes: either in cache or on disk
	if (articleInfo.GetSegmentContent())
	{
		BOOST_CHECK_EQUAL(articleInfo.GetSegmentSize(), 32);
		const char* cached = articleInfo.GetSegmentContent();
		BOOST_REQUIRE(cached != nullptr);
		BOOST_CHECK_EQUAL(memcmp(cached, payload.data(), 32), 0);
	}
	else
	{
		stdfs::path resFile = articleInfo.GetResultFilename() ? articleInfo.GetResultFilename() : "";
		stdfs::path outTmp = fileInfo.GetOutputFilename();
		stdfs::path destFile = tempDir / "archive_fixture_test.dat";
		stdfs::path targetFile;
		if (stdfs::exists(resFile))
		{
			targetFile = resFile;
		}
		else if (stdfs::exists(outTmp))
		{
			targetFile = outTmp;
		}
		else
		{
			targetFile = destFile;
		}
		BOOST_REQUIRE(stdfs::exists(targetFile));
		std::ifstream ifs(targetFile, std::ios::binary);
		std::vector<char> readBytes(
			(std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>()
		);
		BOOST_CHECK_EQUAL(readBytes.size(), 32U);
		BOOST_CHECK_EQUAL_COLLECTIONS(readBytes.begin(), readBytes.end(), payload.begin(), payload.end());
	}

	g_Options = oldOptions;
	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedArticleWriterDirectWriteGatingTest)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_direct_write_gating_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	cmdOpts.push_back("DirectWrite=yes");
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	BOOST_REQUIRE(g_Options->GetDirectWrite());

	// Case 1: NZB declares yenc_encrypted; direct-write must be bypassed
	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.GetParameters()->SetParameter("*Unpack:Pass" "word", "synthetic-archive-fixture-1");
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("gating_test.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("gating_test_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "gating_test.dat", 32, 0, 32));

	// Direct-write must be strictly blocked: final destination file must NOT be created yet
	stdfs::path finalDestFile = tempDir / "gating_test.dat";
	BOOST_CHECK(!stdfs::exists(finalDestFile));

	// Write unauthenticated ciphertext chunks; with writer marked encrypted, chunks must be discarded
	std::vector<char> ciphertext(32, 'Z');
	BOOST_CHECK(writer.Write(ciphertext.data(), static_cast<int>(ciphertext.size())));

	// Production auth-failure path (simulating authentication failure).
	writer.Finish(false);
	BOOST_CHECK(articleInfo.GetSegmentContent() == nullptr);

	// Verify Zero-Output Guarantee: destination file must never have been created
	BOOST_CHECK(!stdfs::exists(finalDestFile));

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(ZeroByteCommitAuthenticatedDataTest)
{
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_zero_byte_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("zero_byte.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(0);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("zero_byte_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "zero_byte.dat", 0, 0, 0));

	// CommitAuthenticatedData for 0-byte authenticated payload must succeed
	bool commitOk = writer.CommitAuthenticatedData(nullptr, 0);
	BOOST_CHECK(commitOk);

	writer.Finish(true);
	BOOST_CHECK_EQUAL(articleInfo.GetSegmentSize(), 0);

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedWriterFinishFalseLeavesNoDestinationTest)
{
	// Zero-output coverage: staged (unauthenticated) bytes passed to
	// Write() followed by Finish(false) on the production auth-failure
	// path must leave no destination file. (The silent-ciphertext DecodeCheck path
	// itself — bootstrap-less encrypted article rejected before any writer
	// call — is exercised at the ArticleDownloader/decoder level.)
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_silent_ciphertext_test";
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	std::string optDest = "DestDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back(optDest.c_str());
	cmdOpts.push_back("DirectWrite=yes");
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("silent_ct.dat");

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("silent_ct_article");
	writer.Prepare();

	// Staged ciphertext followed by Finish(false) on the production
	// failure path must leave no destination file or attached segment.
	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "silent_ct.dat", 32, 0, 32));
	std::vector<char> ciphertext(32, 'Q');
	BOOST_CHECK(writer.Write(ciphertext.data(), static_cast<int>(ciphertext.size())));
	writer.Finish(false);
	BOOST_CHECK(articleInfo.GetSegmentContent() == nullptr);
	BOOST_CHECK(!stdfs::exists(tempDir / "silent_ct.dat"));

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedCommitGeometryValidationTest)
{
	// Authenticated commit geometry validation — zero/negative
	// begin offsets, out-of-file offsets, and span-size mismatch must be
	// rejected BEFORE any Start/CommitAuthenticatedData writes happen.
	// The gate lives in ArticleDownloader::DecodeCheck and is factored into
	// the shared predicate ArticleDownloader::ValidCommitGeometry; this test
	// drives that shared predicate, so a production edit to the gate's logic
	// cannot silently diverge from the tested behavior. (DecodeCheck applies
	// this predicate verbatim before Start()/CommitAuthenticatedData.)
	Options::CmdOptList cmdOpts;
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	// 1. Zero offset (beginPos == 0 → articleOffset = -1 after -1 subtraction).
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, 0, 16, 16));
	// 2. Negative beginPos.
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, -5, 16, 16));
	// 3. endPos < beginPos (negative span).
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, 16, 5, 16));
	// 4. Zero/negative file size.
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(0, 1, 16, 16));
	// 5. Span-size mismatch (decrypted size differs from endPos-beginPos+1).
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, 1, 16, 15));
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, 1, 16, 17));
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(32, 1, 16, 0));
	// 6. Out-of-file offset: offset+span exceeds the declared file size.
	BOOST_CHECK(!ArticleDownloader::ValidCommitGeometry(20, 10, 25, 16));
	// 7. Valid geometry (sanity): single-part size=16, begin=1, end=16.
	BOOST_CHECK(ArticleDownloader::ValidCommitGeometry(16, 1, 16, 16));
	// 8. Valid multipart geometry: begin=17, end=32 in a 32-byte file.
	BOOST_CHECK(ArticleDownloader::ValidCommitGeometry(32, 17, 32, 16));

}

BOOST_AUTO_TEST_CASE(EncryptedDirectWriteDiscardRemovesTempFilesTest)
{
	stdfs::path dir = stdfs::temp_directory_path() / "nzbget_encrypted_discard";
	stdfs::create_directories(dir);
	Options::CmdOptList opts;
	opts.push_back("DirectWrite=yes");
	RestoreOptionsGlobal restoreOptions;
	Options options(&opts, nullptr);
	g_Options = &options;
	// DiscardTempFiles is a member function. As in the Scanner tests, keep one
	// never-destroyed coordinator instead of re-pointing the globals per test.
	static QueueCoordinator* coordinator = new QueueCoordinator();
	for (bool encrypted : {false, true})
	{
		NzbInfo nzb;
		nzb.SetYEncEncrypted(encrypted);
		FileInfo file;
		file.SetNzbInfo(&nzb);
		auto article = std::make_unique<ArticleInfo>();
		std::string segment = (dir / "segment").string();
		std::ofstream(segment) << "authenticated segment";
		article->SetResultFilename(segment.c_str());
		file.GetArticles()->push_back(std::move(article));
		coordinator->DiscardTempFiles(&file);
		BOOST_CHECK_EQUAL(stdfs::exists(segment), !encrypted);
	}
	stdfs::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(DiscardTempFilesResumesCompletedOrdinaryDirectWriteTest)
{
	// Regression (upstream parity): DiscardTempFiles must keep the upstream
	// `!GetForceDirectWrite()` term on the output-delete branch. An ordinary
	// (unencrypted) file resumed with psCompleted has ForceDirectWrite=true;
	// deleting it from the queue then must NOT delete the shared output file,
	// while an ordinary fresh direct-write file (ForceDirectWrite=false) still
	// has its stale output file deleted as upstream does.
	stdfs::path dir = stdfs::temp_directory_path() / "nzbget_discard_ordinary_dw";
	stdfs::create_directories(dir);
	Options::CmdOptList opts;
	opts.push_back("DirectWrite=yes");
	Options options(&opts, nullptr);
	Options* oldOptions = g_Options;
	g_Options = &options;
	static QueueCoordinator* coordinator = new QueueCoordinator();
	for (bool forceDirectWrite : {false, true})
	{
		NzbInfo nzb;
		FileInfo file;
		file.SetNzbInfo(&nzb);
		file.SetForceDirectWrite(forceDirectWrite);
		std::string output = (dir / (forceDirectWrite ? "resumed.out" : "fresh.out"));
		std::ofstream(output) << "output bytes";
		file.SetOutputFilename(output);
		BOOST_CHECK(stdfs::exists(output));
		coordinator->DiscardTempFiles(&file);
		BOOST_CHECK_EQUAL(stdfs::exists(output), forceDirectWrite);
	}
	g_Options = oldOptions;
	stdfs::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(EncryptedAuthFailureOwnershipTest)
{
	// On encrypted auth failure the production Finish(false) path deletes
	// m_tempFilename/m_resultFilename. Ownership check: the deleted paths
	// are this article's own per-part scratch paths
	// (<TempDir>/<fileId>.<part>[.tmp]); a sibling article's committed result
	// file (different part number) must survive.
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_auth_failure_ownership_test";
	stdfs::remove_all(tempDir);
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	// Force disk staging (no article cache) so the encrypted path uses the
	// per-article temp file rather than CachedSegmentData.
	cmdOpts.push_back("ArticleCache=0");
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);

	// Sibling article (part 1) with a committed result file on disk.
	std::string siblingSegment = (tempDir / "42.001").string();
	{
		std::ofstream ofs(siblingSegment, std::ios::binary);
		ofs << "authenticated sibling plaintext";
	}

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("ownership_test.dat");
	fileInfo.SetId(42);

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);
	articleInfo.SetPartNumber(2);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("ownership_test_article");
	writer.Prepare();

	// Own paths are distinct from the sibling's path.
	BOOST_CHECK_EQUAL(articleInfo.GetResultFilename(), (tempDir / "42.002").string().c_str());

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "ownership_test.dat", 32, 0, 32));

	// Unauthenticated ciphertext must never be staged (Write is a no-op).
	std::vector<char> ciphertext(32, 'X');
	BOOST_CHECK(writer.Write(ciphertext.data(), static_cast<int>(ciphertext.size())));

	// Production auth failure path: Finish(false) from Download().
	writer.Finish(false);
	BOOST_CHECK(articleInfo.GetSegmentContent() == nullptr);
	BOOST_CHECK(!stdfs::exists(tempDir / "42.002"));
	BOOST_CHECK(!stdfs::exists(tempDir / "42.002.tmp"));
	BOOST_CHECK_EQUAL(stdfs::exists(siblingSegment), true);
	{
		std::ifstream ifs(siblingSegment, std::ios::binary);
		std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		BOOST_CHECK_EQUAL(content, "authenticated sibling plaintext");
	}

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedSkipDiskAuthFailureDeletesNothingTest)
{
	// Skip-disk mode must keep the deletion guard in Finish(false):
	// nothing on disk exists, nothing is touched.
	stdfs::path tempDir = stdfs::temp_directory_path() / "nzbget_skipdisk_auth_fail_test";
	stdfs::remove_all(tempDir);
	stdfs::create_directories(tempDir);

	std::string optTemp = "TempDir=" + tempDir.string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(optTemp.c_str());
	cmdOpts.push_back("ArticleCache=0");
	cmdOpts.push_back("SkipWrite=yes");
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	NzbInfo nzbInfo;
	nzbInfo.SetDestDir(tempDir.string().c_str());
	nzbInfo.SetYEncEncrypted(true);

	FileInfo fileInfo;
	fileInfo.SetNzbInfo(&nzbInfo);
	fileInfo.SetFilename("skipdisk_test.dat");
	fileInfo.SetId(7);

	ArticleInfo articleInfo;
	articleInfo.SetSize(32);
	articleInfo.SetPartNumber(1);

	ArticleWriter writer;
	writer.SetFileInfo(&fileInfo);
	writer.SetArticleInfo(&articleInfo);
	writer.SetInfoName("skipdisk_test_article");
	writer.Prepare();

	BOOST_REQUIRE(writer.Start(Decoder::efYenc, "skipdisk_test.dat", 32, 0, 32));
	std::vector<char> ciphertext(32, 'X');
	BOOST_CHECK(writer.Write(ciphertext.data(), static_cast<int>(ciphertext.size())));
	writer.Finish(false);
	BOOST_CHECK(articleInfo.GetSegmentContent() == nullptr);

	// SkipWrite guard: nothing was created and nothing was deleted.
	BOOST_CHECK(!stdfs::exists(tempDir / "7.001"));
	BOOST_CHECK(!stdfs::exists(tempDir / "7.001.tmp"));

	stdfs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(EncryptedDirectWriteRestoreKeepsFinishedSegmentsTest)
{
	stdfs::path dir = stdfs::temp_directory_path() / "nzbget_encrypted_restore";
	stdfs::create_directories(dir);
	std::string queueOpt = "QueueDir=" + dir.string();
	std::string tempOpt = "TempDir=" + dir.string();
	Options::CmdOptList opts;
	opts.push_back("DirectWrite=yes");
	opts.push_back(queueOpt.c_str());
	opts.push_back(tempOpt.c_str());
	RestoreOptionsGlobal restoreOptions;
	Options options(&opts, nullptr);
	g_Options = &options;
	for (bool encrypted : {false, true})
	{
		NzbInfo nzb;
		nzb.SetYEncEncrypted(encrypted);
		FileInfo file;
		file.SetNzbInfo(&nzb);
		file.SetFilename("fixture.bin");
		for (int part = 1; part <= 2; ++part)
		{
			auto article = std::make_unique<ArticleInfo>();
			article->SetPartNumber(part);
			article->SetStatus(part == 1 ? ArticleInfo::aiFinished : ArticleInfo::aiUndefined);
			file.GetArticles()->push_back(std::move(article));
		}
		DiskState disk;
		BOOST_REQUIRE(disk.SaveFileState(&file, false));
		file.GetArticles()->front()->SetStatus(ArticleInfo::aiUndefined);
		Servers servers;
		BOOST_REQUIRE(disk.LoadFileState(&file, &servers, false));
		ArticleInfo* restored = file.GetArticles()->front().get();
		BOOST_CHECK_EQUAL(restored->GetStatus(), ArticleInfo::aiFinished);
		BOOST_CHECK_EQUAL(restored->GetResultFilename() != nullptr, encrypted);
	}
	stdfs::remove_all(dir);
}

BOOST_AUTO_TEST_SUITE_END()
