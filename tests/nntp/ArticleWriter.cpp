#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <fstream>
#include "ArticleWriter.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(ForcedDirectWriteKeepsExistingOutputInCollectionDirectory)
{
	struct Fixture
	{
		Options* previousOptions = g_Options;
		ArticleCache* previousCache = g_ArticleCache;
		fs::path directory = fs::temp_directory_path() / fs::make_unique_filename();
		std::unique_ptr<Options> options;
		ArticleCache cache;
		Fixture()
		{
			fs::create_directories(directory);
			Options::CmdOptList cmdOpts;
			cmdOpts.emplace_back("NzbLog=no");
			cmdOpts.emplace_back("WriteLog=none");
			options = std::make_unique<Options>(&cmdOpts, nullptr);
			g_ArticleCache = &cache;
		}
		~Fixture()
		{
			g_ArticleCache = previousCache;
			options.reset();
			g_Options = previousOptions;
			fs::error_code ec;
			fs::remove_all(directory, ec);
		}
	} fixture;

	const fs::path outputPath = fixture.directory / "part151.rar";
	{
		std::ofstream output(outputPath.string(), std::ios::binary);
		output << "existing archive bytes";
	}
	NzbInfo nzb;
	nzb.SetName("Release title");
	nzb.SetDestDir(fixture.directory.string().c_str());
	FileInfo file;
	file.SetNzbInfo(&nzb);
	file.SetFilename("part151.rar");
	file.SetForceDirectWrite(true);
	file.SetOutputInitialized(true);
	file.SetOutputFilename(outputPath.string());
	ArticleInfo article;
	article.SetPartNumber(1);
	ArticleWriter writer;
	writer.SetFileInfo(&file);
	writer.SetArticleInfo(&article);
	writer.Prepare();
	DiskFile output;
	auto paths = writer.SetupOutputFile(output, fixture.directory.string(),
		"part151.rar", "Release title/part151.rar", true, false);
	BOOST_REQUIRE(paths);
	output.Close();
	// The diagnostic label contains the release title, but the output path
	// must address the actual file being resumed inside the collection.
	BOOST_CHECK_EQUAL(paths->finalPath, outputPath.string());
	BOOST_CHECK(writer.CleanupOldData(true, fixture.directory.string(), paths->finalPath));
	BOOST_CHECK(fs::exists(outputPath));
	BOOST_CHECK(!fs::exists(fixture.directory / "Release title"));
	std::ifstream result(outputPath.string(), std::ios::binary);
	std::string content((std::istreambuf_iterator<char>(result)), std::istreambuf_iterator<char>());
	BOOST_CHECK_EQUAL(content, "existing archive bytes");
}

BOOST_AUTO_TEST_SUITE_END()
