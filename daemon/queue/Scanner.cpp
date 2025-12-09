/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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

#include <exception>
#include <fstream>
#include <ios>

#include "Scanner.h"
#include "Options.h"
#include "WorkState.h"
#include "Log.h"
#include "QueueCoordinator.h"
#include "ScanScript.h"
#include "Util.h"
#include "FileSystem.h"
#include "Unpack.h"

#ifdef _WIN32
#include "Utf8.h"
#endif

namespace fs = boost::filesystem;

int Scanner::m_idGen = 0;

Scanner::QueueData::QueueData(
	const char* filename,
	const char* nzbName,
	const char* category,
	bool autoCategory,
	int priority,
	const char* dupeKey,
	int dupeScore,
	EDupeMode dupeMode,
	NzbParameterList* parameters,
	bool addTop,
	bool addPaused,
	NzbInfo* urlInfo,
	EAddStatus* addStatus,
	int* nzbId) 
	: m_filename{ filename ? filename : "" }
	, m_nzbName{ nzbName ? nzbName : "" }
	, m_category{ category ? category : "" }
	, m_autoCategory{ autoCategory }
	, m_priority{ priority }
	, m_dupeKey{ dupeKey ? dupeKey : "" }
	, m_dupeScore{ dupeScore }
	, m_dupeMode{ dupeMode }
	, m_addTop{ addTop }
	, m_addPaused{ addPaused }
	, m_urlInfo{ urlInfo }
	, m_addStatus{ addStatus }
	, m_nzbId{ nzbId }
{
	if (parameters)
	{
		m_parameters.CopyFrom(parameters);
	}
}

void Scanner::QueueData::SetAddStatus(EAddStatus addStatus)
{
	if (m_addStatus)
	{
		*m_addStatus = addStatus;
	}
}

void Scanner::QueueData::SetNzbId(int nzbId)
{
	if (m_nzbId)
	{
		*m_nzbId = nzbId;
	}
}


void Scanner::InitOptions()
{
	m_nzbDirInterval = 1;
	m_scanScript = ScanScriptController::HasScripts();
}

int Scanner::ServiceInterval()
{
	return m_requestedNzbDirScan ? Service::Now :
		g_Options->GetNzbDirInterval() <= 0 ? Service::Sleep :
		// g_Options->GetPauseScan() ? Service::Sleep :   // for that to work we need to react on changing of pause-state
		m_nzbDirInterval;
}

void Scanner::ServiceWork()
{
	debug("Scanner service work");

	if (!DownloadQueue::IsLoaded())
	{
		return;
	}

	m_nzbDirInterval = g_Options->GetNzbDirInterval();

	if (g_WorkState->GetPauseScan() && !m_requestedNzbDirScan)
	{
		return;
	}

	debug("Scanner service work: doing work");

	std::lock_guard<std::mutex> guard{m_scanMutex};

	CheckIncomingArchives(g_Options->GetNzbDirPath());

	// check nzbdir every g_Options->GetNzbDirInterval() seconds or if requested
	bool checkStat = !m_requestedNzbDirScan;
	m_requestedNzbDirScan = false;
	m_scanning = true;
	CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
	if (!checkStat && m_scanScript)
	{
		// if immediate scan requested, we need second scan to process files extracted by scan-scripts
		CheckIncomingNzbs(g_Options->GetNzbDir(), "", checkStat);
	}
	m_scanning = false;

	// if NzbDirFileAge is less than NzbDirInterval (that can happen if NzbDirInterval
	// is set for rare scans like once per hour) we make 4 scans:
	//   - one additional scan is necessary to check sizes of detected files;
	//   - another scan is required to check files which were extracted by scan-scripts;
	//   - third scan is needed to check sizes of extracted files.
	if (g_Options->GetNzbDirInterval() > 0 && g_Options->GetNzbDirFileAge() < g_Options->GetNzbDirInterval())
	{
		int maxPass = m_scanScript ? 3 : 1;
		if (m_pass < maxPass)
		{
			// scheduling another scan of incoming directory in NzbDirFileAge seconds.
			m_nzbDirInterval = g_Options->GetNzbDirFileAge();
			m_pass++;
		}
		else
		{
			m_pass = 0;
		}
	}

	DropOldFiles();
	m_queueList.clear();
}

