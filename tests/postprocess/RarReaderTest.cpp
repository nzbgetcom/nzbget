/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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

#include "RarReader.h"
#include "FileSystem.h"

static std::string currDir = FileSystem::GetCurrentDirectory().Str();
static std::string testDataDir = currDir + "/" + "rarrenamer";

BOOST_AUTO_TEST_CASE(Rar3Test)
{
	{
		RarVolume volume((testDataDir + "/testfile3.part01.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((testDataDir + "/testfile3.part02.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((testDataDir + "/testfile3.part03.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar5Test)
{
	{
		RarVolume volume((testDataDir + "/testfile5.part01.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((testDataDir + "/testfile5.part02.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((testDataDir + "/testfile5.part03.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar3OldNamingTest)
{
	{
		RarVolume volume((testDataDir + "/testfile3oldnam.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == false);
		BOOST_CHECK(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((testDataDir + "/testfile3oldnam.r00").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == false);
		BOOST_CHECK(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((testDataDir + "/testfile3oldnam.r01").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == false);
		BOOST_CHECK(volume.GetVolumeNo() == 2);
	}
}

#ifndef DISABLE_TLS

BOOST_AUTO_TEST_CASE(Rar3EncryptedDataTest)
{
	{
		RarVolume volume((testDataDir + "/testfile3encdata.part01.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((testDataDir + "/testfile3encdata.part02.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((testDataDir + "/testfile3encdata.part03.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar5EncryptedDataTest)
{
	{
		RarVolume volume((testDataDir + "/testfile5encdata.part01.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 0);
	}
	{
		RarVolume volume((testDataDir + "/testfile5encdata.part02.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 1);
	}
	{
		RarVolume volume((testDataDir + "/testfile5encdata.part03.rar").c_str());
		BOOST_CHECK(volume.Read() == true);
		BOOST_CHECK(volume.GetVersion() == 5);
		BOOST_CHECK(volume.GetMultiVolume() == true);
		BOOST_CHECK(volume.GetNewNaming() == true);
		BOOST_CHECK(volume.GetVolumeNo() == 2);
	}
}

BOOST_AUTO_TEST_CASE(Rar3EcryptedNamesTest)
{
	{
		RarVolume volume((testDataDir + "/testfile3encnam.part01.rar").c_str());
		BOOST_CHECK(volume.Read() == false);
		BOOST_CHECK(volume.GetVersion() == 3);
		BOOST_CHECK(volume.GetEncrypted() == true);
	}

	// {
	// 	RarVolume volume((testDataDir + "/testfile3encnam.part01.rar").c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK(volume.Read() == true);
	// 	BOOST_CHECK(volume.GetVersion() == 3);
	// 	BOOST_CHECK(volume.GetMultiVolume() == true);
	// 	BOOST_CHECK(volume.GetNewNaming() == true);
	// 	BOOST_CHECK(volume.GetVolumeNo() == 0);
	// }
	// {
	// 	RarVolume volume((testDataDir + "/testfile3encnam.part02.rar").c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK(volume.Read() == true);
	// 	BOOST_CHECK(volume.GetVersion() == 3);
	// 	BOOST_CHECK(volume.GetMultiVolume() == true);
	// 	BOOST_CHECK(volume.GetNewNaming() == true);
	// 	BOOST_CHECK(volume.GetVolumeNo() == 1);
	// }
	// {
	// 	RarVolume volume((testDataDir + "/testfile3encnam.part03.rar").c_str());
	// 	volume.SetPassword("123");
	// 	BOOST_CHECK(volume.Read() == true);
	// 	BOOST_CHECK(volume.GetVersion() == 3);
	// 	BOOST_CHECK(volume.GetMultiVolume() == true);
	// 	BOOST_CHECK(volume.GetNewNaming() == true);
	// 	BOOST_CHECK(volume.GetVolumeNo() == 2);
	// }
}

// BOOST_AUTO_TEST_CASE(Rar5EncryptedNames)
// {
// 	{
// 		RarVolume volume((testDataDir + "/testfile5encnam.part01.rar").c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK(volume.Read() == true);
// 		BOOST_CHECK(volume.GetVersion() == 5);
// 		BOOST_CHECK(volume.GetMultiVolume() == true);
// 		BOOST_CHECK(volume.GetNewNaming() == true);
// 		BOOST_CHECK(volume.GetVolumeNo() == 0);
// 	}
// 	{
// 		RarVolume volume((testDataDir + "/testfile5encnam.part02.rar").c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK(volume.Read() == true);
// 		BOOST_CHECK(volume.GetVersion() == 5);
// 		BOOST_CHECK(volume.GetMultiVolume() == true);
// 		BOOST_CHECK(volume.GetNewNaming() == true);
// 		BOOST_CHECK(volume.GetVolumeNo() == 1);
// 	}
// 	{
// 		RarVolume volume((testDataDir + "/testfile5encnam.part03.rar").c_str());
// 		volume.SetPassword("123");
// 		BOOST_CHECK(volume.Read() == true);
// 		BOOST_CHECK(volume.GetVersion() == 5);
// 		BOOST_CHECK(volume.GetMultiVolume() == true);
// 		BOOST_CHECK(volume.GetNewNaming() == true);
// 		BOOST_CHECK(volume.GetVolumeNo() == 2);
// 	}

// 	{
// 		RarVolume volume((testDataDir + "/testfile5encnam.part01.rar").c_str());
// 		BOOST_CHECK(volume.Read() == false);
// 		BOOST_CHECK(volume.GetVersion() == 5);
// 		BOOST_CHECK(volume.GetEncrypted() == true);
// 	}
// }

#endif
