/**
 * Copyright 2019 Nokia
 *
 * Licensed under the BSD 3 Clause license
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
    if (ret)
        throw std::runtime_error("failed to set thread scheduling");
}

void set_this_thread_name(const char* name)
{
    int ret = pthread_setname_np(pthread_self(), name);
    if (ret)
        throw std::runtime_error("failed to set thread name");
}

void set_this_thread_affinity(std::size_t cpu)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (ret)
    {
        std::stringstream ss;
        ss << "failed to set thread affinity to cpu " << cpu;
        throw std::runtime_error(ss.str());
    }
}

struct watchdog
{
    watchdog(std::size_t cpu)
    {
        auto normal_thread = [this, cpu]
        {
            set_this_thread_name("poor-watchdog");
            set_this_thread_affinity(cpu);

            while (_running.load(std::memory_order_relaxed))
            {
                _flag.store(true, std::memory_order_relaxed);
                std::this_thread::sleep_for(_interval);
            }
        };

        _thread = std::thread{normal_thread};
    }

    ~watchdog()
    {
        std::cerr << "waiting for watchdog thread to stop\n";
        _running.store(false, std::memory_order_relaxed);
        _thread.join();
    }

    auto interval() const
    {
        return _interval;
    }

    bool ping()
    {
        // false will mean that normal thread was unable to do the work
        return _flag.exchange(false, std::memory_order_relaxed);
    }

private:
    std::atomic<bool> _running{true};
    std::atomic<bool> _flag{true};
    std::thread _thread;
    std::chrono::seconds _interval{1};
};
