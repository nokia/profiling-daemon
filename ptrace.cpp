#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#define CHECK_ERROR(x) { auto err = x; if (err != 0) { std::cerr << #x << " failed: " << strerror(err) << std::endl; exit(1); } }

#ifdef __amd64__
auto retrieve_ip(pid_t pid)
{
    user_regs_struct regs{};

    CHECK_ERROR(ptrace(PTRACE_SEIZE, pid, nullptr, nullptr));
    CHECK_ERROR(ptrace(PTRACE_INTERRUPT, pid, nullptr, nullptr));

    int status;
    ::waitpid(pid, &status, 0);

    CHECK_ERROR(ptrace(PTRACE_GETREGS, pid, nullptr, &regs));
    CHECK_ERROR(ptrace(PTRACE_DETACH, pid, nullptr, nullptr));

    return regs.rip;
}
#endif // __amd64__

#ifdef __arm__
auto retrieve_ip(pid_t pid)
{
    CHECK_ERROR(ptrace(PTRACE_SEIZE, pid, nullptr, nullptr));
    CHECK_ERROR(ptrace(PTRACE_INTERRUPT, pid, nullptr, nullptr));

    int status;
    ::waitpid(pid, &status, 0);

    user_regs regs{};
    CHECK_ERROR(ptrace(PTRACE_GETREGS, pid, nullptr, &regs));

    CHECK_ERROR(ptrace(PTRACE_DETACH, pid, nullptr, nullptr));

    // https://en.wikipedia.org/wiki/ARM_architecture#Registers
    return regs.uregs[15];
}
#endif // __arm__

/**
 * Just an size_t but with default ctor.
 */
struct counter_t
{
    std::size_t value = 0;
};

int main(int argc, char* argv[])
{
    int pid;

    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " PID" << std::endl;
        return 1;
    }

    pid = std::stoi(argv[1]);

    std::cout << pid << std::endl;

    using ip_t = decltype(retrieve_ip(std::declval<pid_t>()));
    std::unordered_map<ip_t, counter_t> histogram;

    while (true)
    {
        auto ip = retrieve_ip(pid);
        //std::cout << "rip: " << std::hex << "0x" << ip << std::endl;
        histogram[ip].value++;

        for (auto elem : histogram)
            std::cerr << std::dec << std::setw(10) << elem.second.value << " " << std::hex << elem.first << '\n';
        std::cerr << '\n';

        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
