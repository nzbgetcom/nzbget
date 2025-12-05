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

#include <boost/test/unit_test.hpp>
#include "SystemHealth.h"

using namespace SystemHealth;

HealthReport CreateFullReport()
{
    HealthReport report;

    Alert alert;
    alert.source = "DiskMonitor";
    alert.category = "System";
    alert.status = Status::Error("Disk Full");
    alert.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(1000));
    report.alerts.push_back(alert);

    SectionReport section;
    section.name = "NewsServers";

    section.issues.push_back(Status::Warning("No active servers"));

    OptionStatus opt;
    opt.name = "Retention";
    opt.status = Status::Info("Value is 0 (Unlimited)");
    section.options.push_back(opt);

    SubsectionReport sub;
    sub.name = "Server1";
    OptionStatus subOpt;
    subOpt.name = "Host";
    subOpt.status = Status::Error("Host is required");
    sub.options.push_back(subOpt);
    
    section.subsections.push_back(sub);
    report.sections.push_back(section);

    return report;
}

BOOST_AUTO_TEST_SUITE(JsonTests)

BOOST_AUTO_TEST_CASE(TestAlertJsonStructure)
{
    Alert alert;
    alert.source = "TestSrc";
    alert.category = "TestCat";
    alert.status = Status::Error("TestMsg");
    alert.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(500));

    Json::JsonObject json = ToJson(alert);

    BOOST_CHECK_EQUAL(json["Source"].as_string(), "TestSrc");
    BOOST_CHECK_EQUAL(json["Category"].as_string(), "TestCat");
    BOOST_CHECK_EQUAL(json["Severity"].as_string(), "Error");
    BOOST_CHECK_EQUAL(json["Message"].as_string(), "TestMsg");

    BOOST_CHECK_EQUAL(json["Timestamp"], 300);
}

BOOST_AUTO_TEST_CASE(TestFullReportJson)
{
    HealthReport report = CreateFullReport();
    std::string jsonStr = ToJsonStr(report);

    BOOST_CHECK(jsonStr.find("\"Alerts\":") != std::string::npos);
    BOOST_CHECK(jsonStr.find("\"Sections\":") != std::string::npos);
    BOOST_CHECK(jsonStr.find("\"NewsServers\"") != std::string::npos);
    BOOST_CHECK(jsonStr.find("\"Server1\"") != std::string::npos);
    BOOST_CHECK(jsonStr.find("\"Disk Full\"") != std::string::npos);
    BOOST_CHECK(jsonStr.find("\"Host is required\"") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(XmlTests)

BOOST_AUTO_TEST_CASE(TestAlertXmlNode)
{
    Alert alert;
    alert.source = "XmlSrc";
    alert.category = "XmlCat";
    alert.status = Status::Error("XmlMsg");
    alert.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(123));

    Xml::XmlNodePtr node = ToXml(alert);
    
    BOOST_REQUIRE(node != nullptr);
    BOOST_CHECK_EQUAL(std::string((char*)node->name), "value");

    std::string xmlSnippet = Xml::Serialize(node);

    BOOST_CHECK(xmlSnippet.find("<name>Source</name>") != std::string::npos);
    BOOST_CHECK(xmlSnippet.find("<string>XmlSrc</string>") != std::string::npos);
    BOOST_CHECK(xmlSnippet.find("<name>Severity</name>") != std::string::npos);
    BOOST_CHECK(xmlSnippet.find("<string>Error</string>") != std::string::npos);

    xmlFreeNode(node); 
}

BOOST_AUTO_TEST_CASE(TestFullReportXmlStr)
{
    HealthReport report = CreateFullReport();
    std::string xmlStr = ToXmlStr(report);

    BOOST_CHECK(xmlStr.find("<methodResponse>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<params>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<name>Alerts</name>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<array>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<string>DiskMonitor</string>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<string>NewsServers</string>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<string>Server1</string>") != std::string::npos);
    BOOST_CHECK(xmlStr.find("<string>Host is required</string>") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