void Scanner::CheckIncomingArchives(const boost::filesystem::path& dir)
{
	const auto archives = FindArchives(dir);
	UnpackArchives(archives);
}

std::vector<boost::filesystem::path> Scanner::FindArchives(const boost::filesystem::path& dir)
{
	std::vector<fs::path> archives;
	archives.reserve(4);

	for (const auto& entry : fs::recursive_directory_iterator(dir))
	{
		if (!entry.is_regular_file())
			continue;

		if (Unpack::IsArchive(entry.path()))
		{
			archives.push_back(entry.path());
		}
	}

	archives.shrink_to_fit();

	return archives;
}

void Scanner::UnpackArchives(const std::vector<boost::filesystem::path>& archives)
{
	if (archives.empty()) return;

	for (const auto& archive : archives)
	{
		const auto filename = archive.filename();
		info("Extracting %s", filename.c_str());

		try
		{
			const auto extractor = Unpack::MakeExtractor(archive, archive.parent_path(), "",
														 Unpack::OverwriteMode::Overwrite);

			const auto result = extractor->Extract();
			boost::system::error_code ec;
			if (result.success)
			{
				info("%s extracted successfully", filename.c_str());

				fs::remove(archive, ec);
				if (ec)
				{
					const auto msg = ec.message();
					error("Failed to remove %s: %s (code %d)", filename.c_str(), msg.c_str(),
						  ec.value());
				}
			}
			else
			{
				error("Failed to extract %s: %s", filename.c_str(), result.message.data());
				auto processedArchive = archive;
				const auto newExtension = archive.extension().string() + ".error";
				processedArchive.replace_extension(newExtension);
				fs::rename(archive, processedArchive, ec);
				if (ec)
				{
					const auto msg = ec.message();
					error("Failed to mark archive %s as failed: %s (code %d)", filename.c_str(),
						  msg.c_str(), ec.value());
				}
			}
		}
		catch (const std::exception& e)
		{
			error("Extraction failed: %s", e.what());
			continue;
		}
	}
}

/**
* Check if there are files in directory for incoming nzb-files
* and add them to download queue
*/
void Scanner::CheckIncomingNzbs(const char* directory, const char* category, bool checkStat)
{
	DirBrowser dir(directory);
	while (const char* filename = dir.Next())
	{
		if (filename[0] == '.')
		{
			// skip hidden files
			continue;
		}

		BString<1024> fullfilename("%s%c%s", directory, PATH_SEPARATOR, filename);
		bool isDirectory = FileSystem::DirectoryExists(fullfilename);

		if (isDirectory)
		{
			ProcessSubdirectory(fullfilename, filename, category, checkStat);
		}
		else if (!isDirectory && CanProcessFile(fullfilename, checkStat))
		{
			ProcessIncomingFile(directory, filename, fullfilename, category);
		}
	}
}

void Scanner::ProcessSubdirectory(const char* fullPath, const char* filename, const char* category, bool checkStat)
{
	const char* useCategory = filename;
	BString<1024> subCategory;
	if (strlen(category) > 0)
	{
		subCategory.Format("%s%c%s", category, PATH_SEPARATOR, filename);
		useCategory = subCategory;
	}
	CheckIncomingNzbs(fullPath, useCategory, checkStat);
}

/**
 * Only files which were not changed during last g_pOptions->GetNzbDirFileAge() seconds
 * can be processed. That prevents the processing of files, which are currently being
 * copied into nzb-directory (eg. being downloaded in web-browser).
 */
