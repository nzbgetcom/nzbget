/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2023-2026 Denis <denis@nzbget.com>
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
#include <fstream>
#include "Options.h"
#include "NzbFile.h"
#include "DownloadInfo.h"
#include "FileSystem.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

const fs::path CURR_DIR = fs::current_path();

void TestNzb(std::string testFilename, std::string expectedCategory)
{
	const fs::path nzbFilename = CURR_DIR / "nzbfile" / (testFilename + ".nzb");
	const fs::path infoFilename = CURR_DIR / "nzbfile" / (testFilename + ".txt");

	NzbFile nzbFile(nzbFilename.string().c_str(), "");
	BOOST_REQUIRE(nzbFile.Parse());

	BOOST_CHECK_EQUAL(expectedCategory, nzbFile.GetCategoryFromFile());

	FILE* infofile = fopen(infoFilename.string().c_str(), FOPEN_RB);
	BOOST_REQUIRE(infofile);
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
	BOOST_REQUIRE(*buffer);

	int fileCount = atoi(buffer);
	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	BOOST_CHECK_EQUAL(nzbInfo->GetFileCount(), fileCount);
	char lastBuffer[1024];

	for (int i = 0; i < fileCount; i++)
	{
		while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
		BOOST_REQUIRE(*buffer);
		FileInfo* fileInfo = nzbInfo->GetFileList()->at(i).get();
		BOOST_REQUIRE(fileInfo);
		Util::TrimRight(buffer);
		BOOST_CHECK_EQUAL(std::string(fileInfo->GetFilename()), std::string(buffer));
		memcpy(lastBuffer, buffer, sizeof(buffer));
	}

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;

	Util::TrimRight(buffer);

	if(strcmp(lastBuffer, buffer) == 0)
	{
		BOOST_CHECK(nzbFile.GetPassword().empty());
	}
	else
	{
		BOOST_CHECK_EQUAL(nzbFile.GetPassword(), std::string(buffer));
	}

	fclose(infofile);

	xmlCleanupParser();
}

BOOST_AUTO_TEST_CASE(NZBParserTest)
{
	TestNzb("dotless", "Movies");
	TestNzb("plain", "TV>4K");
	TestNzb("literal_gt", "TV > 4K");
	TestNzb("passwd{{thisisthepassword}}", "");
	TestNzb("passwdMeta", "");
	TestNzb("passwdEntity", "");
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentTest)
{
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("Clean.Release.Name"), "Clean.Release.Name");
	std::string testPath = FileSystem::SanitizePathSegment("../../etc/cron.d");
	BOOST_CHECK(testPath.find('/') == std::string::npos);
	BOOST_CHECK(testPath.find('\\') == std::string::npos);
	BOOST_CHECK(testPath.find("..") == std::string::npos);
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(".."), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("."), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(""), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("name:with*bad?chars"), "name_with_bad_chars");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("folder/sub\\name"), "folder_sub_name");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("trailing.dots...   "), "trailing.dots");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("   leading.space"), "leading.space");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("CON"), "_CON");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("NUL.txt"), "_NUL.txt");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("AUX"), "_AUX");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("COM1"), "_COM1");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("PRN.dat"), "_PRN.dat");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("LPT1"), "_LPT1");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("Vidéo.2026.mkv"), "Vidéo.2026.mkv");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("Mädchen.mp4"), "Mädchen.mp4");

	// Consecutive dots and spaces
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("..."), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(".. "), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("prefix..middle...suffix"), "prefix_middle_suffix");
	std::string dotPath1 = FileSystem::SanitizePathSegment(".../");
	BOOST_CHECK(dotPath1.find('/') == std::string::npos);
	BOOST_CHECK(dotPath1.find("..") == std::string::npos);
	std::string dotPath2 = FileSystem::SanitizePathSegment("folder/...");
	BOOST_CHECK(dotPath2.find('/') == std::string::npos);
	BOOST_CHECK(dotPath2.find("..") == std::string::npos);

	// Long dot runs and length bounding
	std::string longDots(10000, '.');
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(longDots), "");
	std::string longName(2000, 'a');
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(longName).size(), 1024u);

	// UTF-8 boundary truncation: multi-byte character straddling byte 1024
	std::string splitUtf8 = std::string(1023, 'a') + "\xC3\xA9" + "tail";
	std::string cleanSplit = FileSystem::SanitizePathSegment(splitUtf8);
	BOOST_CHECK_EQUAL(cleanSplit.size(), 1023u);
	BOOST_CHECK_EQUAL(cleanSplit.back(), 'a');

	// UTF-8 multi-byte character entirely within 1024 limit is preserved
	std::string intactUtf8 = std::string(1022, 'a') + "\xC3\xA9" + "tail";
	std::string cleanIntact = FileSystem::SanitizePathSegment(intactUtf8);
	BOOST_CHECK_EQUAL(cleanIntact.size(), 1024u);
	BOOST_CHECK_EQUAL(cleanIntact.substr(1022), "\xC3\xA9");

	// Drive and UNC paths
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("C:\\Windows"), "C__Windows");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment("\\\\server\\share"), "__server_share");
}

