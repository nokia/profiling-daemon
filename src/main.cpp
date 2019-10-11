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

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

volatile sig_atomic_t signal_status = 0;

void signal_handler(int signal)
{
    signal_status = signal;
}

template<class Maps>
void profile_for(const Maps& processes, std::chrono::seconds secs)
{
    std::cerr << "starting profile\n";

    event_loop loop{signal_status};
    perf_session session;
    loop.add_fd(session.fd());

    std::cout << current_time{} << std::endl;
    std::cout << "pid comm dso addr" << std::endl;

    loop.run_for(secs, [&]
    {
        session.read_some([&](const auto& sample)
        {
            auto p = processes.find(sample.pid);
            std::cout << std::dec << sample.pid << ' ' << p.comm << ' ' << p.find_dso(sample.ip) << std::endl;
        });
    });

    std::cerr << "done\n";
    std::cout << std::endl;
}

void wait_for_trigger(fifo& control_fifo, watchdog& wdg)
{
    event_loop loop{signal_status};

    loop.add_fd(control_fifo.fd());

    std::cerr << "control fifo created at " << CONTROL_FIFO_PATH << '\n';
    std::cerr << "waiting for trigger\n";

    bool trigger = false;
    while (!trigger && !signal_status)
    {
        auto read_control_fifo = [&]
        {
            std::cerr << "woke up by control fifo\n";
            loop.stop();
            trigger = true;
        };

        auto timeout = [&]
        {
            std::cerr << "watchdog ping\n";
            if (!wdg.ping())
            {
                std::cerr << "normal thread is starved\n";
                loop.stop();
                trigger = true;
            }
        };

        loop.run_for(std::chrono::seconds{6}, read_control_fifo, timeout);
    }
}

int main(int argc, char **argv)
{
    set_this_thread_name("poor-profiler");
    ::signal(SIGINT, signal_handler);
    ::signal(SIGTERM, signal_handler);

    fifo control_fifo{CONTROL_FIFO_PATH};
    running_processes_snapshot proc;
    watchdog wdg;

    // childs inherit sched so set it after watchdog is started
    set_this_thread_into_realtime();

    std::cerr << "doing initial profiling\n";
    profile_for(proc, std::chrono::seconds(3));

    while (!signal_status)
    {
        wait_for_trigger(control_fifo, wdg);

        if (!signal_status)
            profile_for(proc, std::chrono::seconds(3));
    }
}

