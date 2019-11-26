/**
 * Copyright 2019 Nokia
 *
 * Licensed under the BSD 3 Clause license
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "catch2/catch.hpp"
#include "proc.hpp"

namespace poor_perf
{

TEST_CASE("parse 1")
{
    region_t region{"56373c46a000-56373c46b000 r-xp 0000000a 103:02 10628889                  /usr/bin/python2.7"};
    REQUIRE(region.start == 0x56373c46a000);
    REQUIRE(region.end == 0x56373c46b000);
    REQUIRE(region.perms == "r-xp");
    REQUIRE(region.offset == 0xa);
    REQUIRE(region.pathname == "/usr/bin/python2.7");
    REQUIRE(region.exec());
}

TEST_CASE("parse 2")
{
    region_t region{"56373c46a000-56373c46b000 r--p 0000000a 103:02 10628889"};
    REQUIRE(region.start == 0x56373c46a000);
    REQUIRE(region.end == 0x56373c46b000);
    REQUIRE(region.perms == "r--p");
    REQUIRE(region.offset == 0xa);
    REQUIRE(region.pathname == "");
    REQUIRE(!region.exec());
}

TEST_CASE("parse 3")
{
    region_t region{"ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]"};
    REQUIRE(region.start == 0xffffffffff600000);
    REQUIRE(region.end == 0xffffffffff601000);
    REQUIRE(region.pathname == "[vsyscall]");
    REQUIRE(region.exec());
}

} // namespace
