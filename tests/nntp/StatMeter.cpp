/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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

#include "StatMeter.h"
#include "DiskState.h"

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(ServerVolumeTest)
{
	ServerVolume volume;

	ServerVolume::Stats stats1 = { 100, {10, 20} };
	ServerVolume::Stats stats2 = { 200, {30, 40} };

	volume.AddStats(stats1);
	volume.AddStats(stats2);

	const ServerVolume::VolumeArray* bytesPerSeconds = volume.BytesPerSeconds();
	const ServerVolume::VolumeArray* bytesPerMinutes = volume.BytesPerMinutes();
	const ServerVolume::VolumeArray* bytesPerHours = volume.BytesPerHours();
	const ServerVolume::VolumeArray* bytesPerDays = volume.BytesPerDays();
	const ServerVolume::ArticlesArray& articlesPerDays = volume.GetArticlesPerDays();

	size_t secSlot = static_cast<size_t>(volume.GetSecSlot());
	size_t minSlot = static_cast<size_t>(volume.GetMinSlot());
	size_t hourSlot = static_cast<size_t>(volume.GetHourSlot());
	size_t daySlot = static_cast<size_t>(volume.GetDaySlot());

	const ServerVolume::Articles& articlesPerDay = articlesPerDays.at(daySlot);

	BOOST_CHECK_EQUAL(bytesPerDays->size(), 1);
	BOOST_CHECK_EQUAL(articlesPerDay.failed, 40);
	BOOST_CHECK_EQUAL(articlesPerDay.success, 60);
	BOOST_CHECK_EQUAL(bytesPerSeconds->at(secSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerMinutes->at(minSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerHours->at(hourSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerDays->at(daySlot), 300);
	BOOST_CHECK_EQUAL(volume.GetTotalBytes(), 300);
	BOOST_CHECK_EQUAL(volume.GetCustomBytes(), 300);

	int currentDay = volume.GetDaySlot();
	time_t nextDay = Util::CurrentTime() + 86400;
	volume.CalcSlots(nextDay);

	const ServerVolume::VolumeArray* bytesPerSeconds2 = volume.BytesPerSeconds();
	const ServerVolume::VolumeArray* bytesPerMinutes2 = volume.BytesPerMinutes();
	const ServerVolume::VolumeArray* bytesPerHours2 = volume.BytesPerHours();
	const ServerVolume::VolumeArray* bytesPerDays2 = volume.BytesPerDays();
	const ServerVolume::ArticlesArray& articlesPerDays2 = volume.GetArticlesPerDays();

	BOOST_CHECK_EQUAL(volume.GetDaySlot(), currentDay + 1);
	BOOST_CHECK_EQUAL(bytesPerDays2->size(), 2);
	BOOST_CHECK_EQUAL(articlesPerDays2.size(), 2);

	size_t daySlot2 = static_cast<size_t>(volume.GetDaySlot());
	const ServerVolume::Articles& articlesPerDay2 = articlesPerDays.at(daySlot);
	const ServerVolume::Articles& articlesPerDay3 = articlesPerDays.at(daySlot2);

	volume.ResetCustom();
	BOOST_CHECK_EQUAL(volume.GetCustomBytes(), 0);

	BOOST_CHECK_EQUAL(bytesPerSeconds2->at(secSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerMinutes2->at(minSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerHours2->at(hourSlot), 300);
	BOOST_CHECK_EQUAL(bytesPerDays2->at(daySlot), 300);
	BOOST_CHECK_EQUAL(bytesPerDays2->at(daySlot2), 0);
	BOOST_CHECK_EQUAL(articlesPerDay2.failed, 40);
	BOOST_CHECK_EQUAL(articlesPerDay2.success, 60);
	BOOST_CHECK_EQUAL(articlesPerDay3.failed, 0);
	BOOST_CHECK_EQUAL(articlesPerDay3.success, 0);
	BOOST_CHECK_EQUAL(volume.GetTotalBytes(), 300);

	volume.Reset();

	BOOST_CHECK_EQUAL(bytesPerSeconds2->at(secSlot), 0);
	BOOST_CHECK_EQUAL(bytesPerMinutes2->at(minSlot), 0);
	BOOST_CHECK_EQUAL(bytesPerHours2->at(hourSlot), 0);
	BOOST_CHECK_EQUAL(bytesPerDays2->at(daySlot), 0);
	BOOST_CHECK_EQUAL(articlesPerDay2.failed, 0);
	BOOST_CHECK_EQUAL(articlesPerDay2.success, 0);
	BOOST_CHECK_EQUAL(volume.GetTotalBytes(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