BOOST_AUTO_TEST_CASE(SanitizeRelativePathTest)
{
	std::string sep(1, PATH_SEPARATOR);
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("Movies"), "Movies");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV/HD"), "TV" + sep + "HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV / HD"), "TV" + sep + "HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV>HD"), "TV_HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV > HD"), "TV _ HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV\\Shows"), "TV" + sep + "Shows");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV/../../HD"), "TV" + sep + "HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV/./HD"), "TV" + sep + "HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV/.."), "TV");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("TV/../../../../../etc"), "TV" + sep + "etc");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("../../Movies/4K"), "Movies" + sep + "4K");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("/var/www/"), "var" + sep + "www");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("C:\\Movies\\SciFi"), "C_" + sep + "Movies" + sep + "SciFi");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("Séries/HD"), "Séries" + sep + "HD");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(".."), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("../.."), "");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(""), "");

	// Consecutive dots in relative paths
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("myfolder/.../file"), "myfolder" + sep + "file");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("myfolder/.. /file"), "myfolder" + sep + "file");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(".../file"), "file");

	// Absolute and UNC path normalization
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("\\\\remote-server\\share\\data"), "remote-server" + sep + "share" + sep + "data");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("//remote-server/share/data"), "remote-server" + sep + "share" + sep + "data");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("C:\\Windows\\System32"), "C_" + sep + "Windows" + sep + "System32");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("C:/Windows/System32"), "C_" + sep + "Windows" + sep + "System32");

	// Reserved device names in path components
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("LPT1/subfolder"), "_LPT1" + sep + "subfolder");
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath("movies/CON/item"), "movies" + sep + "_CON" + sep + "item");
}

namespace
{
	void WriteTempNzb(const fs::path& filePath, std::string_view headMeta)
	{
		std::ofstream out(filePath.string());
		out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		    << "<!DOCTYPE nzb PUBLIC \"-//newzBin//DTD NZB 1.0//EN\" \"http://www.newzbin.com/DTD/nzb/nzb-1.0.dtd\">\n"
		    << "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		    << "<head>\n"
		    << headMeta << "\n"
		    << "</head>\n"
		    << "<file poster=\"poster@test.com\" date=\"1335508618\" subject=\"&quot;testfile.mkv&quot; yEnc (1/1)\">\n"
		    << "<groups><group>alt.binaries.test</group></groups>\n"
		    << "<segments><segment bytes=\"100\" number=\"1\">test@msgid</segment></segments>\n"
		    << "</file>\n"
		    << "</nzb>\n";
	}

	void WriteRawNzb(const fs::path& filePath, std::string_view content)
	{
		std::ofstream out(filePath.string());
		out << content;
	}

	// Shared NZB parse harness for the encrypted-release tests below.
	// parseXml takes a full document; parseHeadMeta wraps a <head> fragment.
	std::pair<bool, std::unique_ptr<NzbInfo>> ParseXml(std::string_view xml, const std::string& filename)
	{
		const fs::path tempNzb = fs::temp_directory_path() / filename;
		WriteRawNzb(tempNzb, xml);

		NzbFile nzbFile(tempNzb.string().c_str(), "");
		bool ok = nzbFile.Parse();
		std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
		if (nzbInfo && !nzbFile.GetPassword().empty())
		{
			nzbInfo->GetParameters()->SetParameter("*Unpack:Password", nzbFile.GetPassword().c_str());
		}
		fs::remove(tempNzb);
		return { ok, std::move(nzbInfo) };
	}

