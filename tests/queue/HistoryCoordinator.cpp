#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <fstream>
#include "DiskState.h"
#include "FileSystem.h"
#include "HistoryCoordinator.h"
#include "Options.h"
#include "PrePostProcessor.h"
#include "ServerPool.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

namespace
{

// Keep queue persistence in the fixture's real DiskState files. Queue edits
// and global queue saves are unrelated to the history retry being exercised.
class RetryDownloadQueue final : public DownloadQueue
{
public:
	RetryDownloadQueue() { Init(this); }
	~RetryDownloadQueue() { Final(); }
	bool EditEntry(int, EEditAction, const char*) override { return false; }
	bool EditList(IdList*, NameList*, EMatchMode, EEditAction, const char*) override { return false; }
	void HistoryChanged() override {}
	void Save() override {}
	void SaveChanged() override {}
};

struct HistoryRetryFixture
{
	Options* previousOptions = g_Options;
	PrePostProcessor* previousPostProcessor = g_PrePostProcessor;
	fs::path directory = fs::temp_directory_path() / fs::make_unique_filename();
	RetryDownloadQueue queue;
	std::unique_ptr<Options> options;
	std::unique_ptr<PrePostProcessor> postProcessor;
	HistoryCoordinator history;
	NzbInfo* nzb = nullptr;
	int fileId = 0;

	HistoryRetryFixture()
	{
		fs::create_directories(directory / "queue");
		fs::create_directories(directory / "download");
		const std::string queueDirOption = "QueueDir=" + (directory / "queue").string();
		Options::CmdOptList cmdOpts;
		cmdOpts.emplace_back(queueDirOption.c_str());
		cmdOpts.emplace_back("NzbLog=no");
		cmdOpts.emplace_back("WriteLog=none");
		cmdOpts.emplace_back("ParCheck=force");
		options = std::make_unique<Options>(&cmdOpts, nullptr);
		postProcessor = std::make_unique<PrePostProcessor>();
		g_PrePostProcessor = postProcessor.get();
	}

	~HistoryRetryFixture()
	{
		queue.Detach(postProcessor.get());
		postProcessor.reset();
		g_PrePostProcessor = previousPostProcessor;
		options.reset();
		g_Options = previousOptions;
		fs::error_code ec;
		fs::remove_all(directory, ec);
	}

	void SavePartialFile(bool failedArticle, bool outputExists = true)
	{
		auto nzbInfo = std::make_unique<NzbInfo>();
		nzb = nzbInfo.get();
		nzb->SetName("Retry regression");
		nzb->SetDestDir((directory / "download").string().c_str());
		nzb->SetSize(16);
		nzb->SetFileCount(1);
		nzb->SetTotalArticles(4);
		nzb->SetSuccessArticles(2);
		nzb->SetSuccessSize(8);
		// Completed collection statistics include entries missing from the NZB.
		nzb->SetFailedArticles(2);
		nzb->SetFailedSize(8);

		FileInfo file;
		fileId = file.GetId();
		file.SetNzbInfo(nzb);
		file.SetSubject("partial archive");
		file.SetFilename("part152.rar");
		file.SetOrigname("original-name");
		file.SetFilenameConfirmed(true);
		file.SetSize(16);
		file.SetDecodedFileSize(16);
		file.SetRemainingSize(0);
		file.SetTotalArticles(4);
		file.SetMissedArticles(failedArticle ? 1 : 2);
		file.SetMissedSize(failedArticle ? 4 : 8);
		file.SetSuccessArticles(2);
		file.SetSuccessSize(8);
		file.SetFailedArticles(failedArticle ? 1 : 0);
		file.SetFailedSize(failedArticle ? 4 : 0);
		file.SetCompletedArticles(failedArticle ? 3 : 2);
		for (int part : {1, 3, 4})
		{
			if (part == 3 && !failedArticle) continue;
			auto article = std::make_unique<ArticleInfo>();
			article->SetPartNumber(part);
			article->SetSize(4);
			article->SetMessageId(BString<1024>("part-%i@example.test", part));
			article->SetStatus(part == 3 ? ArticleInfo::aiFailed : ArticleInfo::aiFinished);
			article->SetSegmentOffset((part - 1) * 4);
			article->SetSegmentSize(part == 3 ? 0 : 4);
			file.GetArticles()->push_back(std::move(article));
		}
		BOOST_REQUIRE(g_DiskState->SaveFile(&file));
		BOOST_REQUIRE(g_DiskState->SaveFileState(&file, true));
		if (outputExists)
		{
			std::ofstream output((directory / "download" / "part152.rar").string(), std::ios::binary);
			output << "AAAA........BBBB";
		}
		nzb->GetCompletedFiles()->emplace_back(fileId, "part152.rar", "original-name",
			CompletedFile::cfPartial, 0, false, "", "");
		queue.GetHistory()->push_back(std::make_unique<HistoryInfo>(std::move(nzbInfo)));
	}

