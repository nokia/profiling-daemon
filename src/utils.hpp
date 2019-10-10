#pragma once

#include <chrono>
#include <ostream>
#include <iomanip>

struct current_time
{
};

std::ostream& operator<<(std::ostream& os, current_time)
{
    using clock = std::chrono::system_clock;
    std::time_t now_c = clock::to_time_t(clock::now());
    return os << std::put_time(std::localtime(&now_c), "%F %T");
}