	std::pair<bool, std::unique_ptr<NzbInfo>> ParseHeadMeta(const std::string& headMeta, const std::string& filename)
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head>" + headMeta + "</head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"test.bin\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
			"</file>\n"
			"</nzb>\n";
		return ParseXml(xml, filename);
	}
}

BOOST_AUTO_TEST_CASE(NzbFileMetaParsingTest)
{
	const fs::path tempNzb = fs::temp_directory_path() / "nzbget_test_meta_name.nzb";
	WriteTempNzb(tempNzb,
		"<meta type=\"name\">My.Clean.Release.Name.S01E01 &amp; Special.Edition</meta>\n"
		"<meta type=\"title\">My.Clean.Release.Name.S01E01.mkv</meta>\n"
		"<meta type=\"category\">TV / HD</meta>\n"
		"<meta type=\"password\">secret123</meta>");

	NzbFile nzbFile(tempNzb.string().c_str(), "");
	BOOST_REQUIRE(nzbFile.Parse());

	BOOST_CHECK_EQUAL(nzbFile.GetMetaName(), "My.Clean.Release.Name.S01E01 & Special.Edition");
	BOOST_CHECK_EQUAL(nzbFile.GetMetaTitle(), "My.Clean.Release.Name.S01E01.mkv");
	BOOST_CHECK_EQUAL(nzbFile.GetPassword(), "secret123");
	BOOST_CHECK_EQUAL(nzbFile.GetCategoryFromFile(), "TV / HD");

	auto nzbInfo = nzbFile.DetachNzbInfo();
	BOOST_REQUIRE(nzbInfo);
	BOOST_CHECK_EQUAL(nzbInfo->GetMetaName(), "My.Clean.Release.Name.S01E01 & Special.Edition");

	fs::remove(tempNzb);
}

BOOST_AUTO_TEST_CASE(NzbFileMetaTitleFallbackTest)
{
	const fs::path tempNzb = fs::temp_directory_path() / "nzbget_test_meta_title_fallback.nzb";
	WriteTempNzb(tempNzb, "<meta type=\"title\">Fallback.Release.Title.mkv</meta>");

	NzbFile nzbFile(tempNzb.string().c_str(), "");
	BOOST_REQUIRE(nzbFile.Parse());

	BOOST_CHECK_EQUAL(nzbFile.GetMetaName(), "Fallback.Release.Title.mkv");
	BOOST_CHECK_EQUAL(nzbFile.GetMetaTitle(), "Fallback.Release.Title.mkv");

	auto nzbInfo = nzbFile.DetachNzbInfo();
	BOOST_REQUIRE(nzbInfo);
	BOOST_CHECK_EQUAL(nzbInfo->GetMetaName(), "Fallback.Release.Title.mkv");

	fs::remove(tempNzb);
}

BOOST_AUTO_TEST_CASE(NzbFileMetaPathValidationTest)
{
	const fs::path tempNzb = fs::temp_directory_path() / "nzbget_test_meta_validation.nzb";
	WriteTempNzb(tempNzb,
		"<meta type=\"name\">../../../../test/folder/release</meta>\n"
		"<meta type=\"category\">../../../../media/downloads</meta>");

	NzbFile nzbFile(tempNzb.string().c_str(), "");
	BOOST_REQUIRE(nzbFile.Parse());

	// Slashes and .. in meta name must be normalized and cannot escape base directories
	BOOST_CHECK(nzbFile.GetMetaName().find('/') == std::string::npos);
	BOOST_CHECK(nzbFile.GetMetaName().find('\\') == std::string::npos);
	BOOST_CHECK(nzbFile.GetMetaName().find("..") == std::string::npos);

	// Relative path separators and .. must be safely normalized when generating category path
	std::string sep(1, PATH_SEPARATOR);
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(nzbFile.GetCategoryFromFile()), "media" + sep + "downloads");

	fs::remove(tempNzb);
}