bool Scanner::CanProcessFile(const char* fullFilename, bool checkStat)
{
	const char* extension = strrchr(fullFilename, '.');
	if (!extension ||
		!strcasecmp(extension, ".queued") ||
		!strcasecmp(extension, ".error") ||
		!strcasecmp(extension, ".processed"))
	{
		return false;
	}

	if (!checkStat)
	{
		return true;
	}

	int64 size = FileSystem::FileSize(fullFilename);
	time_t current = Util::CurrentTime();
	bool canProcess = false;
	bool inList = false;

	for (FileList::iterator it = m_fileList.begin(); it != m_fileList.end(); it++)
	{
		FileData& fileData = *it;
		if (!strcmp(fileData.GetFilename(), fullFilename))
		{
			inList = true;
			if (fileData.GetSize() == size &&
				current - fileData.GetLastChange() >= g_Options->GetNzbDirFileAge())
			{
				canProcess = true;
				m_fileList.erase(it);
			}
			else
			{
				fileData.SetSize(size);
				if (fileData.GetSize() != size)
				{
					fileData.SetLastChange(current);
				}
			}
			break;
		}
	}

	if (!inList)
	{
		m_fileList.emplace_back(fullFilename, size, current);
	}

	return canProcess;
}

/**
 * Remove old files from the list of monitored files.
 * Normally these files are deleted from the list when they are processed.
 * However if a file was detected by function "CanProcessFile" once but wasn't
 * processed later (for example if the user deleted it), it will stay in the list,
 * until we remove it here.
 */
void Scanner::DropOldFiles()
{
	time_t current = Util::CurrentTime();

	m_fileList.erase(std::remove_if(m_fileList.begin(), m_fileList.end(),
		[current](FileData& fileData)
		{
			if ((current - fileData.GetLastChange() >=
				(g_Options->GetNzbDirInterval() + g_Options->GetNzbDirFileAge()) * 2) ||
				// can occur if the system clock was adjusted
				current < fileData.GetLastChange())
			{
				debug("Removing file %s from scan file list", fileData.GetFilename());
				return true;
			}
			return false;
		}),
		m_fileList.end());
}

