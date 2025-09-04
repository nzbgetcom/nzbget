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

#include "UnpackValidator.h"
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
	report.status = Status::Severity::Ok;
	bool hasErrors = false;
	bool hasWarnings = false;
	bool hasInfo = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		// TODO: should swap?
		report.alerts = m_alerts;
	}

	for (const auto& validator : m_validators)
	{
		report.sections.push_back(validator->Validate());
	}

	for (const auto& sectionReport : report.sections)
	{
		for (const auto& [name, status] : sectionReport.alerts)
		{
			if (!status.IsOk())
			{
				if (status.IsError()) hasErrors = true;
				if (status.IsWarning()) hasWarnings = true;
				if (status.IsInfo()) hasInfo = true;
				report.alerts.push_back({name, status});
			}
		}

		for (const auto& subsectionReport : sectionReport.subsections)
		{
			for (const auto& option : subsectionReport.options)
			{
				if (!option.status.IsOk())
				{
					if (option.status.IsError()) hasErrors = true;
					if (option.status.IsWarning()) hasWarnings = true;
					if (option.status.IsInfo()) hasInfo = true;
					report.alerts.push_back({option.name, option.status});
				}
			}
		}

		for (const auto& option : sectionReport.options)
		{
			if (!option.status.IsOk())
			{
				if (option.status.IsError()) hasErrors = true;
				if (option.status.IsWarning()) hasWarnings = true;
				if (option.status.IsInfo()) hasInfo = true;
				report.alerts.push_back({option.name, option.status});
			}
		}
	}

	if (hasInfo) report.status = Status::Severity::Info;
	if (hasWarnings) report.status = Status::Severity::Warning;
	if (hasErrors) report.status = Status::Severity::Error;

	return report;
}

void Service::ReportAlert(Alert alert)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_alerts.push_back(std::move(alert));
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

	reportJson["Status"] = SeverityToStr(report.status);
	reportJson["Alerts"] = std::move(alertsArrayJson);
	reportJson["Sections"] = std::move(sectionsArrayJson);

	return Json::serialize(reportJson);
}

namespace
{
const xmlChar* XmlLiteral(const char* literal) { return reinterpret_cast<const xmlChar*>(literal); }

template <typename Collection, typename Converter>
void AppendArrayMember(xmlNodePtr structNode, const char* name, const Collection& collection,
					   Converter converter)
{
	xmlNodePtr memberNode = xmlNewNode(nullptr, XmlLiteral("member"));
	xmlNewChild(memberNode, nullptr, XmlLiteral("name"), XmlLiteral(name));

	xmlNodePtr valueNode = xmlNewNode(nullptr, XmlLiteral("value"));
	xmlNodePtr arrayNode = xmlNewNode(nullptr, XmlLiteral("array"));
	xmlNodePtr dataNode = xmlNewNode(nullptr, XmlLiteral("data"));

	for (const auto& item : collection)
	{
		xmlNodePtr valueWrapper = xmlNewNode(nullptr, XmlLiteral("value"));
		xmlAddChild(valueWrapper, converter(item));
		xmlAddChild(dataNode, valueWrapper);
	}

	xmlAddChild(arrayNode, dataNode);
	xmlAddChild(valueNode, arrayNode);
	xmlAddChild(memberNode, valueNode);
	xmlAddChild(structNode, memberNode);
}
}  // namespace

std::string ToXmlStr(const HealthReport& report)
{
	xmlNodePtr rootNode = xmlNewNode(nullptr, XmlLiteral("value"));
	xmlNodePtr structNode = xmlNewNode(nullptr, XmlLiteral("struct"));

	const auto statusStr = SeverityToStr(report.status);

	Xml::AddNewNode(structNode, "Status", "string", statusStr.data());

	AppendArrayMember(structNode, "Alerts", report.alerts,
					  [](const Alert& alert) { return ToXml(alert); });
	AppendArrayMember(structNode, "Sections", report.sections,
					  [](const SectionReport& section) { return ToXml(section); });

	xmlAddChild(rootNode, structNode);

	std::string result = Xml::Serialize(rootNode);
	xmlFreeNode(rootNode);

	return result;
}
}  // namespace SystemHealth
