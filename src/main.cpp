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

    void add_fd(int fd)
    {
        epoll_event ev;
        ev.events = EPOLLIN;
        epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev);
    }

    template<class F>
    void run_once(F&& f)
    {
        epoll_event ev[1];
        auto ret = epoll_wait(_fd, ev, 1, -1);
        if (ret == 1)
        {
            f();
        }
    }

    template<class F>
    void run_forever(F&& f)
    {
        while (true)
        {
            run_once(f);
        }
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
    event_loop loop;

    loop.add_fd(session.fd());

    loop.run_forever([&]
    {
        std::cerr << "waking up for perf data\n";
        std::size_t event_count = 0;

        session.read_some([&](const auto& sample)
        {
            std::cerr << std::dec << "pid: " << sample.pid << ", tid: " << sample.tid << ", ip: " << std::hex << sample.ip << '\n';
            event_count++;
        });

        std::cerr << "captured " << std::dec << event_count << " events\n";
    });
}

