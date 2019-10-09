#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include "perf.hpp"
#include "proc_maps.hpp"

int main(int argc, char **argv)
{
    parse_maps();
    perf_session session;

    while (true)
    {
        //std::cerr << "buf\n";
        session.read_some([](const auto& sample)
        {
            std::cerr << std::dec << "pid: " << sample.pid << ", tid: " << sample.tid << ", ip: " << std::hex << sample.ip << '\n';
        });
    }
}