	void RetryFailed()
	{
		IdList ids = {nzb->GetId()};
		BOOST_REQUIRE(history.EditList(&queue, &ids, DownloadQueue::eaHistoryRetryFailed, nullptr));
	}
};

} // namespace

BOOST_FIXTURE_TEST_CASE(HistoryRetryMissingNzbArticlesProceedsToPostprocess, HistoryRetryFixture)
{
	// Requeueing a partial file with no failed/undefined article would strand
	// the collection forever: missing NZB entries have no message IDs to retry.
	SavePartialFile(false);
	RetryFailed();
	BOOST_CHECK(nzb->GetFileList()->empty());
	BOOST_REQUIRE_EQUAL(nzb->GetCompletedFiles()->size(), 1u);
	BOOST_CHECK_EQUAL(nzb->GetCompletedFiles()->front().GetId(), fileId);
	BOOST_CHECK_EQUAL(nzb->GetCompletedFiles()->front().GetStatus(), CompletedFile::cfPartial);
	BOOST_CHECK_EQUAL(nzb->GetSuccessArticles(), 2);
	BOOST_CHECK_EQUAL(nzb->GetFailedArticles(), 2);
	BOOST_CHECK_EQUAL(nzb->GetSuccessSize(), 8);
	BOOST_CHECK_EQUAL(nzb->GetFailedSize(), 8);
	BOOST_CHECK(nzb->IsDownloadCompleted(true));
	BOOST_CHECK(nzb->GetPostInfo() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(HistoryRetryFailedArticleStillDownloads, HistoryRetryFixture)
{
	// A real failed entry must remain retryable even alongside a missing NZB entry.
	SavePartialFile(true);
	RetryFailed();
	BOOST_CHECK(nzb->GetCompletedFiles()->empty());
	BOOST_REQUIRE_EQUAL(nzb->GetFileList()->size(), 1u);
	FileInfo* file = nzb->GetFileList()->at(0).get();
	BOOST_CHECK_EQUAL(file->GetRemainingSize(), 4);
	BOOST_CHECK(nzb->GetPostInfo() == nullptr);
	BOOST_REQUIRE(g_DiskState->LoadArticles(file));
	BOOST_REQUIRE(g_DiskState->LoadFileState(file, g_ServerPool->GetServers(), true));
	BOOST_REQUIRE_EQUAL(file->GetArticles()->size(), 3u);
	BOOST_CHECK_EQUAL(file->GetArticles()->at(0)->GetStatus(), ArticleInfo::aiFinished);
	BOOST_CHECK_EQUAL(file->GetArticles()->at(1)->GetStatus(), ArticleInfo::aiUndefined);
	BOOST_CHECK_EQUAL(file->GetArticles()->at(2)->GetStatus(), ArticleInfo::aiFinished);
	BOOST_CHECK_EQUAL(file->GetCompletedArticles(), 2);
	BOOST_CHECK_EQUAL(nzb->GetCurrentSuccessArticles(), 2);
	BOOST_CHECK_EQUAL(nzb->GetCurrentFailedArticles(), 1);
}

BOOST_FIXTURE_TEST_CASE(HistoryRetryMissingNzbArticlesIgnoresPausedRecoveryFiles, HistoryRetryFixture)
{
	SavePartialFile(false);
	auto sparePar = std::make_unique<FileInfo>();
	sparePar->SetNzbInfo(nzb);
	sparePar->SetFilename("release.vol00+01.par2");
	sparePar->SetParFile(true);
	sparePar->SetSize(4);
	sparePar->SetPaused(true);
	nzb->GetFileList()->Add(std::move(sparePar), false);
	RetryFailed();
	BOOST_REQUIRE_EQUAL(nzb->GetFileList()->size(), 1u);
	BOOST_CHECK(nzb->GetFileList()->at(0)->GetParFile());
	BOOST_CHECK(nzb->GetFileList()->at(0)->GetPaused());
	BOOST_CHECK(nzb->IsDownloadCompleted(true));
	BOOST_CHECK(nzb->GetPostInfo() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(HistoryRetryMissingOutputStillDownloads, HistoryRetryFixture)
{
	// Without the completed disk file, successful articles must be fetched again.
	SavePartialFile(false, false);
	RetryFailed();
	BOOST_CHECK(nzb->GetCompletedFiles()->empty());
	BOOST_REQUIRE_EQUAL(nzb->GetFileList()->size(), 1u);
	BOOST_CHECK_EQUAL(nzb->GetFileList()->at(0)->GetRemainingSize(), 8);
	BOOST_CHECK(nzb->GetPostInfo() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
