#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sys/ptrace.h>
#include <sys/user.h>

#define CHECK_ERROR(x) if (auto err = x; err != 0) { std::cerr << #x << " failed: " << strerror(err) << std::endl; exit(1); }

auto retrieve_ip(pid_t pid)
{
    user_regs_struct regs{};

    CHECK_ERROR(ptrace(PTRACE_SEIZE, pid, nullptr, nullptr));
    CHECK_ERROR(ptrace(PTRACE_INTERRUPT, pid, nullptr, nullptr));

    // TODO: waitpid
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK_ERROR(ptrace(PTRACE_GETREGS, pid, nullptr, &regs));
    CHECK_ERROR(ptrace(PTRACE_DETACH, pid, nullptr, nullptr));

    return regs.rip;
}

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

    while (true)
    {
        std::cout << "rip: " << std::hex << "0x" << retrieve_ip(pid) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
