/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

#include "nzbget.h"
#include "LoadScriptFileStrategy.h"
#include "FileSystem.h"
#include "Util.h"
#include "ScriptConfig.h"

namespace LoadScriptFileStrategy
{
	bool HeaderConfigBased::Load(Script& script) const
	{
		// DiskFile infile;
		// if (!infile.Open(script.GetLocation(), DiskFile::omRead))
		// {
		// 	return false;
		// }

		// CharBuffer buffer(1024 * 10 + 1);

		// const int beginSignatureLen = strlen(BEGIN_SCRIPT_SIGNATURE);
		// const int queueEventsSignatureLen = strlen(QUEUE_EVENTS_SIGNATURE);
		// const int taskTimeSignatureLen = strlen(TASK_TIME_SIGNATURE);
		// const int definitionSignatureLen = strlen(DEFINITION_SIGNATURE);

		// // check if the file contains pp-script-signature
		// // read first 10KB of the file and look for signature
		// int readBytes = (int)infile.Read(buffer, buffer.Size() - 1);
		// infile.Close();
		// buffer[readBytes] = '\0';

		// bool postScript = false;
		// bool scanScript = false;
		// bool queueScript = false;
		// bool schedulerScript = false;
		// bool feedScript = false;
		// char* queueEvents = nullptr;
		// char* taskTime = nullptr;

		// bool inConfig = false;
		// bool afterConfig = false;

		// // Declarations "QUEUE EVENT:" and "TASK TIME:" can be placed:
		// // - in script definition body (between opening and closing script signatures);
		// // - immediately before script definition (before opening script signature);
		// // - immediately after script definition (after closing script signature).
		// // The last two pissibilities are provided to increase compatibility of scripts with older
		// // nzbget versions which do not expect the extra declarations in the script defintion body.

		// Tokenizer tok(buffer, "\n\r", true);
		// while (char* line = tok.Next())
		// {
		// 	if (!strncmp(line, QUEUE_EVENTS_SIGNATURE, queueEventsSignatureLen))
		// 	{
		// 		queueEvents = line + queueEventsSignatureLen;
		// 	}
		// 	else if (!strncmp(line, TASK_TIME_SIGNATURE, taskTimeSignatureLen))
		// 	{
		// 		taskTime = line + taskTimeSignatureLen;
		// 	}

		// 	bool header = !strncmp(line, DEFINITION_SIGNATURE, definitionSignatureLen);
		// 	if (!header && !inConfig)
		// 	{
		// 		queueEvents = nullptr;
		// 		taskTime = nullptr;
		// 	}

		// 	if (!header && afterConfig)
		// 	{
		// 		break;
		// 	}

		// 	if (!strncmp(line, BEGIN_SCRIPT_SIGNATURE, beginSignatureLen) && strstr(line, END_SCRIPT_SIGNATURE))
		// 	{
		// 		if (!inConfig)
		// 		{
		// 			inConfig = true;
		// 			postScript = strstr(line, POST_SCRIPT_SIGNATURE);
		// 			scanScript = strstr(line, SCAN_SCRIPT_SIGNATURE);
		// 			queueScript = strstr(line, QUEUE_SCRIPT_SIGNATURE);
		// 			schedulerScript = strstr(line, SCHEDULER_SCRIPT_SIGNATURE);
		// 			feedScript = strstr(line, FEED_SCRIPT_SIGNATURE);
		// 		}
		// 		else
		// 		{
		// 			afterConfig = true;
		// 		}
		// 	}
		// }

		// if (!(postScript || scanScript || queueScript || schedulerScript || feedScript))
		// {
		// 	return false;
		// }

		// // trim decorations
		// char* p;
		// while (queueEvents && *queueEvents && *(p = queueEvents + strlen(queueEvents) - 1) == '#') *p = '\0';
		// if (queueEvents) queueEvents = Util::Trim(queueEvents);
		// while (taskTime && *taskTime && *(p = taskTime + strlen(taskTime) - 1) == '#') *p = '\0';
		// if (taskTime) taskTime = Util::Trim(taskTime);

		// script.SetPostScript(postScript);
		// script.SetScanScript(scanScript);
		// script.SetQueueScript(queueScript);
		// script.SetSchedulerScript(schedulerScript);
		// script.SetFeedScript(feedScript);
		// script.SetQueueEvents(queueEvents);
		// script.SetTaskTime(taskTime);

		return true;
	}

	// ManifestBased::ManifestBased(ManifestFile::Manifest&& manifest_)
	// 	: manifest{std::move(manifest_)} { }

	// bool ManifestBased::Load(Script& script) const
	// {
	// 	return false;
	// }
}
