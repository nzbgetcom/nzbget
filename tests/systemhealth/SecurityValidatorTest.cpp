/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 */

#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include "Options.h"
#include "SecurityValidator.h"
#include "Validators.h"

using namespace SystemHealth;
using namespace SystemHealth::Security;

static Options MakeOptions(const Options::CmdOptList& list)
{
    return Options(&list, nullptr);
}

BOOST_AUTO_TEST_SUITE(SecurityValidatorTests)

BOOST_AUTO_TEST_CASE(TestControlIpInfo)
{
    Options::CmdOptList opts;
    opts.push_back("ControlIp=0.0.0.0");
    Options options(&opts, nullptr);

    ControlIpValidator v(options);
    auto s = v.Validate();
    BOOST_CHECK(s.IsInfo());
}

BOOST_AUTO_TEST_CASE(TestControlPortWarning)
{
    Options::CmdOptList opts;
    opts.push_back("ControlPort=22");
    Options options(&opts, nullptr);

    ControlPortValidator v(options);
    auto s = v.Validate();
    BOOST_CHECK(s.IsWarning());
}

BOOST_AUTO_TEST_CASE(TestSecureControlMissingPort)
{
    Options::CmdOptList opts;
    opts.push_back("SecureControl=yes");
    opts.push_back("SecurePort=0");
    Options options(&opts, nullptr);

    SecureControlValidator v(options);
    auto s = v.Validate();
    BOOST_CHECK(s.IsError());
}

BOOST_AUTO_TEST_CASE(TestFormAuthWarningWhenNoSecure)
{
    Options::CmdOptList opts;
    opts.push_back("FormAuth=yes");
    opts.push_back("SecureControl=no");
    Options options(&opts, nullptr);

    FormAuthValidator v(options);
    auto s = v.Validate();
    BOOST_CHECK(s.IsWarning());
}

BOOST_AUTO_TEST_SUITE_END()