void Scanner::ProcessIncomingFile(
	const char* directory,
	const char* baseFilename,
	const char* fullFilename,
	const char* category
)
{
	const char* extension = strrchr(baseFilename, '.');
	if (!extension)
	{
		return;
	}

	std::string nzbName;
	std::string nzbCategory = category ? category : "";
	std::string dupeKey;
	NzbParameterList parameters;
	int priority = 0;
	bool addTop = false;
	bool addPaused = false;
	bool autoCategory = nzbCategory.empty() ? true : false;
	int dupeScore = 0;
	EDupeMode dupeMode = dmScore;
	EAddStatus addStatus = asSkipped;
	QueueData* queueData = nullptr;
	NzbInfo* nzbInfo = nullptr;
	int nzbId = 0;

	for (QueueData& queueData1 : m_queueList)
	{
		if (FileSystem::SameFilename(queueData1.GetFilename(), fullFilename))
		{
			queueData = &queueData1;
			nzbName = queueData->GetNzbName();
			nzbCategory = queueData->GetCategory();
			priority = queueData->GetPriority();
			dupeKey = queueData->GetDupeKey();
			dupeScore = queueData->GetDupeScore();
			dupeMode = queueData->GetDupeMode();
			addTop = queueData->GetAddTop();
			addPaused = queueData->GetAddPaused();
			parameters.CopyFrom(queueData->GetParameters());
			nzbInfo = queueData->GetUrlInfo();
			autoCategory = queueData->GetAutoCategory();
		}
	}

	InitPPParameters(nzbCategory.c_str(), &parameters, false);

	bool exists = true;

	if (m_scanScript && strcasecmp(extension, ".nzb_processed"))
	{
		ScanScriptController::ExecuteScripts(fullFilename,
			nzbInfo, 
			directory,
			nzbName,
			nzbCategory,
			&priority,
			&parameters,
			&addTop,
			&addPaused,
			dupeKey,
			&dupeScore,
			&dupeMode
		);
		exists = FileSystem::FileExists(fullFilename);
		if (exists && strcasecmp(extension, ".nzb"))
		{
			CString bakname2;
			bool renameOK = FileSystem::RenameBak(fullFilename, "processed", false, bakname2);
			if (!renameOK)
			{
				error("Could not rename file %s to %s: %s", fullFilename, *bakname2,
					*FileSystem::GetLastErrorMessage());
			}
		}
	}

	if (!strcasecmp(extension, ".nzb_processed"))
	{
		CString renamedName;
		bool renameOK = FileSystem::RenameBak(fullFilename, "nzb", true, renamedName);
		if (renameOK)
		{
			bool added = AddFileToQueue(
				renamedName,
				nzbName.c_str(),
				nzbCategory.c_str(),
				autoCategory,
				priority,
				dupeKey.c_str(),
				dupeScore,
				dupeMode,
				&parameters,
				addTop,
				addPaused,
				nzbInfo,
				&nzbId
			);
			addStatus = added ? asSuccess : asFailed;
		}
		else
		{
			error("Could not rename file %s to %s: %s", fullFilename, *renamedName,
				*FileSystem::GetLastErrorMessage());
			addStatus = asFailed;
		}
	}
	else if (exists && !strcasecmp(extension, ".nzb"))
	{
		bool added = AddFileToQueue(
			fullFilename,
			nzbName.c_str(),
			nzbCategory.c_str(),
			autoCategory,
			priority,
			dupeKey.c_str(),
			dupeScore,
			dupeMode,
			&parameters,
			addTop,
			addPaused,
			nzbInfo,
			&nzbId
		);
		addStatus = added ? asSuccess : asFailed;
	}

	if (queueData)
	{
		queueData->SetAddStatus(addStatus);
		queueData->SetNzbId(nzbId);
	}
}

void Scanner::InitPPParameters(const char* category, NzbParameterList* parameters, bool reset)
{
	bool unpack = g_Options->GetUnpack();
	const char* extensions = g_Options->GetExtensions();

	if (!Util::EmptyStr(category))
	{
		Options::Category* categoryObj = g_Options->FindCategory(category, false);
		if (categoryObj)
		{
			unpack = categoryObj->GetUnpack();
			if (!Util::EmptyStr(categoryObj->GetExtensions()))
			{
				extensions = categoryObj->GetExtensions();
			}
		}
	}

	if (reset)
	{
		g_ExtensionManager->ForEach([&parameters](auto script)
			{
				parameters->SetParameter(BString<1024>("%s:", script->GetName()), nullptr);
			}
		);
	}

	if (!parameters->Find("*Unpack:"))
	{
		parameters->SetParameter("*Unpack:", unpack ? "yes" : "no");
	}

	if (!Util::EmptyStr(extensions))
	{
		// create pp-parameter for each post-processing or queue- script
		Tokenizer tok(extensions, ",;");
		while (const char* scriptName = tok.Next())
		{
			g_ExtensionManager->ForEach([&](auto script)
				{
					BString<1024> paramName("%s:", scriptName);
					if ((script->GetPostScript() || script->GetQueueScript()) &&
						!parameters->Find(paramName) &&
						strcmp(scriptName, script->GetName()) == 0)
					{
						parameters->SetParameter(paramName, "yes");
					}
				}
			);
		}
	}
}

CString Scanner::ResolveCategory(const char* category, const char* filename)
{
	CString useCategory = category ? category : "";
	Options::Category* categoryObj = g_Options->FindCategory(useCategory, true);
	if (categoryObj && strcmp(useCategory, categoryObj->GetName()))
	{
		useCategory = categoryObj->GetName();
		detail("Category '%s' matched to '%s' for '%s'", category, *useCategory, filename);
	}
	return useCategory;
}