BOOST_AUTO_TEST_CASE(NzbFileCategoryControlCharsTest)
{
	const fs::path tempNzb = fs::temp_directory_path() / "nzbget_test_meta_control_chars.nzb";
	WriteTempNzb(tempNzb,
		"<meta type=\"category\">\r\n\t  Movies\r\n/\t4K  \r\n</meta>");

	NzbFile nzbFile(tempNzb.string().c_str(), "");
	BOOST_REQUIRE(nzbFile.Parse());

	// Control characters must be stripped to protect line-based DiskState persistence
	const std::string& category = nzbFile.GetCategoryFromFile();
	BOOST_CHECK(category.find('\n') == std::string::npos);
	BOOST_CHECK(category.find('\r') == std::string::npos);
	BOOST_CHECK(category.find('\t') == std::string::npos);

	std::string sep(1, PATH_SEPARATOR);
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(category), "Movies" + sep + "4K");

	fs::remove(tempNzb);
}

BOOST_AUTO_TEST_CASE(EmptyNzbNameFallbackTest)
{
	// MakeNiceNzbName must never return empty string
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("   .nzb", true), "nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName(".nzb", true), "nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("...", true), "nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("   ", true), "nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("", true), "nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("   .nzb", false), ".nzb");

	// Valid names must not be altered
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("My.Release.nzb", true), "My.Release");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("My.Release.nzb", false), "My.Release.nzb");
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("My.Release", false), "My.Release");

	// Consecutive dots normalized
	BOOST_CHECK_EQUAL(NzbInfo::MakeNiceNzbName("My..Release...nzb", true), "My_Release");
}

BOOST_AUTO_TEST_CASE(BuildFinalDirNameUniqueIdTest)
{
	NzbInfo nzbInfo1;
	nzbInfo1.SetName("");
	std::string finalDir1 = *nzbInfo1.BuildFinalDirName();
	std::string expectedSuffix1 = "nzb-" + std::to_string(nzbInfo1.GetId());
	BOOST_CHECK(finalDir1.rfind(expectedSuffix1) != std::string::npos);

	NzbInfo nzbInfo2;
	nzbInfo2.SetName("");
	std::string finalDir2 = *nzbInfo2.BuildFinalDirName();
	std::string expectedSuffix2 = "nzb-" + std::to_string(nzbInfo2.GetId());
	BOOST_CHECK(finalDir2.rfind(expectedSuffix2) != std::string::npos);

	// Both empty downloads must have different final directories
	BOOST_CHECK_NE(finalDir1, finalDir2);
}

BOOST_AUTO_TEST_CASE(YEncEncryptedMetaParsingTest)
{
	auto parseXml = ParseXml;

	// 1. Valid NZB 1.1 encrypted release
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head><meta type=\"password\">secret</meta><meta type=\"yenc_encrypted\">true</meta></head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"[1/1] - &quot;f.bin&quot; yEnc (1/2)\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg1@test</segment>\n"
			"<segment bytes=\"100\" number=\"2\">msg2@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "valid_nzb11.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
		auto articles = info->GetFileList()->front()->GetArticles();
		BOOST_REQUIRE_EQUAL(articles->size(), 2U);
	}

	// 2. Arbitrary non-numbered subjects with yenc_encrypted metadata
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head><meta type=\"password\">secret</meta><meta type=\"yenc_encrypted\">true</meta></head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"&quot;random_movie.mkv&quot;\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg1@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"<file poster=\"p\" date=\"100\" subject=\"&quot;random_movie.par2&quot;\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg2@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "arbitrary_sub.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// 3. Obfuscated hexadecimal subjects with yenc_encrypted metadata
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head><meta type=\"password\">secret</meta><meta type=\"yenc_encrypted\">true</meta></head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"7a8b9c0d1e2f3a4b\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg1@test</segment>\n"
			"<segment bytes=\"100\" number=\"2\">msg2@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "obfuscated_sub.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// 4. Archive-password-only release (password without yenc_encrypted meta)
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head><meta type=\"password\">archive_secret</meta></head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"archive.bin\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg1@test</segment>\n"
			"<segment bytes=\"100\" number=\"2\">msg2@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "archive_pwd.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
		BOOST_CHECK_EQUAL(info->GetPassword(), "archive_secret");
	}

	// 5. Unencrypted release without password
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<file poster=\"p\" date=\"100\" subject=\"unencrypted.bin\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments>\n"
			"<segment bytes=\"100\" number=\"1\">msg1@test</segment>\n"
			"<segment bytes=\"100\" number=\"2\">msg2@test</segment>\n"
			"</segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "unencrypted.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}

	// 6. yenc_encrypted accepts only "true" (case-insensitive); other truthy
	// spellings do not turn on encrypted handling
	for (const char* val : {"TRUE", "True", "yes", "1", "false", "on"})
	{
		const std::string xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
			"<head><meta type=\"password\">secret</meta><meta type=\"yenc_encrypted\">" + std::string(val) + "</meta></head>\n"
			"<file poster=\"p\" date=\"100\" subject=\"gate.bin\">\n"
			"<groups><group>a.b.t</group></groups>\n"
			"<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
			"</file>\n"
			"</nzb>\n";
		auto [ok, info] = parseXml(xml, "meta_gate.nzb");
		BOOST_REQUIRE(ok);
		BOOST_REQUIRE(info);
		if (!strcasecmp(val, "true"))
		{
			BOOST_CHECK(info->IsYEncEncrypted());
		}
		else
		{
			BOOST_CHECK(!info->IsYEncEncrypted());
		}
	}
}

