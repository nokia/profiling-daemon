#pragma once

#include <chrono>
#include <ostream>
#include <iomanip>
#include <pthread.h>

struct current_time
{
};

std::ostream& operator<<(std::ostream& os, current_time)
{
    using clock = std::chrono::system_clock;
    std::time_t now_c = clock::to_time_t(clock::now());
    return os << std::put_time(std::localtime(&now_c), "%F %T");
}

enum class thread_priority
{
    low,
    high
};

void set_this_thread_scheduling(thread_priority priority)
{
    int sched = priority == thread_priority::low ? SCHED_OTHER : SCHED_FIFO;
    int prio = priority == thread_priority::low ? 50 : sched_get_priority_max(sched);

    ::sched_param param{};
    param.sched_priority = prio;
    int ret = pthread_setschedparam(pthread_self(), sched, &param);
    std::cerr << ret;
    if (ret)
        throw std::runtime_error("failed to set thread scheduling");
}
