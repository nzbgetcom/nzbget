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

#ifndef SYSTEM_HEALTH_SERVICE_H
#define SYSTEM_HEALTH_SERVICE_H

#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <vector>
#include <iostream>
#include "SectionValidator.h"
#include "Options.h"
#include "NewsServer.h"
#include "FeedInfo.h"
#include "Scheduler.h"
#include "Status.h"

namespace SystemHealth
{

struct Alert
{
	std::string source;
	std::string category;
	Status status;
	std::chrono::system_clock::time_point timestamp;
};

struct HealthReport
{
	std::vector<Alert> alerts;
	std::vector<SectionReport> sections;
};

class Service
{
public:
	Service(const Options& options, const Servers& servers, const Feeds& feeds,
			const ::Scheduler::TaskList& tasks);
	HealthReport Diagnose() const;

	void ReportAlert(Alert alert);

private:
	const Options& m_options;
	const Servers& m_servers;
	const Feeds& m_feeds;
	const ::Scheduler::TaskList& m_tasks;

	std::vector<std::unique_ptr<SectionValidator>> m_validators;
	std::vector<Alert> m_alerts;

	mutable std::mutex m_mutex;
};

void Log(const HealthReport& report);
void Log(const SectionReport& sectionReport);
void Log(const Alert& alert);

Json::JsonObject ToJson(const Alert& alert);
Xml::XmlNodePtr ToXml(const Alert& alert);
std::string ToJsonStr(const HealthReport& report);
std::string ToXmlStr(const HealthReport& report);
}  // namespace SystemHealth

extern SystemHealth::Service* g_SystemHealth;

#endif
