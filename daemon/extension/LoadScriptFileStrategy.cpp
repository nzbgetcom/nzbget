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
#include "ManifestFile.h"
#include "Util.h"
#include "ScriptConfig.h"

namespace LoadScriptFileStrategy
{
	const char* BEGIN_SCRIPT_SIGNATURE = "### NZBGET ";
	const char* POST_SCRIPT_SIGNATURE = "POST-PROCESSING";
	const char* SCAN_SCRIPT_SIGNATURE = "SCAN";
	const char* QUEUE_SCRIPT_SIGNATURE = "QUEUE";
	const char* SCHEDULER_SCRIPT_SIGNATURE = "SCHEDULER";
	const char* FEED_SCRIPT_SIGNATURE = "FEED";
	const char* END_SCRIPT_SIGNATURE = " SCRIPT";
	const char* QUEUE_EVENTS_SIGNATURE = "### QUEUE EVENTS:";
	const char* TASK_TIME_SIGNATURE = "### TASK TIME:";
	const char* DEFINITION_SIGNATURE = "###";

	const int BEGIN_SINGNATURE_LEN = strlen(BEGIN_SCRIPT_SIGNATURE);
	const int QUEUE_EVENTS_SIGNATURE_LEN = strlen(QUEUE_EVENTS_SIGNATURE);
	const int TASK_TIME_SIGNATURE_LEN = strlen(TASK_TIME_SIGNATURE);
	const int DEFINITION_SIGNATURE_LEN = strlen(DEFINITION_SIGNATURE);

	bool HeaderConfigBased::Load(Script& script) const
	{
		DiskFile infile;
		if (!infile.Open(script.GetLocation(), DiskFile::omRead))
		{
			return false;
		}

		CharBuffer buffer(1024 * 10 + 1);

		// check if the file contains pp-script-signature
		// read first 10KB of the file and look for signature
		int readBytes = (int)infile.Read(buffer, buffer.Size() - 1);
		infile.Close();
		buffer[readBytes] = '\0';

		bool postScript = false;
		bool scanScript = false;
		bool queueScript = false;
		bool schedulerScript = false;
		bool feedScript = false;
		char* queueEvents = nullptr;
		char* taskTime = nullptr;

		bool inConfig = false;
		bool afterConfig = false;

		// Declarations "QUEUE EVENT:" and "TASK TIME:" can be placed:
		// - in script definition body (between opening and closing script signatures);
		// - immediately before script definition (before opening script signature);
		// - immediately after script definition (after closing script signature).
		// The last two pissibilities are provided to increase compatibility of scripts with older
		// nzbget versions which do not expect the extra declarations in the script defintion body.

		Tokenizer tok(buffer, "\n\r", true);
		while (char* line = tok.Next())
		{
			if (!strncmp(line, QUEUE_EVENTS_SIGNATURE, QUEUE_EVENTS_SIGNATURE_LEN))
			{
				queueEvents = line + QUEUE_EVENTS_SIGNATURE_LEN;
			}
			else if (!strncmp(line, TASK_TIME_SIGNATURE, TASK_TIME_SIGNATURE_LEN))
			{
				taskTime = line + TASK_TIME_SIGNATURE_LEN;
			}

			bool header = !strncmp(line, DEFINITION_SIGNATURE, DEFINITION_SIGNATURE_LEN);
			if (!header && !inConfig)
			{
				queueEvents = nullptr;
				taskTime = nullptr;
			}

			if (!header && afterConfig)
			{
				break;
			}

			if (!strncmp(line, BEGIN_SCRIPT_SIGNATURE, BEGIN_SINGNATURE_LEN) && strstr(line, END_SCRIPT_SIGNATURE))
			{
				if (!inConfig)
				{
					inConfig = true;
					postScript = strstr(line, POST_SCRIPT_SIGNATURE);
					scanScript = strstr(line, SCAN_SCRIPT_SIGNATURE);
					queueScript = strstr(line, QUEUE_SCRIPT_SIGNATURE);
					schedulerScript = strstr(line, SCHEDULER_SCRIPT_SIGNATURE);
					feedScript = strstr(line, FEED_SCRIPT_SIGNATURE);
				}
				else
				{
					afterConfig = true;
				}
			}
		}

		if (!(postScript || scanScript || queueScript || schedulerScript || feedScript))
		{
			return false;
		}

		// trim decorations
		char* p;
		while (queueEvents && *queueEvents && *(p = queueEvents + strlen(queueEvents) - 1) == '#') *p = '\0';
		if (queueEvents) queueEvents = Util::Trim(queueEvents);
		while (taskTime && *taskTime && *(p = taskTime + strlen(taskTime) - 1) == '#') *p = '\0';
		if (taskTime) taskTime = Util::Trim(taskTime);

		script.SetPostScript(postScript);
		script.SetScanScript(scanScript);
		script.SetQueueScript(queueScript);
		script.SetSchedulerScript(schedulerScript);
		script.SetFeedScript(feedScript);
		script.SetQueueEvents(queueEvents);
		script.SetTaskTime(taskTime);

		return true;
	}

	ManifestBased::ManifestBased(ManifestFile::Manifest&& manifest_)
		: manifest{std::move(manifest_)} { }

	bool ManifestBased::Load(Script& script) const
	{
		script.SetDisplayName(manifest.displayName);
		return true;
	}

	std::unique_ptr<const Strategy> Factory::Create(const char* dir)
	{
		ManifestFile::Manifest manifest;
		if (ManifestFile::Load(manifest, dir))
			return std::make_unique<ManifestBased>(std::move(manifest));
		
		return std::make_unique<HeaderConfigBased>();
	}
}
