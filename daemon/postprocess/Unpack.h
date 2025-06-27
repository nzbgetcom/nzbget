/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2018 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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


#ifndef UNPACK_H
#define UNPACK_H

#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "ScriptController.h"

class UnpackController final : public Thread, public ScriptController
{
public:
	void Run() override;
	void Stop() override;
	static void StartJob(PostInfo* postInfo);
	static bool HasCompletedArchiveFiles(NzbInfo* nzbInfo);
	static const char* DecodeSevenZipExitCode(int ec);

	~UnpackController();

protected:
	bool ReadLine(char* buf, int bufSize, FILE* stream) override;
	void AddMessage(Message::EKind kind, const char* text) override;

private:
	enum EUnpacker
	{
		upUnrar,
		upSevenZip
	};

	enum SevenZipExitCodes
	{
		NoError = 0,
		Warning = 1,
		FatalError = 2,
		CmdLineError = 7,
		NotEnoughMemoryError = 8,
		CanceledByUser = 255,
	};

	typedef std::vector<CString> FileListBase;
	class FileList : public FileListBase
	{
	public:
		bool Exists(const char* filename);
	};

	typedef std::vector<CString> ParamListBase;
	class ParamList : public ParamListBase
	{
	public:
		bool Exists(const char* param);
	};

	PostInfo* m_postInfo = nullptr;
	CString m_name;
	CString m_infoName;
	CString m_infoNameUp;
	CString m_destDir;
	CString m_finalDir;
	CString m_unpackDir;
	CString m_unpackExtendedDir;
	CString m_password;
	EUnpacker m_unpacker;
	bool m_interDir = false;
	bool m_allOkMessageReceived = false;
	bool m_noFilesMessageReceived = false;
	bool m_hasParFiles = false;
	bool m_hasRarFiles = false;
	bool m_hasNotUnpackedRarFiles = false;
	bool m_hasRenamedArchiveFiles = false;
	bool m_hasSevenZipFiles = false;
	bool m_hasSevenZipMultiFiles = false;
	bool m_hasSplittedFiles = false;
	bool m_unpackOk = false;
	bool m_unpackStartError = false;
	bool m_unpackSpaceError = false;
	bool m_unpackDecryptError = false;
	bool m_unpackPasswordError = false;
	bool m_cleanedUpDisk = false;
	bool m_autoTerminated = false;
	bool m_finalDirCreated = false;
	bool m_unpackDirCreated = false;
	bool m_passListTried = false;
	FileList m_joinedFiles;

	void ExecuteUnpack(EUnpacker unpacker, const char* password, bool multiVolumes);
	void ExecuteUnrar(const char* password);
	void ExecuteSevenZip(const char* password, bool multiVolumes);
	void UnpackArchives(EUnpacker unpacker, bool multiVolumes);
	void JoinSplittedFiles();
	bool JoinFile(const char* fragBaseName);
	void Completed();
	void CreateUnpackDir();
	bool Cleanup();
	void CheckArchiveFiles();
	void SetProgressLabel(const char* progressLabel);
#ifndef DISABLE_PARCHECK
	void RequestParCheck(bool forceRepair);
#endif
	bool FileHasRarSignature(const char* filename);
	bool PrepareCmdParams(const char* command, ParamList* params, const char* infoName);
};

#endif
