/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

#include <fstream>
#include <vector>
#include <cstring>
#include <boost/test/unit_test.hpp>
#include "RarReader.h"
#include "FileSystem.h"
#include "ContentMap.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

const fs::path CURR_DIR = fs::current_path();
const fs::path TEST_DATA_DIR = CURR_DIR / "rarrenamer";

BOOST_AUTO_TEST_CASE(Rar3Test)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile3.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3.part02.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3.part03.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar5Test)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile5.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5.part02.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5.part03.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar3OldNamingTest)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile3oldnam.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), false);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3oldnam.r00";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), false);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3oldnam.r01";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), false);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	}
}

#ifndef DISABLE_TLS

BOOST_AUTO_TEST_CASE(Rar3EncryptedDataTest)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile3encdata.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3encdata.part02.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile3encdata.part03.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar5EncryptedDataTest)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile5encdata.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5encdata.part02.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5encdata.part03.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), true);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar3EcryptedNamesTest)
{
	{
		const fs::path file = TEST_DATA_DIR / "testfile3encnam.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_CHECK_EQUAL(volume.Read(), false);
		BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
		BOOST_CHECK_EQUAL(volume.GetEncrypted(), true);
	}

	// {
	// 	RarVolume volume((testDataDir + PATH_SEPARATOR + "testfile3encnam.part01.rar").string().c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK_EQUAL(volume.Read(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
	// 	BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
	// }
	// {
	// 	RarVolume volume((testDataDir + PATH_SEPARATOR + "testfile3encnam.part02.rar").string().c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK_EQUAL(volume.Read(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
	// 	BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
	// }
	// {
	// 	RarVolume volume((testDataDir + "/testfile3encnam.part03.rar").string().c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK_EQUAL(volume.Read(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
	// 	BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
	// 	BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
	// }
}

// BOOST_AUTO_TEST_CASE(Rar5EncryptedNames)
// {
// 	{
// 		RarVolume volume((testDataDir + PATH_SEPARATOR + "testfile5encnam.part01.rar").string().c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK_EQUAL(volume.Read(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
// 		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
// 		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 0);
// 	}
// 	{
// 		RarVolume volume((testDataDir + PATH_SEPARATOR + "testfile5encnam.part02.rar").string().c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK_EQUAL(volume.Read(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
// 		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
// 		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 1);
// 	}
// 	{
// 		RarVolume volume((testDataDir + "/testfile5encnam.part03.rar").string().c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK_EQUAL(volume.Read(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
// 		BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
// 		BOOST_CHECK_EQUAL(volume.GetNewNaming(), true);
// 		BOOST_CHECK_EQUAL(volume.GetVolumeNo(), 2);
// 	}

// 	{
// 		RarVolume volume((testDataDir + PATH_SEPARATOR + "testfile5encnam.part01.rar").string().c_str());
// 		BOOST_CHECK_EQUAL(volume.Read(), false);
// 		BOOST_CHECK_EQUAL(volume.GetVersion(), 5);
// 		BOOST_CHECK_EQUAL(volume.GetEncrypted(), true);
// 	}
// }

#endif

namespace
{

class MemoryRarSource : public ContentSource
{
public:
	MemoryRarSource(std::vector<char> data) : m_data(std::move(data)) {}
	virtual int64 Size() { return (int64)m_data.size(); }
	virtual bool Read(int64 offset, void* buffer, int64 size)
	{
		if (offset < 0 || size < 0 || offset + size > (int64)m_data.size())
		{
			return false;
		}
		memcpy(buffer, m_data.data() + offset, size);
		return true;
	}
private:
	std::vector<char> m_data;
};

std::vector<char> SlurpFile(const fs::path& path)
{
	std::ifstream in(path.string(), std::ios::binary);
	return std::vector<char>((std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
}

}

BOOST_AUTO_TEST_CASE(RarStoreFieldsTest)
{
	// per-file method/packed-size/data-offset are exposed now; the testdata
	// volumes carry real WinRAR output, so assert structural invariants
	{
		const fs::path file = TEST_DATA_DIR / "testfile3.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_REQUIRE(volume.Read());
		BOOST_REQUIRE(!volume.GetFiles()->empty());
		RarFile& inner = volume.GetFiles()->front();
		int64 volumeSize = (int64)fs::file_size(file);
		BOOST_CHECK(inner.GetPackedSize() > 0);
		BOOST_CHECK(inner.GetDataOffset() > 0);
		BOOST_CHECK(inner.GetDataOffset() + inner.GetPackedSize() <= volumeSize);
		BOOST_CHECK(inner.GetMethod() != 0);	// rar3 methods are 0x30..0x35
		BOOST_CHECK(!inner.GetEncryptedData());
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_REQUIRE(volume.Read());
		BOOST_REQUIRE(!volume.GetFiles()->empty());
		RarFile& inner = volume.GetFiles()->front();
		int64 volumeSize = (int64)fs::file_size(file);
		BOOST_CHECK(inner.GetPackedSize() > 0);
		BOOST_CHECK(inner.GetDataOffset() > 0);
		BOOST_CHECK(inner.GetDataOffset() + inner.GetPackedSize() <= volumeSize);
		BOOST_CHECK(!inner.GetEncryptedData());
	}
	// -p volumes: data encrypted, headers plain - the flag must show
	{
		const fs::path file = TEST_DATA_DIR / "testfile3encdata.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_REQUIRE(volume.Read());
		BOOST_REQUIRE(!volume.GetFiles()->empty());
		BOOST_CHECK(volume.GetFiles()->front().GetEncryptedData());
	}
	{
		const fs::path file = TEST_DATA_DIR / "testfile5encdata.part01.rar";
		RarVolume volume(file.string().c_str());
		BOOST_REQUIRE(volume.Read());
		BOOST_REQUIRE(!volume.GetFiles()->empty());
		BOOST_CHECK(volume.GetFiles()->front().GetEncryptedData());
	}
}

BOOST_AUTO_TEST_CASE(RarReadFromMemorySourceTest)
{
	// the same volume parsed through an in-memory ContentSource must give
	// the same result as the disk path (donor-side symmetry)
	MemoryRarSource source(SlurpFile(TEST_DATA_DIR / "testfile3.part01.rar"));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	BOOST_CHECK_EQUAL(volume.GetVersion(), 3);
	BOOST_CHECK_EQUAL(volume.GetMultiVolume(), true);
	BOOST_REQUIRE(!volume.GetFiles()->empty());
	BOOST_CHECK(volume.GetFiles()->front().GetPackedSize() > 0);

	MemoryRarSource source5(SlurpFile(TEST_DATA_DIR / "testfile5.part01.rar"));
	RarVolume volume5("memory");
	BOOST_REQUIRE(volume5.ReadFrom(source5));
	BOOST_CHECK_EQUAL(volume5.GetVersion(), 5);
}

BOOST_AUTO_TEST_SUITE_END()