// GAP13-03: the second canonical NZB namespace (.../nzb/nzb-1.1.dtd) parses
// exactly like the 2003/nzb namespace: both parsers are namespace-agnostic.
BOOST_AUTO_TEST_CASE(YEncEncryptedSecondNamespaceTest)
{
	const std::string xml =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<nzb xmlns=\"http://www.newzbin.com/DTD/nzb/nzb-1.1.dtd\">\n"
		"<head><meta type=\"password\">secret</meta><meta type=\"yenc_encrypted\">true</meta></head>\n"
		"<file poster=\"p\" date=\"100\" subject=\"test.bin\">\n"
		"<groups><group>a.b.t</group></groups>\n"
		"<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
		"</file>\n"
		"</nzb>\n";
	auto [ok, info] = ParseXml(xml, "enc_second_namespace.nzb");
	BOOST_CHECK(ok);
	BOOST_REQUIRE(info);
	BOOST_CHECK(info->IsYEncEncrypted());
	BOOST_CHECK_EQUAL(std::string(info->GetPassword()), "secret");
	BOOST_REQUIRE_EQUAL(info->GetFileCount(), 1);
	auto articles = info->GetFileList()->front()->GetArticles();
	BOOST_REQUIRE_EQUAL(articles->size(), 1U);
}

BOOST_AUTO_TEST_CASE(EncryptedReleaseWithoutPasswordStructuralAbortTest)
{
	// Structural tier: yenc_encrypted release without password meta and
	// without a filename token must fail with METADATA_VALIDATION — but at
	// the effective-password gate AFTER the caller-parameter merge
	// (Scanner::AddFileToQueue / ProcessStandalone), because an
	// API-supplied *Unpack:Password can still satisfy the requirement.
	// Parse() itself accepts the release; this test pins the parse-level
	// contract: passwordless metadata still yields a parsable NzbInfo that
	// reports no effective password.
	auto parseXml = ParseXml;

	const std::string xml =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		"<head><meta type=\"yenc_encrypted\">true</meta></head>\n"
		"<file poster=\"p\" date=\"100\" subject=\"nopass.bin\">\n"
		"<groups><group>a.b.t</group></groups>\n"
		"<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
		"</file>\n"
		"</nzb>\n";
	auto [ok, info] = parseXml(xml, "enc_nopwd.nzb");
	BOOST_CHECK(ok);
	BOOST_REQUIRE(info);
	BOOST_CHECK(info->IsYEncEncrypted());
	// No effective password at parse level: the Scanner/standalone gate
	// rejects this release (see ScannerEffectivePasswordGateTest).
	BOOST_CHECK_EQUAL(info->GetPassword(), "");
}

