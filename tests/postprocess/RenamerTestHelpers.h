#ifndef RENAMER_TEST_HELPERS_H
#define RENAMER_TEST_HELPERS_H

#include "nzbget.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "Options.h"
#include "DownloadInfo.h"
#include "PostDownloadRenamer.h"

const fs::path CURR_DIR = fs::current_path();
const std::string METANAME = "Some.Filename.1080p.WEB.H264";

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

inline int RunRename(const NzbInfo* nzbInfo, PostDownloadRenamer::Scope scope)
{
	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(const_cast<NzbInfo*>(nzbInfo));
	PostDownloadRenamer::Controller renamer;
	return PostDownloadRenamer::RenameFiles(renamer, &postInfo, scope);
}

#endif
