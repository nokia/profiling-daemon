#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include <sys/epoll.h>

#include "perf.hpp"
#include "proc_maps.hpp"

struct event_loop
{
    event_loop()
    {
        _fd = ::epoll_create1(0);
        if (_fd == -1)
            throw std::runtime_error{"could not create epoll instance"};
    }

    ~event_loop()
    {
        ::close(_fd);
    }

private:
    int _fd;
};

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

