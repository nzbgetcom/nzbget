/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
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

#ifndef APP_HEALTH_H
#define APP_HEALTH_H

#include <vector>
#include <unordered_map>
#include "HealthCheck.h"
#include "Options.h"
#include "Json.h"
#include "Xml.h"
#include "Checks.h"

namespace HealthCheck
{
	void HealthMonitor::RunChecks()
	{
		m_report = CheckUp();
	}

	HealthReport HealthMonitor::CheckUp() const
	{
		HealthReport report;
		SectionReport paths;

		const auto mainDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::MAINDIR, g_Options->GetMainDir()); },
			[]() { return Checks::CheckRequiredDir(Options::MAINDIR, g_Options->GetMainDir()); },
			[]() { return Checks::Directory::Writable(Options::MAINDIR, g_Options->GetMainDir()); }
		);
		const auto destDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::DESTDIR, g_Options->GetDestDir()); },
			[]() { return Checks::CheckRequiredDir(Options::DESTDIR, g_Options->GetDestDir()); },
			[]() { return Checks::Directory::Writable(Options::DESTDIR, g_Options->GetDestDir()); },
			[]() { return Checks::CheckValueUnique(Options::DESTDIR, g_Options->GetDestDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() }
				});
			}
		);
		const auto interDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckInterDirOption(g_Options->GetInterDir()); },
			[]() { return Checks::CheckRequiredDir(Options::INTERDIR, g_Options->GetInterDir()); },
			[]() { return Checks::Directory::Writable(Options::INTERDIR, g_Options->GetInterDir()); },
			[]() { return Checks::CheckValueUnique(Options::INTERDIR, g_Options->GetInterDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() }
				});
			}
		);
		const auto nzbDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::NZBDIR, g_Options->GetNzbDir()); },
			[]() { return Checks::CheckRequiredDir(Options::NZBDIR, g_Options->GetNzbDir()); },
			[]() { return Checks::Directory::Writable(Options::NZBDIR, g_Options->GetNzbDir()); },
			[]() { return Checks::CheckValueUnique(Options::NZBDIR, g_Options->GetNzbDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() },
					{ Options::INTERDIR, g_Options->GetInterDir() }
				});
			}
		);
		const auto queueDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::QUEUEDIR, g_Options->GetQueueDir()); },
			[]() { return Checks::CheckRequiredDir(Options::QUEUEDIR, g_Options->GetQueueDir()); },
			[]() { return Checks::Directory::Writable(Options::QUEUEDIR, g_Options->GetQueueDir()); },
			[]() { return Checks::CheckValueUnique(Options::QUEUEDIR, g_Options->GetQueueDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() },
					{ Options::INTERDIR, g_Options->GetInterDir() },
					{ Options::NZBDIR, g_Options->GetNzbDir() }
				});
			}
		);
		const auto tmpDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredDir(Options::WEBDIR, g_Options->GetWebDir()); },
			[]() { return Checks::Directory::Readable(Options::WEBDIR, g_Options->GetWebDir()); },
			[]() { return Checks::CheckValueUnique(Options::WEBDIR, g_Options->GetWebDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() },
					{ Options::INTERDIR, g_Options->GetInterDir() },
					{ Options::QUEUEDIR, g_Options->GetQueueDir() },
					{ Options::NZBDIR, g_Options->GetNzbDir() },
					{ Options::TEMPDIR, g_Options->GetTempDir() }
				});
			}
		);
		const auto webDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::CheckRequiredDir(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::Directory::Writable(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::CheckValueUnique(Options::TEMPDIR, g_Options->GetTempDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() },
					{ Options::INTERDIR, g_Options->GetInterDir() },
					{ Options::QUEUEDIR, g_Options->GetQueueDir() },
					{ Options::NZBDIR, g_Options->GetNzbDir() }
				});
			}
		);
		const auto scriptDirCheck = Checks::ComposeSpecs(
			[]() { return Checks::CheckRequiredOption(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::CheckRequiredDir(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::Directory::Writable(Options::TEMPDIR, g_Options->GetTempDir()); },
			[]() { return Checks::CheckValueUnique(Options::TEMPDIR, g_Options->GetTempDir(),
				{
					{ Options::MAINDIR, g_Options->GetMainDir() },
					{ Options::DESTDIR, g_Options->GetDestDir() },
					{ Options::INTERDIR, g_Options->GetInterDir() },
					{ Options::QUEUEDIR, g_Options->GetQueueDir() },
					{ Options::NZBDIR, g_Options->GetNzbDir() },
					{ Options::WEBDIR, g_Options->GetWebDir() }
				});
			}
		);

		paths[Options::MAINDIR] = mainDirCheck();
		paths[Options::DESTDIR] = destDirCheck();
		paths[Options::INTERDIR] = interDirCheck();
		paths[Options::NZBDIR] = nzbDirCheck();
		paths[Options::QUEUEDIR] = queueDirCheck();
		paths[Options::TEMPDIR] = tmpDirCheck();
		paths[Options::WEBDIR] = webDirCheck();
		paths[Options::SCRIPTDIR] = scriptDirCheck();
		// paths[Options::LOGFILE] = Checks::CheckLogFile(g_Options->GetLogFile(), g_Options->GetWriteLog());
// 		paths[Options::CERTSTORE] = Checks::CheckCertStore(g_Options->GetCertStore(), g_Options->GetCertCheck());
// #ifndef _WIN32
// 		paths[Options::LOCKFILE] = Checks::CheckLockFile(g_Options->GetLockFile());
// #endif
		report.paths.swap(paths);

		return report;
	}

	std::string ToJsonStr(const HealthReport& report)
	{
		Json::JsonObject reportJson;
		Json::JsonObject pathsJson;

		for (const auto& [name, check] : report.paths)
		{
			pathsJson[name] = ToJson(check);
		}

		reportJson["Paths"] = std::move(pathsJson);

		return Json::serialize(reportJson);
	}

	std::string ToXmlStr(const HealthReport& report)
	{
		return "";
	}
}

#endif