BOOST_AUTO_TEST_CASE(EncryptedReleaseWithPasswordInFilenameAcceptedTest)
{
	// The filename {{...}} token is a supported app-native password source.
	// An encrypted release whose ONLY password is the filename token must parse
	// successfully at ingest; the resolved password is exposed via GetPassword()
	// so Scanner/process-standalone can publish *Unpack:Password.
	const fs::path tempNzb = fs::temp_directory_path() / "Release {{filename_secret}}.nzb";
	const std::string xml =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		"<head><meta type=\"yenc_encrypted\">true</meta></head>\n"
		"<file poster=\"p\" date=\"100\" subject=\"test.bin\">\n"
		"<groups><group>a.b.t</group></groups>\n"
		"<segments><segment bytes=\"100\" number=\"1\">msg1@test</segment></segments>\n"
		"</file>\n"
		"</nzb>\n";
	WriteRawNzb(tempNzb, xml);
	NzbFile nzbFile(tempNzb.string().c_str(), "");
	bool ok = nzbFile.Parse();
	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	fs::remove(tempNzb);

	BOOST_CHECK(ok);
	BOOST_REQUIRE(nzbInfo);
	BOOST_CHECK(nzbInfo->IsYEncEncrypted());
	BOOST_CHECK_EQUAL(nzbFile.GetPassword(), "filename_secret");
}

BOOST_AUTO_TEST_CASE(EncryptedReleaseUnsupportedVersionTest)
{
	// Unsupported yenc_version on an encrypted release is a structural
	// METADATA_VALIDATION failure at queue time — independent of meta ordering.
	auto parseXml = ParseHeadMeta;

	auto hasValidation = [](const std::unique_ptr<NzbInfo>& info) -> bool
	{
		bool found = false;
		for (Message& message : info->GuardCachedMessages())
		{
			if (message.GetKind() == Message::mkError &&
				strstr(message.GetText(), "METADATA_VALIDATION"))
			{
				found = true;
			}
		}
		return found;
	};

	// Unsupported version (before the encrypted marker)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">1.0</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_bad_version_before.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Unsupported version (after the encrypted marker)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_encrypted\">true</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_version\">2.0</meta>",
			"enc_bad_version_after.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Supported version parses fine
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_good_version.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Unsupported cipher
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_bad_cipher.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Supported cipher parses fine
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_good_cipher.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Empty version tag on encrypted release is rejected (Body Std v1.2 §8)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\"></meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_empty_version.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Self-closing empty version tag
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\"/>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_empty_version_selfclose.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Whitespace-only version tag
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">   </meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_whitespace_version.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Empty cipher tag on encrypted release is rejected (Body Std v1.2 §8)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\"></meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_empty_cipher.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Self-closing empty cipher tag
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\"/>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_empty_cipher_selfclose.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Whitespace-only cipher tag
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">   </meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_whitespace_cipher.nzb");
		BOOST_CHECK(!ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(hasValidation(info));
	}

	// Absent version and cipher meta tags parse fine (absent is allowed)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_absent_version_cipher.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Correct version and cipher together parse fine
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"enc_good_version_and_cipher.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Version/cipher metas on a NON-encrypted release are ignored
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">9.9</meta>"
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta>",
			"enc_metas_unencrypted.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}
}

