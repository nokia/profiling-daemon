#pragma once

#include <iostream>
#include <boost/program_options.hpp>
#include <cxxopts.hpp>

namespace poor_perf
{

enum class mode_t
{
    watchdog,
    oneshot
};

std::istream& operator>>(std::istream& is, mode_t& mode)
{
    std::string s;
    is >> s;

    if (s == "watchdog")
        mode = mode_t::watchdog;
    else if (s == "oneshot")
        mode = mode_t::oneshot;
    else
        is.setstate(std::ios_base::failbit);

    return is;
}

std::ostream& operator<<(std::ostream& os, const mode_t&)
{
    return os << "mode";
}

struct options_t
{
    options_t(const cxxopts::ParseResult& opts)
        : mode{opts["mode"].as<mode_t>()},
        output{opts["output"].as<std::string>()},
        duration{opts["duration"].as<std::size_t>()},
        cpu{opts["cpu"].as<std::size_t>()}
    {
    }

    mode_t mode;
    std::string output;
    std::size_t duration;
    std::size_t cpu;
};

auto parse_options(int argc, char **argv)
{
    cxxopts::Options desc{"profd", "profiling daemon"};

    desc.add_options()
        ("output", "where to write profile to; use - for standard output", cxxopts::value<std::string>()->default_value("/rom/profile.txt"))
        ("cpu", "", cxxopts::value<std::size_t>()->default_value("0"))
        ("duration", "", cxxopts::value<std::size_t>()->default_value("5"))
        ("mode", "either watchdog or oneshot", cxxopts::value<mode_t>()->default_value("watchdog"));

    // need this `options` wrapper since cxxopts's result gets invalidated when `cxxopts::Options` object gets destroyed
    return options_t{desc.parse(argc, argv)};
}

} // namespace
