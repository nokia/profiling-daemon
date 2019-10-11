#pragma once

#include <chrono>
#include <ostream>
#include <iomanip>
#include <pthread.h>
#include <thread>

struct current_time
{
};

std::ostream& operator<<(std::ostream& os, current_time)
{
    using clock = std::chrono::system_clock;
    std::time_t now_c = clock::to_time_t(clock::now());
    return os << std::put_time(std::localtime(&now_c), "%F %T");
}

void set_this_thread_into_realtime()
{
    ::sched_param param{};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    std::cerr << ret;
    if (ret)
        throw std::runtime_error("failed to set thread scheduling");
}

struct watchdog
{
    watchdog()
    {
        auto increment = [&]
        {
            while (true)
            {
                _counter++;
                std::this_thread::sleep_for(std::chrono::seconds{1});
            }
        };

        _thread = std::thread{increment};
    }

private:
    std::atomic<int> _counter;
    std::thread _thread;
};