BOOST_AUTO_TEST_CASE(EncryptedReleaseDuplicateOptionalMetadataTest)
{
	auto parseXml = ParseHeadMeta;

	auto getValidationMessage = [](const std::unique_ptr<NzbInfo>& info) -> std::string
	{
		if (!info) return "";
		for (Message& message : info->GuardCachedMessages())
		{
			if (message.GetKind() == Message::mkError &&
				strstr(message.GetText(), "METADATA_VALIDATION"))
			{
				return message.GetText();
			}
		}
		return "";
	};

	// One fail-closed rule: any present invalid optional meta (yenc_version /
	// yenc_cipher), including duplicates, rejects an encrypted release.
	// Per field: one bad-then-valid, one valid-then-bad, one invalid-only,
	// plus controls. Marker position (before/after yenc_encrypted) is
	// order-independent, so a single representative ordering per shape is
	// enough; the single-value empty/self-closing/whitespace shapes are
	// already covered by EncryptedReleaseUnsupportedVersionTest.
	const std::string marker = "<meta type=\"yenc_encrypted\">true</meta>";
	const std::string password = "<meta type=\"password\">secret</meta>";

	struct RejectCase
	{
		const char* label;
		std::string headMeta;
		const char* expectedFragment;
	};
	const RejectCase rejectCases[] = {
		// yenc_version: bad-then-valid
		{"ver bad-then-valid", marker + password +
			"<meta type=\"yenc_version\">1.0</meta><meta type=\"yenc_version\">1.2</meta>",
			"unsupported yenc_version: 1.0"},
		// yenc_version: valid-then-bad
		{"ver valid-then-bad", marker + password +
			"<meta type=\"yenc_version\">1.2</meta><meta type=\"yenc_version\">1.0</meta>",
			"unsupported yenc_version: 1.0"},
		// yenc_version: invalid-only (first offending value reported)
		{"ver invalid-only", marker + password +
			"<meta type=\"yenc_version\">1.0</meta><meta type=\"yenc_version\">2.0</meta>",
			"unsupported yenc_version: 1.0"},
		// yenc_cipher: bad-then-valid
		{"cipher bad-then-valid", marker + password +
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta><meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>",
			"unsupported yenc_cipher: AES-256-GCM"},
		// yenc_cipher: valid-then-bad
		{"cipher valid-then-bad", marker + password +
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta><meta type=\"yenc_cipher\">AES-256-GCM</meta>",
			"unsupported yenc_cipher: AES-256-GCM"},
		// yenc_cipher: invalid-only (first offending value reported)
		{"cipher invalid-only", marker + password +
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta><meta type=\"yenc_cipher\">ChaCha20</meta>",
			"unsupported yenc_cipher: AES-256-GCM"},
	};
	int rejectIndex = 0;
	for (const auto& rejectCase : rejectCases)
	{
		BOOST_TEST_CONTEXT(rejectCase.label)
		{
			auto [ok, info] = parseXml(rejectCase.headMeta,
				"dup_reject_" + std::to_string(rejectIndex++) + ".nzb");
			BOOST_CHECK(!ok);
			BOOST_REQUIRE(info);
			std::string msg = getValidationMessage(info);
			BOOST_CHECK(msg.find(rejectCase.expectedFragment) != std::string::npos);
		}
	}

	// --- Controls ---
	// Absent optional metadata: accepted
	{
		auto [ok, info] = parseXml(
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"dup_control_absent_optional.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Valid-only duplicate version metadata: accepted
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"dup_control_valid_duplicate_version.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Valid-only duplicate cipher metadata: accepted
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"dup_control_valid_duplicate_cipher.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Valid-only duplicate version AND cipher metadata: accepted
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"yenc_version\">1.2</meta>"
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>"
			"<meta type=\"password\">secret</meta>"
			"<meta type=\"yenc_encrypted\">true</meta>",
			"dup_control_valid_duplicate_both.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(info->IsYEncEncrypted());
	}

	// Unencrypted release with bad-then-valid version: unchanged (accepted)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">9.9</meta>"
			"<meta type=\"yenc_version\">1.2</meta>",
			"dup_control_unencrypted_version_bad_then_valid.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}

	// Unencrypted release with invalid-only version: unchanged (accepted)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_version\">9.9</meta>"
			"<meta type=\"yenc_version\">8.8</meta>",
			"dup_control_unencrypted_version_invalid_only.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}

	// Unencrypted release with bad-then-valid cipher: unchanged (accepted)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta>"
			"<meta type=\"yenc_cipher\">XChaCha20-Poly1305</meta>",
			"dup_control_unencrypted_cipher_bad_then_valid.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}

	// Unencrypted release with invalid-only cipher: unchanged (accepted)
	{
		auto [ok, info] = parseXml(
			"<meta type=\"yenc_cipher\">AES-256-GCM</meta>"
			"<meta type=\"yenc_cipher\">Blowfish</meta>",
			"dup_control_unencrypted_cipher_invalid_only.nzb");
		BOOST_CHECK(ok);
		BOOST_REQUIRE(info);
		BOOST_CHECK(!info->IsYEncEncrypted());
	}
}

BOOST_AUTO_TEST_SUITE_END()
