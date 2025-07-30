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
 *
 */


#ifndef SCANNER_H
#define SCANNER_H

#include <mutex>
#include <string>
#include "DownloadInfo.h"
#include "Thread.h"
#include "Service.h"
#include "NzbFile.h"

class Scanner final : public Service
{
public:
	enum EAddStatus
	{
		asSkipped,
		asSuccess,
		asFailed
	};

	void InitOptions();
	void ScanNzbDir(bool syncMode);
	EAddStatus AddExternalFile(
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
	);
	void InitPPParameters(const char* category, NzbParameterList* parameters, bool reset);

protected:
	int ServiceInterval() override;
	void ServiceWork() override;

private:
	class FileData
	{
	public:
		FileData(const char* filename, int64 size, time_t lastChange)
			: m_filename{ filename ? filename : "" }
			, m_size{ size }
			, m_lastChange{ lastChange } {}
		const char* GetFilename() const { return m_filename.c_str(); }
		int64 GetSize() { return m_size; }
		void SetSize(int64 size) { m_size = size; }
		time_t GetLastChange() { return m_lastChange; }
		void SetLastChange(time_t lastChange) { m_lastChange = lastChange; }
	private:
		std::string m_filename;
		int64 m_size;
		time_t m_lastChange;
	};

	using FileList = std::deque<FileData>;

	class QueueData
	{
	public:
		QueueData(
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
			int* nzbId
		);
		const char* GetFilename() const { return m_filename.c_str(); }
		const char* GetNzbName() const { return m_nzbName.c_str(); }
		const char* GetCategory() const { return m_category.c_str(); }
		int GetPriority() { return m_priority; }
		const char* GetDupeKey() const { return m_dupeKey.c_str(); }
		int GetDupeScore() { return m_dupeScore; }
		EDupeMode GetDupeMode() { return m_dupeMode; }
		NzbParameterList* GetParameters() { return &m_parameters; }
		bool GetAddTop() { return m_addTop; }
		bool GetAddPaused() { return m_addPaused; }
		bool GetAutoCategory() const { return m_autoCategory; }
		NzbInfo* GetUrlInfo() { return m_urlInfo; }
		void SetAddStatus(EAddStatus addStatus);
		void SetNzbId(int nzbId);
	private:
		NzbParameterList m_parameters;
		std::string m_filename;
		std::string m_nzbName;
		std::string m_category;
		std::string m_dupeKey;
		int m_priority;
		int m_dupeScore;
		bool m_autoCategory;
		bool m_addTop;
		bool m_addPaused;
		EDupeMode m_dupeMode;
		NzbInfo* m_urlInfo;
		EAddStatus* m_addStatus;
		int* m_nzbId;
	};

	using QueueList = std::deque<QueueData>;

	FileList m_fileList;
	QueueList m_queueList;
	std::mutex m_scanMutex;
	std::atomic<bool> m_requestedNzbDirScan{false};
	std::atomic<bool> m_scanning{false};
	static int m_idGen;
	int m_nzbDirInterval = 0;
	int m_pass = 0;
	bool m_scanScript = false;
	
	/**
	 * @brief Detects the category from the NZB file and sets it in the NzbInfo object.
	 *
	 * If a category is found in the NZB file and matches a configured category, it is set in nzbInfo.
	 * Otherwise, the detected category (if any) is set directly.
	 */
	void DetectAndSetCategory(const NzbFile& nzbFile, NzbInfo& nzbInfo, const char* nzbName);
	void CheckIncomingNzbs(const char* directory, const char* category, bool checkStat);
	bool AddFileToQueue(
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
	);
	void ProcessIncomingFile(
		const char* directory, 
		const char* baseFilename,
		const char* fullFilename
	);
	bool CanProcessFile(const char* fullFilename, bool checkStat);
	void DropOldFiles();
};

extern Scanner* g_Scanner;

#endif
