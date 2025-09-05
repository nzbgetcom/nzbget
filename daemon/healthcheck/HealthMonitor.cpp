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
		std::vector<std::pair<std::string_view, Report>> sections;
		Report pathsSection;
		Report general;

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

		pathsSection[Options::MAINDIR] = mainDirCheck();
		if (!pathsSection[Options::MAINDIR].IsOk())
		{
			general[Options::MAINDIR] = pathsSection[Options::MAINDIR];
		}
		pathsSection[Options::DESTDIR] = destDirCheck();
		if (!pathsSection[Options::DESTDIR].IsOk())
		{
			general[Options::DESTDIR] = pathsSection[Options::DESTDIR];
		}
		pathsSection[Options::INTERDIR] = interDirCheck();
		if (!pathsSection[Options::INTERDIR].IsOk())
		{
			general[Options::INTERDIR] = pathsSection[Options::INTERDIR];
		}
		pathsSection[Options::NZBDIR] = nzbDirCheck();
		if (!pathsSection[Options::NZBDIR].IsOk())
		{
			general[Options::NZBDIR] = pathsSection[Options::NZBDIR];
		}
		pathsSection[Options::QUEUEDIR] = queueDirCheck();
		if (!pathsSection[Options::QUEUEDIR].IsOk())
		{
			general[Options::QUEUEDIR] = pathsSection[Options::QUEUEDIR];
		}
		pathsSection[Options::TEMPDIR] = tmpDirCheck();
		if (!pathsSection[Options::TEMPDIR].IsOk())
		{
			general[Options::TEMPDIR] = pathsSection[Options::TEMPDIR];
		}
		pathsSection[Options::WEBDIR] = webDirCheck();
		if (!pathsSection[Options::WEBDIR].IsOk())
		{
			general[Options::WEBDIR] = pathsSection[Options::WEBDIR];
		}
		pathsSection[Options::SCRIPTDIR] = scriptDirCheck();
		if (!pathsSection[Options::SCRIPTDIR].IsOk())
		{
			general[Options::SCRIPTDIR] = pathsSection[Options::SCRIPTDIR];
		}
		// pathsSection[Options::LOGFILE] = Checks::CheckLogFile(g_Options->GetLogFile(), g_Options->GetWriteLog());
// 		pathsSection[Options::CERTSTORE] = Checks::CheckCertStore(g_Options->GetCertStore(), g_Options->GetCertCheck());
// #ifndef _WIN32
// 		pathsSection[Options::LOCKFILE] = Checks::CheckLockFile(g_Options->GetLockFile());
// #endif
		sections.push_back({ "Paths", std::move(pathsSection) });

		report.sections.swap(sections);
		report.general.swap(general);

		return report;
	}

	std::string ToJsonStr(const HealthReport& report)
	{
		Json::JsonObject reportJson;
		Json::JsonObject sectionsJson;
		Json::JsonObject generalJson;

		for (auto& [sectionName, section] : report.sections)
		{
			Json::JsonObject sectionJson;
			for (const auto& [name, check] : section)
			{
				sectionJson[name] = ToJson(check);
			}

			sectionsJson[sectionName] = std::move(sectionJson);
		}

		for (const auto& [name, check] : report.general)
		{
			generalJson[name] = ToJson(check);
		}

		reportJson["Sections"] = std::move(sectionsJson);
		reportJson["General"] = std::move(generalJson);

		return Json::serialize(reportJson);
	}

	std::string ToXmlStr(const HealthReport& report)
	{
		return "";
	}
}

#endif