void Scanner::DetectAndSetCategory(const NzbFile& nzbFile, NzbInfo& nzbInfo, const char* nzbName)
{
	const std::string& nzbCategory = nzbFile.GetCategoryFromFile();
	if (nzbCategory.empty())
	{
		detail("Auto-category: no category detected in '%s'", nzbName);
		return;
	}
	Options::Category* categoryObj = g_Options->FindCategory(nzbCategory.c_str(), true);
	if (categoryObj)
	{
		const char* category = categoryObj->GetName();
		detail("Category '%s' matched to '%s' for '%s'", category, nzbCategory.c_str(), nzbName);
		nzbInfo.SetCategory(category);
	}
	else
	{
		detail("Auto-category: using detected category '%s' for '%s'", nzbCategory.c_str(), nzbName);
		nzbInfo.SetCategory(nzbCategory.c_str());
	}
}

bool Scanner::AddFileToQueue(
	const char* filename,
	const char* nzbName,
	const char* category,
	bool autoCategory,
	int priority,
	const char* dupeKey,
	int dupeScore,
	EDupeMode dupeMode,
	NzbParameterList* parameters,
	bool addTop,
	bool addPaused,
	NzbInfo* urlInfo,
	int* nzbId
)
{
	const char* basename = FileSystem::BaseFileName(filename);

	info("Adding collection %s to queue", basename);

	NzbFile nzbFile(filename, category);
	bool ok = nzbFile.Parse();
	if (!ok)
	{
		error("Could not add collection %s to queue", basename);
	}

	CString bakname2;
	if (!FileSystem::RenameBak(filename, ok ? "queued" : "error", false, bakname2))
	{
		ok = false;
		error("Could not rename file %s to %s: %s", filename, *bakname2,
			*FileSystem::GetLastErrorMessage());
	}

	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	nzbInfo->SetQueuedFilename(bakname2);

	if (autoCategory)
	{
		DetectAndSetCategory(nzbFile, *nzbInfo, nzbName);
		InitPPParameters(nzbInfo->GetCategory(), parameters, true);
	}

	if (nzbName && strlen(nzbName) > 0)
	{
		nzbInfo->SetName(nullptr);
		nzbInfo->SetFilename(nzbName);
		nzbInfo->BuildDestDirName();
	}

	nzbInfo->SetDupeKey(dupeKey);
	nzbInfo->SetDupeScore(dupeScore);
	nzbInfo->SetDupeMode(dupeMode);
	nzbInfo->SetPriority(priority);
	if (urlInfo)
	{
		nzbInfo->SetUrl(urlInfo->GetUrl());
		nzbInfo->SetUrlStatus(urlInfo->GetUrlStatus());
		nzbInfo->SetFeedId(urlInfo->GetFeedId());
		nzbInfo->SetDupeHint(urlInfo->GetDupeHint());
		nzbInfo->SetDesiredServerId(urlInfo->GetDesiredServerId());
		nzbInfo->SetSkipScriptProcessing(urlInfo->GetSkipScriptProcessing());
		nzbInfo->SetSkipDiskWrite(urlInfo->GetSkipDiskWrite());
	}

	if (!nzbFile.GetPassword().empty())
	{
		nzbInfo->GetParameters()->SetParameter("*Unpack:Password", nzbFile.GetPassword().c_str());
	}

	nzbInfo->GetParameters()->CopyFrom(parameters);

	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		fileInfo->SetPaused(addPaused);
	}

	NzbInfo* addedNzb = nullptr;

	if (ok)
	{
		addedNzb = g_QueueCoordinator->AddNzbFileToQueue(std::move(nzbInfo), std::move(urlInfo), addTop);
	}
	else if (urlInfo)
	{
		for (Message& message : nzbInfo->GuardCachedMessages())
		{
			urlInfo->AddMessage(message.GetKind(), message.GetText(), false);
		}
	}
	else
	{
		nzbInfo->SetDeleteStatus(NzbInfo::dsScan);
		addedNzb = g_QueueCoordinator->AddNzbFileToQueue(std::move(nzbInfo), std::move(urlInfo), addTop);
	}

	if (nzbId)
	{
		*nzbId = addedNzb ? addedNzb->GetId() : 0;
	}

	return ok;
}

