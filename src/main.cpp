#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>
#include <signal.h>

#include "perf.hpp"
#include "fifo.hpp"
#include "proc.hpp"
#include "event_loop.hpp"
#include "utils.hpp"
#include "output.hpp"
#include "options.hpp"

namespace poor_perf
{

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

volatile sig_atomic_t signal_status = 0;

void signal_handler(int signal)
{
    signal_status = signal;
}

void profile_for(output_stream& output, std::size_t cpu, const running_processes_snapshot& processes, std::chrono::seconds secs)
{
    event_loop loop{signal_status};

    output.message("profiling cpu: ", cpu);
    output << "$ time;cpu;pid;comm;pathname;addr;name\n";

    perf_session session{cpu};
    loop.add_fd(session.fd());

    loop.run_for(secs, [&](auto fd)
    {
        session.read_some([&](const auto& sample)
        {
            auto s = processes.find_symbol(sample.pid, sample.ip);

            output << std::dec << sample.time << ';' << sample.cpu << ';' << sample.pid << ';'
                   << s.comm << ';'
                   << s.pathname <<
                   ";0x" << std::hex << s.addr << ';'
                   << s.name << '\n';
        });

        output.stream().flush();
    });

    output.message("done");
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
        auto read_control_fifo = [&](int)
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

        loop.run_for(wdg.interval() * 2, read_control_fifo, timeout);
    }

    return trigger;
}

void watchdog_mode(const options_t& options)
{
    running_processes_snapshot proc;
    watchdog wdg{options.cpu};

    {
        output_stream f{options.output};
        f.message("watchdog mode started on cpu ", options.cpu);
    }

    // childs inherit sched so set it after watchdog is started
    set_this_thread_into_realtime();

    while (!signal_status)
    {
        auto t = wait_for_trigger(wdg);

        if (t != trigger::none)
        {
            // I want to make sure that after profiling is done, the
            // file is flushed and closed
            output_stream f{options.output};
            f.message("woke up by ", t);
            profile_for(f, options.cpu, proc, std::chrono::seconds{options.duration});
        }
    }
}

void oneshot_mode(const options_t& options)
{
    const auto output = options.output;
    const auto duration = options.duration;
    const auto cpu = options.cpu;

    set_this_thread_into_realtime();
    running_processes_snapshot proc;
    output_stream f{output};
    f.message("oneshot profiling");
    profile_for(f, cpu, proc, std::chrono::seconds{duration});
}

} // namespace

int main(int argc, char **argv)
{
    const auto options = poor_perf::options_t{argc, argv};

    ::set_this_thread_name("poor-perf");
    ::set_this_thread_affinity(1);
    ::signal(SIGINT, poor_perf::signal_handler);
    ::signal(SIGTERM, poor_perf::signal_handler);

    if (options.mode == poor_perf::mode_t::watchdog)
        poor_perf::watchdog_mode(options);
    else if (options.mode == poor_perf::mode_t::oneshot)
        poor_perf::oneshot_mode(options);
    else
        options.show_help();
}

