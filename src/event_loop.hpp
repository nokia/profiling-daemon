#pragma once

#include <chrono>
#include <queue>

#include <sys/epoll.h>

struct event_loop
{
    using clock = std::chrono::steady_clock;

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
            run_once(std::forward<F>(f));
        }
    }

    template<class F>
    void run_for(std::chrono::milliseconds duration, F&& f)
    {
        _run_until = clock::now() + duration;
        while (clock::now() < _run_until)
            run_once(std::forward<F>(f));
    }

    ~event_loop()
    {
        ::close(_fd);
    }

private:
    clock::time_point _run_until;
    int _fd;
};

