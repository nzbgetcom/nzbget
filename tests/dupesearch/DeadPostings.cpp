/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
#include <filesystem>
#include <fstream>
#include "DeadPostings.h"

namespace fs = std::filesystem;

namespace
{

std::vector<std::string> Ids(const char* prefix, int count, int from = 0)
{
	std::vector<std::string> ids;
	for (int i = from; i < from + count; i++)
	{
		ids.push_back(std::string(prefix) + std::to_string(i) + "@x");
	}
	return ids;
}

}

BOOST_AUTO_TEST_SUITE(DupeSearchTest)

BOOST_AUTO_TEST_CASE(DeadPostingsRememberedTest)
{
	DeadPostings dead;
	Posting::Sketch posting = Posting::MakeSketch(Ids("a", 6000));
	BOOST_CHECK(!dead.IsDead(posting));
	dead.Add(posting);
	BOOST_CHECK(dead.IsDead(posting));

	// a re-listed posting with two re-uploaded segments is the same dead posting
	std::vector<std::string> relisted = Ids("a", 5998, 2);
	relisted.push_back("z1@x");
	relisted.push_back("z2@x");
	BOOST_CHECK(dead.IsDead(Posting::MakeSketch(relisted)));
	// another posting isn't
	BOOST_CHECK(!dead.IsDead(Posting::MakeSketch(Ids("b", 6000))));
}

BOOST_AUTO_TEST_CASE(DeadPostingsExpireTest)
{
	DeadPostings dead;
	Posting::Sketch posting = Posting::MakeSketch(Ids("a", 100));
	time_t then = 1000000;
	dead.Add(posting, then);
	BOOST_CHECK(dead.IsDead(posting, then + DeadPostings::DeadTtlSec - 1));
	BOOST_CHECK(!dead.IsDead(posting, then + DeadPostings::DeadTtlSec));

	// expired entries are dropped when something is added
	dead.Add(Posting::MakeSketch(Ids("b", 100)), then + DeadPostings::DeadTtlSec + 10);
	BOOST_CHECK(!dead.IsDead(posting, then + 5));
}

BOOST_AUTO_TEST_CASE(DeadPostingsPersistTest)
{
	fs::path path = fs::temp_directory_path() / "nzbget-dead-postings-test";
	fs::remove(path);
	fs::remove(path.string() + ".new");

	Posting::Sketch posting = Posting::MakeSketch(Ids("a", 500));
	{
		DeadPostings dead;
		dead.SetStatePath(path.string());
		dead.Add(posting);
	}
	// the file was replaced in one step: no temporary file is left
	BOOST_CHECK(fs::exists(path));
	BOOST_CHECK(!fs::exists(path.string() + ".new"));

	DeadPostings reloaded;
	reloaded.SetStatePath(path.string());
	reloaded.Load();
	BOOST_CHECK(reloaded.IsDead(posting));
	BOOST_CHECK(!reloaded.IsDead(Posting::MakeSketch(Ids("b", 500))));

	// an unreadable or empty file: nothing is dead
	{
		std::ofstream file(path, std::ios::trunc);
		file << "garbage\n\t\nnot\ta\tnumber\n";
	}
	DeadPostings broken;
	broken.SetStatePath(path.string());
	broken.Load();
	BOOST_CHECK(!broken.IsDead(posting));
	fs::remove(path);
}

BOOST_AUTO_TEST_CASE(PostingSketchOfFileTest)
{
	fs::path path = fs::temp_directory_path() / "nzbget-sketch-of-file-test.nzb";
	auto write = [&](const std::string& prefix)
	{
		std::ofstream file(path, std::ios::trunc);
		file << "<?xml version=\"1.0\"?><nzb><file poster=\"p\" subject=\"&quot;a.mkv&quot;\"><segments>";
		for (int i = 0; i < 30; i++)
		{
			file << "<segment bytes=\"10\" number=\"" << i + 1 << "\">" << prefix << i << "@x</segment>";
		}
		file << "</segments></file></nzb>";
	};

	write("a");
	Posting::Sketch first;
	BOOST_REQUIRE(Posting::SketchOfFile(path.string(), first));
	BOOST_CHECK_EQUAL(first.size(), 30U);
	BOOST_CHECK(Posting::SameSketch(first, Posting::MakeSketch(Ids("a", 30))));

	// a changed file (another size) is read again, not served from the cache
	write("bb");
	Posting::Sketch second;
	BOOST_REQUIRE(Posting::SketchOfFile(path.string(), second));
	BOOST_CHECK(!Posting::SameSketch(first, second));

	// missing and malformed files
	fs::remove(path);
	BOOST_CHECK(!Posting::SketchOfFile(path.string(), second));
	{
		std::ofstream file(path, std::ios::trunc);
		file << "<html>not an nzb";
	}
	BOOST_CHECK(!Posting::SketchOfFile(path.string(), second));
	fs::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
