#ifndef RENAMER_TEST_HELPERS_H
#define RENAMER_TEST_HELPERS_H

#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "Options.h"
#include "DownloadInfo.h"
#include "PostDownloadRenamer.h"

const fs::path CURR_DIR = fs::current_path();
const std::string METANAME = "Some.Filename.1080p.WEB.H264";

struct RenamerTestFixture
{
	fs::path workingDir;
	Options::CmdOptList cmdOpts;
	Options options;

	RenamerTestFixture()
		: workingDir(CURR_DIR / boost::unit_test::framework::current_test_case().p_name.get())
		, options(&cmdOpts, nullptr)
	{
		fs::remove_all(workingDir);
		fs::create_directory(workingDir);
	}

	~RenamerTestFixture()
	{
		fs::remove_all(workingDir);
	}
};

class PostDownloadRenamerDownloadQueueMock final : public DownloadQueue
{
public:
	PostDownloadRenamerDownloadQueueMock() { Init(this); }
	bool EditEntry(int ID, EEditAction action, const char* args) { return false; }
	bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) { return false; }
	void HistoryChanged() {}
	void Save() {}
	void SaveChanged() {}
};

inline void WriteEmptyFile(const fs::path& path)
{
	std::ofstream f(path);
	f.close();
}

inline void WriteFileWithSize(const fs::path& path, size_t size)
{
	std::ofstream f(path, std::ios::binary);
	f.write(std::string(size, 'x').c_str(), size);
	f.close();
}

inline std::unique_ptr<NzbInfo> SetupNzb(const fs::path& workingDir,
	const std::vector<std::string>& completedObfuscatedNames)
{
	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(workingDir.filename().string().c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", METANAME.c_str());

	int id = 1;
	for (const std::string& name : completedObfuscatedNames)
	{
		nzbInfo->GetCompletedFiles()->emplace_back(
			id++, name, name, CompletedFile::cfSuccess, 0, false, "", "");
	}

	return nzbInfo;
}

// PostDownloadRenamer only exposes the scope-agnostic RenameFiles(Thread&, PostInfo*)
// in production: it derives which scope(s) to run from the NzbInfo's own status fields.
// To exercise a single scope in isolation, drive those same public fields so exactly one
// side is eligible, then read back the matching count from the result.
inline int RunRename(const NzbInfo* nzbInfo, PostDownloadRenamer::Scope scope)
{
	NzbInfo* mutableNzbInfo = const_cast<NzbInfo*>(nzbInfo);
	if (scope == PostDownloadRenamer::Scope::Downloaded)
	{
		// Mark the extracted (after-unpack) pass as already done, so only the
		// downloaded-files pass is eligible to run.
		mutableNzbInfo->SetPostUnpackRenamingStatus(NzbInfo::RenamingStatus::Success);
	}
	else
	{
		// Make the extracted pass eligible, and mark the downloaded-files pass as
		// already done so only the extracted pass runs.
		mutableNzbInfo->SetUnpackStatus(NzbInfo::usSuccess);
		mutableNzbInfo->SetPostRenamingStatus(NzbInfo::RenamingStatus::Success);
	}

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(mutableNzbInfo);
	PostDownloadRenamer::Controller renamer;
	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);
	return scope == PostDownloadRenamer::Scope::Downloaded ? result.downloadedCount : result.extractedCount;
}

#endif
