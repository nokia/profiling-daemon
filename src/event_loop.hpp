#pragma once

#include <chrono>
#include <queue>

#include <signal.h>
#include <sys/epoll.h>

struct event_loop
{
    using clock = std::chrono::steady_clock;

    event_loop(volatile sig_atomic_t& signal_status) : _signal_status(signal_status)
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
    bool run_once(F&& f, std::chrono::milliseconds timeout = std::chrono::milliseconds{-1})
    {
        epoll_event ev[1];
        auto ret = epoll_wait(_fd, ev, 1, timeout.count());
        if (ret == 1)
        {
            f();
            return true;
        }
        return false;
    }

    template<class F>
    void run_forever(F&& f)
    {
        _running = true;
        while (_running && !_signal_status)
            run_once(std::forward<F>(f));
    }

    template<class F, class Timeout>
    void run_for(std::chrono::milliseconds duration, F&& f, Timeout&& timeout)
    {
        _run_until = clock::now() + duration;
        _running = true;

        while (true)
        {
            if (clock::now() >= _run_until)
            {
                timeout();
                return;
            }

            if (!_running || _signal_status)
                return;

            run_once(std::forward<F>(f), std::chrono::duration_cast<std::chrono::milliseconds>(_run_until - clock::now()));
        }
    }

    template<class F>
    void run_for(std::chrono::milliseconds duration, F&& f)
    {
        run_for(duration, std::forward<F>(f), []{});
    }

    void stop()
    {
        _running = false;
    }

    ~event_loop()
    {
        ::close(_fd);
    }

private:
    clock::time_point _run_until;
    int _fd;
    bool _running;
    volatile sig_atomic_t& _signal_status;
};

