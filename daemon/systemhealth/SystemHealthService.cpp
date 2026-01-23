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
#include "PathsValidator.h"
#include "IncomingNzbValidator.h"
#include "SchedulerTasksValidator.h"
#include "NewsServersValidator.h"
#include "LoggingValidator.h"
#include "ExtensionScriptsValidator.h"
#include "ConnectionValidator.h"
#include "DownloadQueueValidator.h"
#include "SecurityValidator.h"
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
Alert::Alert(Severity severity, std::string category, std::string source, std::string message)
	: m_severity(severity)
	, m_category(std::move(category))
	, m_source(std::move(source))
	, m_message(std::move(message))
	, m_timestamp{std::chrono::system_clock::now()}
{}

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
		{ return existing.GetSource() == alert.GetSource() && existing.GetCategory() == alert.GetCategory(); });

	if (alert.GetSeverity() == Severity::Ok)
	{
		if (it != m_alerts.end())
		{
			m_alerts.erase(it);
		}
		return;
	}

	if (it != m_alerts.end())
	{
		*it = std::move(alert);
	}
	else
	{
		m_alerts.push_back(std::move(alert));
	}
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
			error("[%s][%s]: %s", report.name.c_str(), optName.c_str(), status.GetMessage().c_str());
		else if (status.IsWarning())
			warn("[%s][%s]: %s", report.name.c_str(), optName.c_str(), status.GetMessage().c_str());
		else if (status.IsInfo())
			info("[%s][%s]: %s", report.name.c_str(), optName.c_str(), status.GetMessage().c_str());
	}

	for (const auto& section : report.subsections)
	{
		for (const auto& [optName, status] : section.options)
		{
			if (status.IsError())
				error("[%s][%s.%s]: %s", report.name.c_str(), section.name.c_str(), optName.c_str(),
					  status.GetMessage().c_str());
			else if (status.IsWarning())
				warn("[%s][%s.%s]: %s", report.name.c_str(), section.name.c_str(), optName.c_str(),
					 status.GetMessage().c_str());
			else if (status.IsInfo())
				info("[%s][%s.%s]: %s", report.name.c_str(), section.name.c_str(), optName.c_str(),
					 status.GetMessage().c_str());
		}
	}
}

void Log(const Alert& alert)
{
	if (alert.GetSeverity() == Severity::Error)
		error("[%s][%s]: %s", alert.GetCategory().c_str(), alert.GetSource().c_str(),
			  alert.GetMessage().c_str());
	else if (alert.GetSeverity() == Severity::Warning)
		warn("[%s][%s]: %s", alert.GetCategory().c_str(), alert.GetSource().c_str(),
			 alert.GetMessage().c_str());
	else if (alert.GetSeverity() == Severity::Info)
		info("[%s][%s]: %s", alert.GetCategory().c_str(), alert.GetSource().c_str(),
			 alert.GetMessage().c_str());
}

Json::JsonObject ToJson(const Alert& alert)
{
	Json::JsonObject alertJson;
	alertJson["Source"] = alert.GetSource();
	alertJson["Category"] = alert.GetCategory();
	alertJson["Severity"] = SeverityToStr(alert.GetSeverity());
	alertJson["Message"] = alert.GetMessage();
	alertJson["Timestamp"] = alert.GetTimestamp().time_since_epoch().count();

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

// <Alert>
//   <Source>...</Source>
//   <Category>...</Category>
//   <Severity>...</Severity>
//   <Message>...</Message>
//   <Timestamp>123456789</Timestamp>
// </Alert>
Xml::XmlNodePtr ToXml(const Alert& alert)
{
	xmlNodePtr structNode = Xml::CreateStructNode();

	Xml::AddNewNode(structNode, "Source", "string", alert.GetSource().c_str());
	Xml::AddNewNode(structNode, "Category", "string", alert.GetCategory().c_str());
	const auto severity = SeverityToStr(alert.GetSeverity());
	Xml::AddNewNode(structNode, "Severity", "string", severity.data());
	Xml::AddNewNode(structNode, "Message", "string", alert.GetMessage().c_str());

	auto ticks = std::to_string(alert.GetTimestamp().time_since_epoch().count());
	Xml::AddNewNode(structNode, "Timestamp", "i8", ticks.c_str());

	return structNode->parent;
}

std::string ToXmlStr(const HealthReport& report)
{
	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNodePtr structNode = Xml::CreateStructNode();
	xmlDocSetRootElement(doc, structNode->parent);

	std::vector<xmlNodePtr> alertNodes;
	for (const auto& alert : report.alerts)
	{
		alertNodes.push_back(ToXml(alert));
	}
	Xml::AddArrayNode(structNode, "Alerts", alertNodes);

	std::vector<xmlNodePtr> sectionNodes;
	for (const auto& section : report.sections)
	{
		sectionNodes.push_back(ToXml(section));
	}
	Xml::AddArrayNode(structNode, "Sections", sectionNodes);

	std::string result = Xml::Serialize(structNode->parent);

	xmlFreeDoc(doc);

	return result;
}

}  // namespace SystemHealth
