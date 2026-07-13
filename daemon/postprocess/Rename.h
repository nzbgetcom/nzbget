/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef RENAME_H
#define RENAME_H

#include "Thread.h"
#include "DownloadInfo.h"
#include "ScriptController.h"
#include "RarRenamer.h"

#ifndef DISABLE_PARCHECK
#include "ParRenamer.h"
#endif

class RenameController final : public Thread, public ScriptController
{
public:
	enum EJobKind
	{
		jkPar,
		jkRar
	};

	RenameController();
	void Run() override;
	static void StartJob(PostInfo* postInfo, EJobKind kind);

protected:
	void AddMessage(Message::EKind kind, const char* text) override;

private:
	PostInfo* m_postInfo = nullptr;
	CString m_destDir;
	int m_renamedCount = 0;
	EJobKind m_kind;

#ifndef DISABLE_PARCHECK
	class PostParRenamer final : public ParRenamer
	{
	protected:
		void UpdateProgress() override { m_owner->UpdateParRenameProgress(); }
		void PrintMessage(Message::EKind kind, const char* format, ...) override PRINTF_SYNTAX(3);
		void RegisterParredFile(const char* filename) override
			{ m_owner->m_postInfo->GetParredFiles()->push_back(filename); }
		void RegisterRenamedFile(const char* oldFilename, const char* newFileName) override
			{ m_owner->RegisterRenamedFile(oldFilename, newFileName); }
		bool IsStopped() override { return m_owner->IsStopped(); }
	private:
		RenameController* m_owner = nullptr;
		friend class RenameController;
	};

	PostParRenamer m_parRenamer;

	void UpdateParRenameProgress();
#endif

	class PostRarRenamer final : public RarRenamer
	{
	protected:
		void UpdateProgress() override { m_owner->UpdateRarRenameProgress(); }
		void PrintMessage(Message::EKind kind, const char* format, ...) override PRINTF_SYNTAX(3);
		void RegisterRenamedFile(const char* oldFilename, const char* newFilename) override
			{ m_owner->RegisterRenamedFile(oldFilename, newFilename); }
		bool IsStopped() override { return m_owner->IsStopped(); }
	private:
		RenameController* m_owner = nullptr;
		friend class RenameController;
	};

	PostRarRenamer m_rarRenamer;

	void UpdateRarRenameProgress();

	void ExecRename(const char* destDir, const char* finalDir, const char* nzbName);
	void RenameCompleted();
	void RegisterRenamedFile(const char* oldFilename, const char* newFilename);
};

#endif
