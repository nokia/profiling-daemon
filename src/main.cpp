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
#include "proc_maps.hpp"
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

void wait_for_trigger()
{
    event_loop loop{signal_status};

    fifo control_fifo{CONTROL_FIFO_PATH};
    loop.add_fd(control_fifo.fd());

    std::cerr << "control fifo created at " << CONTROL_FIFO_PATH << '\n';
    std::cerr << "waiting for trigger\n";

    loop.run_forever([&]
    {
        std::cerr << "woke up by control fifo\n";
        loop.stop();
    });
}

int main(int argc, char **argv)
{
    ::signal(SIGINT, signal_handler);

    running_processes_snapshot proc;

    while (!signal_status)
    {
        wait_for_trigger();

        if (!signal_status)
            profile_for(proc, std::chrono::seconds(3));
    }
}

