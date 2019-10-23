#include "catch2/catch.hpp"
#include "proc.hpp"

namespace poor_perf
{

TEST_CASE("parse 1")
{
    map_entry_t entry{"56373c46a000-56373c46b000 r-xp 0000000a 103:02 10628889                  /usr/bin/python2.7"};
    REQUIRE(entry.start == 0x56373c46a000);
    REQUIRE(entry.end == 0x56373c46b000);
    REQUIRE(entry.perms == "r-xp");
    REQUIRE(entry.offset == 0xa);
    REQUIRE(entry.pathname == "/usr/bin/python2.7");
    REQUIRE(entry.exec());
}

TEST_CASE("parse 2")
{
    map_entry_t entry{"56373c46a000-56373c46b000 r--p 0000000a 103:02 10628889"};
    REQUIRE(entry.start == 0x56373c46a000);
    REQUIRE(entry.end == 0x56373c46b000);
    REQUIRE(entry.perms == "r--p");
    REQUIRE(entry.offset == 0xa);
    REQUIRE(entry.pathname == "");
    REQUIRE(!entry.exec());
}

TEST_CASE("parse 3")
{
    map_entry_t entry{"ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]"};
    REQUIRE(entry.start == 0xffffffffff600000);
    REQUIRE(entry.end == 0xffffffffff601000);
    REQUIRE(entry.pathname == "[vsyscall]");
    REQUIRE(entry.exec());
}

} // namespace
