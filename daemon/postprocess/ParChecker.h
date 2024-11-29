/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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


#ifndef PARCHECKER_H
#define PARCHECKER_H

#ifndef DISABLE_PARCHECK

#include <string>
#include <vector>

#include <par2/par2repairer.h>

#include "NString.h"
#include "Container.h"
#include "FileSystem.h"
#include "Log.h"

class Repairer;

class ParChecker
{
public:
	virtual ~ParChecker() = default;

	enum EStatus
	{
		psFailed,
		psRepairPossible,
		psRepaired,
		psRepairNotNeeded
	};

	enum EStage
	{
		ptLoadingPars,
		ptVerifyingSources,
		ptRepairing,
		ptVerifyingRepaired,
	};

	class AbstractRepairer
	{
	public:
		virtual ~AbstractRepairer() {};
		virtual Repairer* GetRepairer() = 0;
	};

	void Execute();
	void SetDestDir(const char* destDir) { m_destDir = destDir ? destDir : ""; }
	const char* GetParFilename() { return m_parFilename.c_str(); }
	const char* GetInfoName() { return m_infoName.c_str(); }
	void SetInfoName(const char* infoName) { m_infoName = infoName ? infoName : ""; }
	void SetNzbName(const char* nzbName) { m_nzbName = nzbName ? nzbName : ""; }
	void SetParQuick(bool parQuick) { m_parQuick = parQuick; }
	bool GetParQuick() { return m_parQuick; }
	void SetForceRepair(bool forceRepair) { m_forceRepair = forceRepair; }
	bool GetForceRepair() { return m_forceRepair; }
	void SetParFull(bool parFull) { m_parFull = parFull; }
	bool GetParFull() { return m_parFull; }
	EStatus GetStatus() { return m_status; }
	void AddParFile(const char* parFilename);
	void QueueChanged();
	void Cancel();

protected:
	class Segment
	{
	public:
		Segment(bool success, int64 offset, int size, uint32 crc) 
			: m_success(success)
			, m_offset(offset)
			, m_size(size)
			, m_crc(crc) 
		{}
		bool GetSuccess() { return m_success; }
		int64 GetOffset() { return m_offset; }
		int GetSize() { return m_size; }
		uint32 GetCrc() { return m_crc; }

	private:
		bool m_success;
		int64 m_offset;
		int m_size;
		uint32 m_crc;
	};

	typedef std::deque<Segment> SegmentList;

	class DupeSource
	{
	public:
		DupeSource(int id, const char* directory) 
			: m_id(id)
			, m_directory(directory ? directory : "") 
		{}
		int GetId() { return m_id; }
		const char* GetDirectory() { return m_directory.c_str(); }
		int GetUsedBlocks() { return m_usedBlocks; }
		void SetUsedBlocks(int usedBlocks) { m_usedBlocks = usedBlocks; }

	private:
		int m_id;
		std::string m_directory;
		int m_usedBlocks = 0;
	};

	typedef std::deque<DupeSource> DupeSourceList;

	enum EFileStatus
	{
		fsUnknown,
		fsSuccess,
		fsPartial,
		fsFailure
	};

	/**
	* Unpause par2-files
	* returns true, if the files with required number of blocks were unpaused,
	* or false if there are no more files in queue for this collection or not enough blocks
	*/
	virtual bool RequestMorePars(int blockNeeded, int* blockFound) = 0;
	virtual void UpdateProgress() {}
	virtual bool IsStopped() { return false; };
	virtual void Completed() {}
	virtual void PrintMessage(Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(3) {}
	virtual void RegisterParredFile(const char* filename) {}
	virtual bool IsParredFile(const char* filename) { return false; }
	virtual EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments) { return fsUnknown; }
	virtual const char* FindFileOrigname(const char* filename) { return nullptr; }
	virtual void RequestDupeSources(DupeSourceList* dupeSourceList) {}
	virtual void StatDupeSources(DupeSourceList* dupeSourceList) {}
	EStage GetStage() { return m_stage; }
	const char* GetProgressLabel() { return m_progressLabel.c_str(); }
	int GetFileProgress() { return m_fileProgress; }
	int GetStageProgress() { return m_stageProgress; }

private:
	class StreamBuf : public std::streambuf
	{
	public:
		StreamBuf(ParChecker* owner, Message::EKind kind) : m_owner(owner), m_kind(kind) {}
		virtual int overflow(int ch) override;
	private:
		ParChecker* m_owner;
		Message::EKind m_kind;
		std::string m_buffer;
	};