void Scanner::ScanNzbDir(bool syncMode)
{
	{
		std::lock_guard<std::mutex> guard{m_scanMutex};
		m_scanning = true;
		m_requestedNzbDirScan = true;
		WakeUp();
	}

	while (syncMode && (m_scanning || m_requestedNzbDirScan))
	{
		Util::Sleep(100);
	}
}

Scanner::EAddStatus Scanner::AddArchive(const char* filename, const char* category,
										bool autoCategory, int priority, const char* dupeKey,
										int dupeScore, EDupeMode dupeMode,
										NzbParameterList* parameters, bool addTop, bool addPaused,
										NzbInfo* urlInfo, const char* buffer, int bufSize)
{
	if (Util::EmptyStr(filename))
	{
		error("Failed to add the archive to the download queue: Archive filename cannot be empty");
		return EAddStatus::asFailed;
	}

	if (g_Options->GetTempDirPath().empty())
	{
		error("Failed to create file '%s': '%s' is required and cannot be empty", filename,
			  Options::TEMPDIR.data());
		return EAddStatus::asFailed;
	}

	if (g_Options->GetNzbDirPath().empty())
	{
		error("Failed to create file '%s': '%s' is required and cannot be empty", filename,
			  Options::NZBDIR.data());
		return EAddStatus::asFailed;
	}

	boost::system::error_code ec;
	const auto uniqueDir = fs::unique_path("download-%%%%-%%%%", ec);
	if (ec)
	{
		const auto msg = ec.message();
		error("Failed to generate unique temporary path: %s (code %d)", msg.c_str(), ec.value());
		return EAddStatus::asFailed;
	}

	const auto downloadDir = g_Options->GetTempDirPath() / uniqueDir;
	const auto unpackDir = downloadDir / "_unpack";
#ifdef _WIN32
	const auto wfilename = Utf8::Utf8ToWide(filename);
	if (!wfilename)
	{
		error("Failed to covert '%s' to wide string", filename);
		return EAddStatus::asFailed;
	}
	const auto archiveFile = downloadDir / wfilename.value();
#else
	const auto archiveFile = downloadDir / filename;
#endif
	const auto unpackDirStr = unpackDir.string();
	const auto archiveFileStr = archiveFile.string();
	const auto downloadDirStr = downloadDir.string();

	fs::create_directories(unpackDir, ec);
	if (ec)
	{
		const std::string msg = ec.message();
		error("Could not create directory '%s': %s (code %d)", unpackDirStr.c_str(), msg.c_str(),
			  ec.value());
		return EAddStatus::asFailed;
	}

	{
		std::ofstream file(archiveFile.c_str(), std::ios::binary);
		if (!file.is_open())
		{
			error("Failed to create archive file '%s' in temporary directory: %s",
				  archiveFileStr.c_str(), std::strerror(errno));

			fs::remove_all(downloadDir, ec);
			if (ec)
			{
				const std::string msg = ec.message();
				error("Failed to remove temporary directory '%s': %s (code %d)",
					  downloadDirStr.c_str(), msg.c_str(), ec.value());
			}
			return EAddStatus::asFailed;
		}

		file.write(buffer, bufSize);
		file.close();
		if (!file)
		{
			error("Failed to write archive file to '%s': %s", archiveFileStr.c_str(),
				  std::strerror(errno));

			fs::remove_all(downloadDir, ec);
			if (ec)
			{
				const auto msg = ec.message();
				error("Failed to clean up temporary directory '%s': %s (code %d)",
					  downloadDirStr.c_str(), msg.c_str(), ec.value());
			}
			return EAddStatus::asFailed;
		}
	}

	try
	{
		info("Extracting %s", filename);

		const auto extractor = Unpack::MakeExtractor(std::move(archiveFile), unpackDir, "",
													 Unpack::OverwriteMode::Overwrite);

		const auto result = extractor->Extract();
		if (!result.success)
		{
			error("Failed to extract '%s' into '%s': %s", archiveFileStr.c_str(),
				  unpackDirStr.c_str(), result.message.data());
			fs::remove_all(downloadDir, ec);
			if (ec)
			{
				const auto msg = ec.message();
				error("Failed to remove temporary directory '%s': %s (code %d)",
					  downloadDirStr.c_str(), msg.c_str(), ec.value());
			}
			return EAddStatus::asFailed;
		}

		info("%s extracted successfully", filename);

		std::vector<fs::path> files;
		files.reserve(4);
		for (const auto& entry : fs::recursive_directory_iterator(unpackDir))
		{
			if (!entry.is_regular_file()) continue;

			files.push_back(std::move(entry.path()));
		}

		files.shrink_to_fit();

		{
			std::lock_guard<std::mutex> guard{m_scanMutex};

			for (const auto& file : files)
			{
				const auto relativePath = fs::relative(file, unpackDir, ec);
				if (ec)
				{
					const std::string msg = ec.message();
					error("Failed to calculate relative path for '%s' against '%s': %s (code %d)",
						  file.c_str(), unpackDirStr.c_str(), msg.c_str(), ec.value());
					ec.clear();
					continue;
				}
				const auto destPath = g_Options->GetNzbDirPath() / relativePath;
				const auto parentDir = destPath.parent_path();
				fs::create_directories(parentDir, ec);
				if (ec)
				{
					const std::string msg = ec.message();
					error("Failed to create destination directory '%s' for %s (code %d)",
						  parentDir.c_str(), msg.c_str(), ec.value());
					ec.clear();
					continue;
				}
				fs::rename(file, destPath, ec);
				if (ec)
				{
					const std::string msg = ec.message();
					error("Failed to move extracted NZB '%s' to '%s': %s (code %d)", file.c_str(),
						  destPath.c_str(), msg.c_str(), ec.value());
					ec.clear();
					continue;
				}

				const auto destPathStr = destPath.string();
				const auto filenameStr = destPath.filename().string();

				CString useCategory = ResolveCategory(category, filenameStr.c_str());

				m_queueList.emplace_back(destPathStr.c_str(), filenameStr.c_str(), useCategory,
										 autoCategory, priority, dupeKey, dupeScore, dupeMode,
										 parameters, addTop, addPaused, urlInfo, nullptr, nullptr);
			}
		}

		fs::remove_all(downloadDir, ec);
		if (ec)
		{
			const auto msg = ec.message();
			error("Cleanup of temporary directory '%s' failed: %s (code %d)", downloadDir.c_str(),
				  msg.c_str(), ec.value());
		}

		ScanNzbDir(true);
		return EAddStatus::asSuccess;
	}
	catch (const std::exception& e)
	{
		error("Extraction failed: %s", e.what());
		fs::remove_all(downloadDir, ec);
		if (ec)
		{
			const std::string msg = ec.message();
			error("Failed to remove temporary directory '%s': %s (code %d)", downloadDirStr.c_str(),
				  msg.c_str(), ec.value());
		}
		return EAddStatus::asFailed;
	}
}

