/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 TheOtherP <theotherp@posteo.net>
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
#include "QueueCoordinator.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

BOOST_AUTO_TEST_CASE(PropagationWaitDisabledTest)
{
	time_t now = 1760000000;

	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(now, 0, now));
	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(now + 60, 0, now));
}

BOOST_AUTO_TEST_CASE(PropagationWaitTest)
{
	time_t now = 1760000000;
	int delay = 15 * 60;

	// posted just now or within the delay: held back
	BOOST_CHECK(QueueCoordinator::IsPropagationWait(now, delay, now));
	BOOST_CHECK(QueueCoordinator::IsPropagationWait(now - 14 * 60, delay, now));
	BOOST_CHECK(QueueCoordinator::IsPropagationWait(now - delay, delay, now));

	// posted before the delay: downloadable
	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(now - delay - 1, delay, now));
	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(now - 3600, delay, now));
}

BOOST_AUTO_TEST_CASE(PropagationWaitPerFileTest)
{
	// a collection whose files were posted at different times:
	// the older file is downloadable, the late one (e.g. an nfo or a par2) is still held back
	time_t now = 1760000000;
	int delay = 15 * 60;
	time_t mkvPosted = now - 3600;
	time_t nfoPosted = now - 60;

	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(mkvPosted, delay, now));
	BOOST_CHECK(QueueCoordinator::IsPropagationWait(nfoPosted, delay, now));
	BOOST_CHECK(!QueueCoordinator::IsPropagationWait(nfoPosted, delay, nfoPosted + delay + 1));
}

BOOST_AUTO_TEST_SUITE_END()