	typedef std::deque<std::string> FileList;
	typedef std::deque<void*> SourceList;
	typedef std::vector<bool> ValidBlocks;

	bool m_queuedParFilesChanged;
	bool m_verifyingExtraFiles;
	bool m_cancelled;
	bool m_hasDamagedFiles;
	bool m_parQuick = false;
	bool m_forceRepair = false;
	bool m_parFull = false;
	int m_processedCount;
	int m_filesToRepair;
	int m_extraFiles;
	int m_quickFiles;
	int m_fileProgress;
	int m_stageProgress;
	std::string m_infoName;
	std::string m_destDir;
	std::string m_nzbName;
	std::string m_parFilename;
	std::string m_progressLabel;
	std::string m_errMsg;
	EStatus m_status = psFailed;
	EStage m_stage;
	FileList m_queuedParFiles;
	FileList m_processedFiles;
	SourceList m_sourceFiles;
	DupeSourceList m_dupeSources;
	StreamBuf m_parOutStream{this, Message::mkDetail};
	StreamBuf m_parErrStream{this, Message::mkError};
	std::ostream m_parCout{&m_parOutStream};
	std::ostream m_parCerr{&m_parErrStream};
	std::mutex m_queuedParFilesMtx;
	std::mutex m_repairerMtx;
	std::mutex m_sigFileNameMtx;
	std::mutex m_sigProgressMtx;
	std::mutex m_sigDoneMtx;

	// "m_repairer" should be of type "Par2::Par2Repairer", however to prevent the
	// including of libpar2-headers into this header-file we use an empty abstract class.
	std::unique_ptr<AbstractRepairer> m_repairer;
	Repairer* GetRepairer() { return m_repairer->GetRepairer(); }

	void Cleanup();
	EStatus RunParCheckAll();
	EStatus RunParCheck(std::string parFilename);
	int PreProcessPar();
	bool LoadMainParBak();
	int ProcessMorePars();
	bool LoadMorePars();
	bool AddSplittedFragments();
	bool AddMissingFiles();
	bool AddDupeFiles();
	bool AddExtraFiles(bool onlyMissing, bool externalDir, const char* directory);
	bool IsProcessedFile(const char* filename);
	void SaveSourceList();
	void DeleteLeftovers();
	void SignalFilename(std::string str);
	void SignalProgress(int progress);
	void SignalDone(std::string str, int available, int total);
	EFileStatus VerifyDataFile(Par2::DiskFile& diskFile, Par2::Par2RepairerSourceFile& sourceFile, int& availableBlocks);
	bool VerifySuccessDataFile(Par2::DiskFile& diskFile, Par2::Par2RepairerSourceFile& sourceFile, uint32 downloadCrc);
	bool VerifyPartialDataFile(Par2::DiskFile& diskFile, Par2::Par2RepairerSourceFile& sourceFile, SegmentList& segments, ValidBlocks& validBlocks);
	void SortExtraFiles(std::vector<std::string>& extrafiles);
	bool SmartCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, SegmentList& segments, uint32& downloadCrc);
	bool DumbCalcFileRangeCrc(DiskFile& file, int64 start, int64 end, uint32& downloadCrc);
	void CheckEmptyFiles();
	std::string GetPacketCreator();
	bool MaybeSplittedFragement(const char* filename1, const char* filename2);

	friend class Repairer;
};

#endif

#endif
