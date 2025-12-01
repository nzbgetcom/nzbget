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

#include "Service.h"
#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include "SystemHealth.h"

// using namespace SystemHealth;

// struct HealthFixture
// {
//     Service service;

//     SystemAlert CreateAlert(std::string source, std::string category, Status::Level level, std::string msg)
//     {
//         Alert alert;
//         alert.source = source;
//         alert.category = category;
//         alert.status = (level == Status::Error) ? Status::Error(msg) :
//                        (level == Status::Warning) ? Status::Warning(msg) :
//                        Status::Ok();
//         alert.timestamp = std::time(nullptr);
//         return alert;
//     }
// };


// BOOST_FIXTURE_TEST_SUITE(HealthLogicSuite, HealthFixture)


// BOOST_AUTO_TEST_CASE(ReportSingleAlert_AppearsInDiagnose)
// {
//     auto alert = CreateAlert("Disk", "Space", Status::Error, "Disk full");

//     service.ReportAlert(alert);
//     HealthReport report = service.Diagnose( /* dummy state */ );

//     // 1. Check Global Status
//     // Error should propagate to the top
//     BOOST_CHECK_EQUAL(report.status, false); 

//     // 2. Check the alerts list
//     BOOST_REQUIRE_EQUAL(report.systemAlerts.size(), 1);
//     BOOST_CHECK_EQUAL(report.systemAlerts[0].source, "Disk");
//     BOOST_CHECK_EQUAL(report.systemAlerts[0].status.message, "Disk full");
//     BOOST_CHECK_EQUAL(report.systemAlerts[0].status.level, Status::Error);
// }

// // Test Case 2: Deduplication (Updating existing alert)
// // Reporting the same source+category twice should update the timestamp, not add a new entry.
// BOOST_AUTO_TEST_CASE(ReportDuplicateAlert_UpdatesTimestampOnly)
// {
//     auto alert1 = CreateAlert("Network", "Conn", Status::Error, "Timeout");
    
//     // Simulate time passing (ensure timestamp changes)
//     std::this_thread::sleep_for(std::chrono::seconds(1));
//     auto alert2 = CreateAlert("Network", "Conn", Status::Error, "Timeout");

//     service.ReportAlert(alert1);
//     service.ReportAlert(alert2); // Should update, not add

//     // Get internal state (via Diagnose or GetActiveAlerts if you exposed it)
//     HealthReport report = service.Diagnose( /* dummy */ );

//     BOOST_CHECK_EQUAL(report.systemAlerts.size(), 1); // Still 1
    
//     // Verify it used the data from the second alert (newer timestamp)
//     BOOST_CHECK(report.systemAlerts[0].timestamp >= alert2.timestamp);
// }

// // Test Case 3: Recovery (Auto-Cleanup)
// // Reporting 'Ok' should remove the error from the list.
// BOOST_AUTO_TEST_CASE(ReportOk_ClearsExistingError)
// {
//     // Arrange: System is broken
//     service.ReportAlert(CreateAlert("DB", "Conn", Status::Error, "Lost"));
    
//     HealthReport initial = service.Diagnose( /* dummy */ );
//     BOOST_REQUIRE_EQUAL(initial.systemAlerts.size(), 1);

//     // Act: System recovers
//     service.ReportAlert(CreateAlert("DB", "Conn", Status::Ok, "Connected"));

//     // Assert: Error is gone
//     HealthReport finalReport = service.Diagnose( /* dummy */ );
//     BOOST_CHECK_EQUAL(finalReport.systemAlerts.size(), 0);
//     BOOST_CHECK_EQUAL(finalReport.isValid, true);
// }

// // Test Case 4: Aggregation
// // Ensure distinct sources don't overwrite each other.
// BOOST_AUTO_TEST_CASE(MultipleDistinctAlerts_AreAggregated)
// {
//     service.ReportAlert(CreateAlert("Disk", "Space", Status::Warning, "90%"));
//     service.ReportAlert(CreateAlert("Net", "Speed", Status::Error, "Slow"));

//     HealthReport report = service.Diagnose( /* dummy */ );

//     BOOST_CHECK_EQUAL(report.systemAlerts.size(), 2);
//     BOOST_CHECK_EQUAL(report.isValid, false); // Because one is Error
// }

// BOOST_AUTO_TEST_SUITE_END()