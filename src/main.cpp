#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include "perf.hpp"
#include "proc_maps.hpp"
#include "event_loop.hpp"

int main(int argc, char **argv)
{
    auto pid_to_maps = parse_maps();
    perf_session session;
    event_loop loop;

    loop.add_fd(session.fd());

    loop.run_forever([&]
    {
        std::cerr << "waking up for perf data\n";
        std::size_t event_count = 0;

        session.read_some([&](const auto& sample)
        {
            auto it = pid_to_maps.find(sample.pid);
            if (it != pid_to_maps.end())
            {
                std::cerr << std::dec << it->first << " " << it->second.comm
                          << ": " << it->second.find_dso(sample.ip) << '\n';
            }

            event_count++;
        });

        std::cerr << "captured " << std::dec << event_count << " events\n";
    });
}

