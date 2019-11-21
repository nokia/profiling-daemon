#pragma once

#include <iostream>
#include <boost/program_options.hpp>
#include <cxxopts.hpp>

namespace poor_perf
{

enum class mode_t
{
    watchdog,
    oneshot,
    help
};

std::istream& operator>>(std::istream& is, mode_t& mode)
{
    std::string s;
    is >> s;

    if (s == "watchdog")
        mode = mode_t::watchdog;
    else if (s == "oneshot")
        mode = mode_t::oneshot;
    else if (s == "help")
        mode = mode_t::help;
    else
        is.setstate(std::ios_base::failbit);

    return is;
}

std::ostream& operator<<(std::ostream& os, const mode_t&)
{
    return os << "mode";
}

/**
 * A wrapper for cxxopts library.
 *
 * Need this since cxxopts's result gets invalidated when `cxxopts::Options` object gets destroyed
 */
struct options_t
{
    options_t(int argc, char **argv) : _desc{"profd", "profiling daemon"}
    {
        _desc.add_options()
            ("help", "show help")
            ("output", "where to write profile to; use - for standard output", cxxopts::value<std::string>()->default_value("/rom/profile.txt"))
            ("cpu", "cpu core to profile on", cxxopts::value<std::size_t>()->default_value("0"))
            ("duration", "duration in seconds", cxxopts::value<std::size_t>()->default_value("5"))
            ("mode", "either watchdog or oneshot", cxxopts::value<mode_t>()->default_value("watchdog"));

        auto result = _desc.parse(argc, argv);

        mode = result.count("help") ? mode_t::help : result["mode"].as<mode_t>();
        output = result["output"].as<std::string>();
        duration = result["duration"].as<std::size_t>();
        cpu = result["cpu"].as<std::size_t>();
    }

    void show_help() const
    {
        std::cout << _desc.help() << std::endl;
    }

    mode_t mode;
    std::string output;
    std::size_t duration;
    std::size_t cpu;

private:
    cxxopts::Options _desc;
};

} // namespace
