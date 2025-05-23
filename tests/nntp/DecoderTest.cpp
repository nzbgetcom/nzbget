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
#include "Decoder.h"
#include "YEncoder.h"

std::string yEncEncode(const std::string& data)
{
    std::string encodedData = "";
    for (unsigned char c : data)
    {
        encodedData += static_cast<unsigned char>((c + 42) % 256);
    }
    return encodedData;
}

/**
 * Single message:
 * 
 * =ybegin line=128 size=111401 name=al_larsonbw030_ball.jpg\r\n
 * )_)=J*:tpsp*++++V+V**)_*m*0./0/.00/011024:44334>896:A>....\r\n
 * ...Â´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÍ©)_\r\n
 * =yend size=111401\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(SingleMessageTest)
{
    Decoder decoder;
    decoder.SetCrcCheck(true);
    std::stringstream ss;

    const std::string data = "nzbget";
    const std::string crc = "a30bff0d";
    const std::string filename = "name.dat";
    const std::string encodedData = yEncEncode(data);

    ss << "=ybegin line=128 size=" << data.size() << " name=" << filename << "\r\n";
    ss << encodedData << "\r\n";
    ss << "=yend size=" << data.size() << " crc32=" << crc << "\r\n";
    ss << ".\r\n";

    std::string msg = ss.str();

    int len = decoder.DecodeBuffer(msg.data(), msg.size());

    auto status = decoder.Check();
    auto size = decoder.GetSize();
    auto calculatedCrc = decoder.GetCalculatedCrc();
    auto articleName = decoder.GetArticleFilename();
    auto format = decoder.GetFormat();
    auto eof = decoder.GetEof();
    auto res = msg.substr(0, len);

    BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
    BOOST_CHECK_EQUAL(articleName, filename);
    BOOST_CHECK_EQUAL(size, 6);
    BOOST_CHECK_EQUAL(calculatedCrc, 0xa30bff0d);
    BOOST_CHECK_EQUAL(format, Decoder::efYenc);
    BOOST_CHECK_EQUAL(eof, true);
    BOOST_CHECK_EQUAL(res, "nzbget");
}

/**
 * Multipart message:
 * 
 * =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=1 end=100000\r\n
 * .... data\r\n
 * =yend size=100000 part=1 pcrc32=abcdef12\r\n
 * .\r\n
 * =ybegin part=5 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=400001 end=500000\r\n
 * .... data\r\n
 * =yend size=100000 part=10 pcrc32=12a45c78 crc32=abcdef12\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(MultipartMessageTest)
{
    Decoder decoder;
    decoder.SetCrcCheck(true);
    std::stringstream ss;

    const std::string crc = "a30bff0d";
    const std::string data = "nzb";
    const std::string encodedData = yEncEncode(data);
    const std::string pcrc = "cb64ae30";
    const std::string filename = "name.dat";

    const std::string data2 = "get";
    const std::string encodedData2 = yEncEncode(data2);
    const std::string pcrc2 = "fd3b2e70";
    const size_t totalSize = data.size() + data2.size();

    ss << "=ybegin part=1 total=2 line=128 size=" << totalSize << " name=" << filename << "\r\n";
    ss << "=ypart begin=1 end=" << data.size() << "\r\n";
    ss << encodedData << "\r\n";
    ss << "=yend size=" << data.size() << " pcrc32=" << pcrc << "\r\n";
    ss << ".\r\n";

    std::string msg = ss.str();
    ss.str("");

    ss << "=ybegin part=2 total=2 line=128 size=" << totalSize << " name=" << filename << "\r\n";
    ss << "=ypart begin=" << data.size() + 1 << " end=" << totalSize << "\r\n";
    ss << encodedData2 << "\r\n";
    ss << "=yend size=" << data2.size() << " pcrc32=" << pcrc2 << " crc32=" << crc << "\r\n";
    ss << ".\r\n";

    std::string msg2 = ss.str();

    int len = decoder.DecodeBuffer(msg.data(), msg.size());
    {
        auto status = decoder.Check();
        auto size = decoder.GetSize();
        auto calculatedCrc = decoder.GetCalculatedCrc();
        auto articleName = decoder.GetArticleFilename();
        auto format = decoder.GetFormat();
        auto eof = decoder.GetEof();
        auto res = msg.substr(0, len);

        BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
        BOOST_CHECK_EQUAL(articleName, filename);
        BOOST_CHECK_EQUAL(size, totalSize);
        BOOST_CHECK_EQUAL(calculatedCrc, 0xcb64ae30);
        BOOST_CHECK_EQUAL(format, Decoder::efYenc);
        BOOST_CHECK_EQUAL(eof, true);
        BOOST_CHECK_EQUAL(res, "nzb");
    }

    decoder.Clear();
    decoder.SetCrcCheck(true);

    int len2 = decoder.DecodeBuffer(msg2.data(), msg2.size());

    auto articleName = decoder.GetArticleFilename();
    auto status = decoder.Check();
    auto size = decoder.GetSize();
    auto calculatedCrc = decoder.GetCalculatedCrc();
    auto eof = decoder.GetEof();
    auto res = msg2.substr(0, len);

    BOOST_CHECK_EQUAL(articleName, filename);
    BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
    BOOST_CHECK_EQUAL(len, data2.size());
    BOOST_CHECK_EQUAL(len + len2, totalSize);
    BOOST_CHECK_EQUAL(size, totalSize);
    BOOST_CHECK_EQUAL(calculatedCrc, 0xfd3b2e70);
    BOOST_CHECK_EQUAL(eof, true);
    BOOST_CHECK_EQUAL(res, "get");
}

/**
 * Unusual multipart message where there's no \r\n separating between name=... and =ypart:
 * 
 * =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat=ypart begin=1 end=100000\r\n
 * .... data\r\n
 * =yend size=100000 part=1 pcrc32=abcdef12\r\n
 * .\r\n
 * =ybegin part=5 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=400001 end=500000\r\n
 * .... data\r\n
 * =yend size=100000 part=10 pcrc32=12a45c78 crc32=abcdef12\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(UnusualMultipartMessageTest)
{
    Decoder decoder;
    decoder.SetCrcCheck(true);
    std::stringstream ss;

    const std::string crc = "a30bff0d";
    const std::string data = "nzb";
    const std::string encodedData = yEncEncode(data);
    const std::string pcrc = "cb64ae30";
    const std::string filename = "name.dat";

    const std::string data2 = "get";
    const std::string encodedData2 = yEncEncode(data2);
    const std::string pcrc2 = "fd3b2e70";
    const size_t totalSize = data.size() + data2.size();

    ss << "=ybegin part=1 total=2 line=128 size=" << totalSize << " name=" << filename;
    ss << "=ypart begin=1 end=" << data.size() << "\r\n";
    ss << encodedData << "\r\n";
    ss << "=yend size=" << data.size() << " pcrc32=" << pcrc << "\r\n";
    ss << ".\r\n";

    std::string msg = ss.str();
    ss.str("");

    ss << "=ybegin part=2 total=2 line=128 size=" << totalSize << " name=" << filename;
    ss << "=ypart begin=" << data.size() + 1 << " end=" << totalSize << "\r\n";
    ss << encodedData2 << "\r\n";
    ss << "=yend size=" << data2.size() << " pcrc32=" << pcrc2 << " crc32=" << crc << "\r\n";
    ss << ".\r\n";

    std::string msg2 = ss.str();

    int len = decoder.DecodeBuffer(msg.data(), msg.size());
    {
        auto status = decoder.Check();
        auto size = decoder.GetSize();
        auto calculatedCrc = decoder.GetCalculatedCrc();
        auto articleName = decoder.GetArticleFilename();
        auto format = decoder.GetFormat();
        auto eof = decoder.GetEof();
        auto res = msg.substr(0, len);

        BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
        BOOST_CHECK_EQUAL(articleName, filename);
        BOOST_CHECK_EQUAL(size, totalSize);
        BOOST_CHECK_EQUAL(calculatedCrc, 0xcb64ae30);
        BOOST_CHECK_EQUAL(format, Decoder::efYenc);
        BOOST_CHECK_EQUAL(eof, true);
        BOOST_CHECK_EQUAL(res, "nzb");
    }

    decoder.Clear();
    decoder.SetCrcCheck(true);

    int len2 = decoder.DecodeBuffer(msg2.data(), msg2.size());

    auto articleName = decoder.GetArticleFilename();
    auto status = decoder.Check();
    auto size = decoder.GetSize();
    auto calculatedCrc = decoder.GetCalculatedCrc();
    auto eof = decoder.GetEof();
    auto res = msg2.substr(0, len);

    BOOST_CHECK_EQUAL(articleName, filename);
    BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
    BOOST_CHECK_EQUAL(len, data2.size());
    BOOST_CHECK_EQUAL(len + len2, totalSize);
    BOOST_CHECK_EQUAL(size, totalSize);
    BOOST_CHECK_EQUAL(calculatedCrc, 0xfd3b2e70);
    BOOST_CHECK_EQUAL(eof, true);
    BOOST_CHECK_EQUAL(res, "get");
}