Scanner::EAddStatus Scanner::AddArchive(
	const char* filename,
	const char* category,
	bool autoCategory,
	int priority,
	const char* dupeKey,
	int dupeScore,
	EDupeMode dupeMode,
	NzbParameterList* parameters,
	bool addTop,
	bool addPaused,
	NzbInfo* urlInfo,
	const char* tmpFilename)
{
	CharBuffer buffer;
	if (!FileSystem::LoadFileIntoBuffer(tmpFilename, buffer, false))
	{
		error("Could not open file '%s'", tmpFilename);
		return EAddStatus::asFailed;
	}
	boost::system::error_code ec;
	fs::remove(tmpFilename, ec);
	if (ec)
	{
		const auto msg = ec.message();
		error("Failed to remove '%s': %s (code %d)", tmpFilename, msg.c_str(), ec.value());
	}

	return AddArchive(
		filename,
		category,
		autoCategory,
		priority,
		dupeKey,
		dupeScore,
		dupeMode,
		parameters,
		addTop,
		addPaused,
		urlInfo,
		*buffer,
		buffer.Size()
	);
}

Scanner::EAddStatus Scanner::AddExternalFile(
	const char* nzbName,
	const char* category,
	bool autoCategory,
	int priority,
	const char* dupeKey,
	int dupeScore,
	EDupeMode dupeMode,
	NzbParameterList* parameters,
	bool addTop,
	bool addPaused,
	NzbInfo* urlInfo,
	const char* fileName,
	const char* buffer,
	int bufSize,
	int* nzbId
)
{
	bool nzb = false;
	BString<1024> tempFileName;

	if (fileName)
	{
		tempFileName = fileName;
	}
	else
	{
		int num = ++m_idGen;
		while (tempFileName.Empty() || FileSystem::FileExists(tempFileName))
		{
			tempFileName.Format("%s%cnzb-%i.tmp", g_Options->GetTempDir(), PATH_SEPARATOR, num);
			num++;
		}

		if (!FileSystem::SaveBufferIntoFile(tempFileName, buffer, bufSize))
		{
			error("Could not create file %s", *tempFileName);
			return asFailed;
		}

		// "buffer" doesn't end with nullptr, therefore we can't search in it with "strstr"
		BString<1024> buf;
		buf.Set(buffer, bufSize);
		nzb = !strncmp(buf, "<?xml", 5) && strstr(buf, "<nzb");
	}

	// move file into NzbDir, make sure the file name is unique
	CString validNzbName = FileSystem::MakeValidFilename(FileSystem::BaseFileName(nzbName));

	const char* extension = strrchr(nzbName, '.');
	if (nzb && (!extension || strcasecmp(extension, ".nzb")))
	{
		validNzbName.Append(".nzb");
	}

	BString<1024> scanFileName("%s%c%s", g_Options->GetNzbDir(), PATH_SEPARATOR, *validNzbName);

	char *ext = strrchr(validNzbName, '.');
	if (ext)
	{
		*ext = '\0';
		ext++;
	}

	int num = 2;
	while (FileSystem::FileExists(scanFileName))
	{
		if (ext)
		{
			scanFileName.Format("%s%c%s_%i.%s", g_Options->GetNzbDir(),
				PATH_SEPARATOR, *validNzbName, num, ext);
		}
		else
		{
			scanFileName.Format("%s%c%s_%i", g_Options->GetNzbDir(),
				PATH_SEPARATOR, *validNzbName, num);
		}
		num++;
	}

	EAddStatus addStatus;

	{
		std::lock_guard<std::mutex> guard{m_scanMutex};

		if (!FileSystem::MoveFile(tempFileName, scanFileName))
		{
			error("Could not move file %s to %s: %s", *tempFileName, *scanFileName,
				*FileSystem::GetLastErrorMessage());
			FileSystem::DeleteFile(tempFileName);
			return asFailed;
		}

		CString useCategory = ResolveCategory(category, nzbName);

		addStatus = asSkipped;
		m_queueList.emplace_back(
			scanFileName,
			nzbName,
			useCategory,
			autoCategory,
			priority,
			dupeKey,
			dupeScore,
			dupeMode,
			parameters,
			addTop,
			addPaused,
			urlInfo,
			&addStatus,
			nzbId
		);
	}

	ScanNzbDir(true);

	return addStatus;
}
