#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>
#include <signal.h>

#include <boost/program_options.hpp>

#include "perf.hpp"
#include "fifo.hpp"
#include "proc.hpp"
#include "event_loop.hpp"
#include "utils.hpp"

namespace poor_perf
{

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

volatile sig_atomic_t signal_status = 0;

void signal_handler(int signal)
{
    signal_status = signal;
}

std::fstream open_file_for_writing(const std::string& output)
{
    std::fstream f{output, std::fstream::out | std::fstream:: app | std::fstream::ate};
    if (!f)
        throw std::runtime_error{"could not open '" + output + "' for writing"};
    return std::move(f);
}

void profile_for(const std::string& output, const running_processes_snapshot& processes, std::chrono::seconds secs)
{
    std::cerr << "starting profile\n";

    event_loop loop{signal_status};
    perf_session session{0};
    loop.add_fd(session.fd());

    auto f = open_file_for_writing(output);

    loop.run_for(secs, [&]
    {
        session.read_some([&](const auto& sample)
        {
            auto p = processes.find(sample.pid);
            f << std::dec << sample.pid << ' ' << p.comm << ' ' << p.find_dso(sample.ip) << std::endl;
        });
    });

    f << "done\n" << std::endl;
}

enum class trigger
{
    none,
    control_fifo,
    watchdog
};

std::ostream& operator<<(std::ostream& os, trigger trigger)
{
    switch (trigger)
    {
        case trigger::none: return os << "none";
        case trigger::control_fifo: return os << "control fifo";
        case trigger::watchdog: return os << "watchdog";
        default: return os << "<unknown>";
    }
    return os;
}

auto wait_for_trigger(watchdog& wdg)
{
    event_loop loop{signal_status};

    fifo control_fifo{CONTROL_FIFO_PATH};
    loop.add_fd(control_fifo.fd());

    std::cerr << "control fifo created at " << CONTROL_FIFO_PATH << '\n';
    std::cerr << "waiting for trigger\n";

    auto trigger = trigger::none;
    while (trigger == trigger::none && !signal_status)
    {
        auto read_control_fifo = [&]
        {
            std::cerr << "woke up by control fifo\n";
            std::cerr << control_fifo.read();
            loop.stop();
            trigger = trigger::control_fifo;
        };

        auto timeout = [&]
        {
            std::cerr << "watchdog ping\n";
            if (!wdg.ping())
            {
                std::cerr << "watchdog is starved\n";
                loop.stop();
                trigger = trigger::watchdog;
            }
        };

        loop.run_for(std::chrono::seconds{2}, read_control_fifo, timeout);
    }

    return trigger;
}

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

std::ostream& operator<<(std::ostream& os, const mode_t& mode)
{
    return os << "mode";
}

auto parse_options(int argc, char **argv)
{
    namespace po = boost::program_options;

    po::options_description desc;
    desc.add_options()
        ("output", po::value<std::string>()->default_value("/rom/profile.txt"))
        ("mode", po::value<mode_t>()->default_value(mode_t::watchdog));

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    return vm;
}

void watchdog_mode(const boost::program_options::variables_map& options)
{
    running_processes_snapshot proc;
    watchdog wdg;

    const auto output = options["output"].as<std::string>();
    std::cerr << "watchdog mode, output: " << output << "\n";
    open_file_for_writing(output) << current_time{} << ": watchdog mode started\n\n";

    // childs inherit sched so set it after watchdog is started
    set_this_thread_into_realtime();

    while (!signal_status)
    {
        auto t = wait_for_trigger(wdg);

        if (t != trigger::none)
        {
            open_file_for_writing(output) << current_time{} << ": woke up by " << t << '\n';
            profile_for(output, proc, std::chrono::seconds(3));
        }
    }
}

void oneshot_mode(const boost::program_options::variables_map& options)
{
    running_processes_snapshot proc;

    const auto output = options["output"].as<std::string>();
    std::cerr << "oneshot mode, output: " << output << "\n";

    set_this_thread_into_realtime();

    open_file_for_writing(output) << current_time{} << ": oneshot profiling\n";
    profile_for(output, proc, std::chrono::seconds(3));
}

} // namespace

int main(int argc, char **argv)
{
    const auto options = poor_perf::parse_options(argc, argv);

    ::set_this_thread_name("poor-perf");
    ::set_this_thread_affinity(1);
    ::signal(SIGINT, poor_perf::signal_handler);
    ::signal(SIGTERM, poor_perf::signal_handler);

    switch (options["mode"].as<poor_perf::mode_t>())
    {
    case poor_perf::mode_t::watchdog:
        poor_perf::watchdog_mode(options);
        break;
    case poor_perf::mode_t::oneshot:
        poor_perf::oneshot_mode(options);
        break;
    }
}

