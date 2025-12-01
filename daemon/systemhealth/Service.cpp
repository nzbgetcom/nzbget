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

#include "nzbget.h"

#include "ExtensionManager.h"
#include "Service.h"
#include "PathsValidator.h"
#include "IncomingNzbValidator.h"
#include "SchedulerTasksValidator.h"
#include "NewsServersValidator.h"
#include "LoggingValidator.h"
#include "ExtensionScriptsValidator.h"
#include "ConnectionValidator.h"
#include "DownloadQueueValidator.h"
#include "SecurityValidator.h"
#include "IncomingNzbValidator.h"
#include "CheckAndRepairValidator.h"
#include "SchedulerTasksValidator.h"
#include "FeedsValidator.h"
#include "CategoriesValidator.h"
#include "UnpackValidator.h"
#include "DisplayValidator.h"
#include "Status.h"
#include "Json.h"
#include "Xml.h"

namespace SystemHealth
{
Service::Service(const Options& options, const Servers& servers, const ::Feeds& feeds,
				 const ::Scheduler::TaskList& tasks)
	: m_options(options), m_servers(servers), m_feeds(feeds), m_tasks(tasks)
{
	m_validators.reserve(12);
	m_validators.push_back(std::make_unique<Paths::PathsValidator>(m_options));
	m_validators.push_back(std::make_unique<NewsServers::NewsServersValidator>(m_servers));
	m_validators.push_back(std::make_unique<Display::DisplayValidator>(m_options));
	m_validators.push_back(std::make_unique<Logging::LoggingValidator>(m_options));
	m_validators.push_back(std::make_unique<ExtensionScripts::ExtensionScriptsValidator>(
		m_options, *g_ExtensionManager));
	m_validators.push_back(std::make_unique<Connection::ConnectionValidator>(m_options));
	m_validators.push_back(std::make_unique<DownloadQueue::DownloadQueueValidator>(m_options));
	m_validators.push_back(std::make_unique<Security::SecurityValidator>(m_options));
	m_validators.push_back(std::make_unique<IncomingNzb::IncomingNzbValidator>(m_options));
	m_validators.push_back(std::make_unique<CheckAndRepair::CheckAndRepairValidator>(m_options));
	m_validators.push_back(std::make_unique<Unpack::UnpackValidator>(m_options));
	m_validators.push_back(std::make_unique<Scheduler::SchedulerTasksValidator>(m_tasks));
	m_validators.push_back(
		std::make_unique<Category::CategoriesValidator>(m_options, *m_options.GetCategories()));
	m_validators.push_back(std::make_unique<Feeds::FeedsValidator>(m_feeds, m_options));
}

HealthReport Service::Diagnose() const
{
	HealthReport report;
	report.sections.reserve(m_validators.size());

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		report.alerts = m_alerts;
	}

	for (const auto& validator : m_validators)
	{
		report.sections.push_back(validator->Validate());
	}

	return report;
}

void Service::ReportAlert(Alert alert)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = std::find_if(
		m_alerts.begin(), m_alerts.end(), [&](const Alert& existing)
		{ return existing.source == alert.source && existing.category == alert.category; });

	if (alert.status.IsOk())
	{
		if (it != m_alerts.end()) m_alerts.erase(it);
		return;
	}

	if (it != m_alerts.end())
	{
		it->status = std::move(alert.status);
		it->timestamp = alert.timestamp;
		return;
	}

	m_alerts.push_back(std::move(alert));
}

void Log(const HealthReport& report)
{
	for (const auto& alert : report.alerts) Log(alert);
	for (const auto& sectionReport : report.sections) Log(sectionReport);
}

void Log(const SectionReport& report)
{
	for (const auto& issue : report.issues)
	{
		if (issue.IsError())
			error("%s", issue.GetMessage().c_str());
		else if (issue.IsWarning())
			warn("%s", issue.GetMessage().c_str());
		else if (issue.IsInfo())
			info("%s", issue.GetMessage().c_str());
	}

	for (const auto& [optName, status] : report.options)
	{
		if (status.IsError())
			error("[%s]: %s", optName.c_str(), status.GetMessage().c_str());
		else if (status.IsWarning())
			warn("[%s]: %s", optName.c_str(), status.GetMessage().c_str());
		else if (status.IsInfo())
			info("[%s]: %s", optName.c_str(), status.GetMessage().c_str());
	}

	for (const auto& section : report.subsections)
	{
		for (const auto& [optName, status] : section.options)
		{
			if (status.IsError())
				error("[%s][%s]: %s", report.name.c_str(), optName.c_str(),
					  status.GetMessage().c_str());
			else if (status.IsWarning())
				warn("[%s][%s]: %s", report.name.c_str(), optName.c_str(),
					 status.GetMessage().c_str());
			else if (status.IsInfo())
				info("[%s][%s]: %s", report.name.c_str(), optName.c_str(),
					 status.GetMessage().c_str());
		}
	}
}

void Log(const Alert& alert)
{
	if (alert.status.IsError())
		error("[%s][%s]: %s", alert.category.c_str(), alert.source.c_str(),
			  alert.status.GetMessage().c_str());
	else if (alert.status.IsWarning())
		warn("[%s][%s]: %s", alert.category.c_str(), alert.source.c_str(),
			 alert.status.GetMessage().c_str());
	else if (alert.status.IsInfo())
		info("[%s][%s]: %s", alert.category.c_str(), alert.source.c_str(),
			 alert.status.GetMessage().c_str());
}

Json::JsonObject ToJson(const Alert& alert)
{
	Json::JsonObject alertJson;
	alertJson["Source"] = alert.source;
	alertJson["Category"] = alert.category;
	alertJson["Severity"] = SeverityToStr(alert.status.GetSeverity());
	alertJson["Message"] = alert.status.GetMessage();
	alertJson["Timestamp"] = alert.timestamp.time_since_epoch().count();

	return alertJson;
}

std::string ToJsonStr(const HealthReport& report)
{
	Json::JsonObject reportJson;
	Json::JsonArray sectionsArrayJson;
	Json::JsonArray alertsArrayJson;

	for (const auto& alert : report.alerts)
	{
		alertsArrayJson.push_back(ToJson(alert));
	}

	for (const auto& section : report.sections)
	{
		sectionsArrayJson.push_back(ToJson(section));
	}

	reportJson["Alerts"] = std::move(alertsArrayJson);
	reportJson["Sections"] = std::move(sectionsArrayJson);

	return Json::serialize(reportJson);
}

}  // namespace SystemHealth
