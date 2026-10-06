/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DUPECOORDINATOR_H
#define DUPECOORDINATOR_H

#include "DownloadInfo.h"

class DupeCoordinator
{
public:
	enum EDupeStatus
	{
		dsNone = 0,
		dsQueued = 1,
		dsDownloading = 2,
		dsSuccess = 4,
		dsWarning = 8,
		dsFailure = 16
	};

	void NzbCompleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbFound(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void HistoryMark(DownloadQueue* downloadQueue, HistoryInfo* historyInfo, NzbInfo::EMarkStatus markStatus);
	EDupeStatus GetDupeStatus(DownloadQueue* downloadQueue, const char* name, const char* dupeKey);
	RawNzbList ListHistoryDupes(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	static bool SameNameOrKey(const char* name1, const char* dupeKey1, const char* name2, const char* dupeKey2);
	/* The dupe-backup in history ReturnBestDupe would move to the queue for
	 * this title (nullptr if none, or if a good-duplicate already exists). */
	HistoryInfo* FindDupeBackup(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* nzbName, const char* dupeKey);
	/* Whether a queued item at the given health (permille) and duplicate score
	 * should be abandoned early for a backup with the given score (option
	 * <HealthCheck> value "dupe"): the backup's score must not fall below the
	 * score the item still warrants at its remaining health. */
	static bool DupeFailoverWarranted(int itemScore, int health, int backupScore);
	/* the download gets a duplicate in its place when it fails or turns out dead:
	 * DupeMode score; with HealthCheck=dupe a forced one too - it never fails
	 * outright while a viable duplicate waits (B46) */
	static bool FailsOver(NzbInfo* nzbInfo);
	/* With HealthCheck=dupe: a successful item of the same (non-empty) duplicate key
	 * whose files are still on disk, or nullptr (B70, B77). Within DownloadQueue-lock. */
	static NzbInfo* DownloadedOnDisk(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	/* the item's final (or destination) directory holds a file other than a side file */
	static bool FilesOnDisk(NzbInfo* nzbInfo);
	/* why no duplicate in history can take the download's place, for the log */
	std::string NoBackupReason(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);

private:
	void ReturnBestDupe(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* nzbName, const char* dupeKey);
	void HistoryCleanup(DownloadQueue* downloadQueue, HistoryInfo* markHistoryInfo);
};

extern DupeCoordinator* g_DupeCoordinator;

#endif
