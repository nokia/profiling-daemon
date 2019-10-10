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

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

volatile sig_atomic_t signal_status = 0;

void signal_handler(int signal)
{
    signal_status = signal;
}

template<class Maps>
void profile_for(const Maps& pid_to_maps, std::chrono::seconds secs)
{
    std::cerr << "starting profile\n";

    event_loop loop{signal_status};
    perf_session session;
    loop.add_fd(session.fd());

    loop.run_for(secs, [&]
    {
        std::cerr << "waking up for perf data\n";
        std::size_t event_count = 0;

        session.read_some([&](const auto& sample)
        {
            auto it = pid_to_maps.find(sample.pid);
            if (it != pid_to_maps.end())
            {
                std::cout << std::dec << it->first << " " << it->second.comm
                          << " " << it->second.find_dso(sample.ip) << std::endl;
            }

            event_count++;
        });

        std::cerr << "captured " << std::dec << event_count << " events\n";
    });
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

    auto pid_to_maps = parse_maps();

    while (!signal_status)
    {
        wait_for_trigger();

        if (!signal_status)
            profile_for(pid_to_maps, std::chrono::seconds(3));
    }
}

