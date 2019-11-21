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

auto parse_options(int argc, char **argv)
{
    namespace po = boost::program_options;

    po::options_description desc;
    desc.add_options()
        ("output", po::value<std::string>()->default_value("/rom/profile.txt"))
        ("cpu", po::value<std::size_t>()->default_value(0u))
        ("duration", po::value<std::size_t>()->default_value(5u))
        ("mode", po::value<mode_t>()->default_value(mode_t::watchdog));

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    return vm;
}

} // namespace
