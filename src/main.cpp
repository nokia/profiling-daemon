#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include <fcntl.h>
#include <sys/stat.h>

#include "perf.hpp"
#include "proc_maps.hpp"
#include "event_loop.hpp"

struct fifo
{
    explicit fifo(const char* path) : _path(path)
    {
        int ret = ::mkfifo(_path, 0x666);
        if (ret != 0)
            throw std::runtime_error{"could not create control fifo"};

        _fd = ::open(_path, O_RDONLY | O_NONBLOCK);
        if (_fd == -1)
            throw std::runtime_error{"could not open control fifo for reading"};
    }

    ~fifo()
    {
        ::close(_fd);
        ::unlink(_path);
    }

    char read()
    {
        char ret;
        ::read(_fd, &ret, sizeof(ret));
        return ret;
    }

    int fd() const
    {
        return _fd;
    }

private:
    const char* _path;
    int _fd;
};

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

template<class Maps>
void profile_for(const Maps& pid_to_maps, std::chrono::seconds secs)
{
    std::cerr << "starting profile\n";

    event_loop loop;
    perf_session session;
    loop.add_fd(session.fd());

    loop.run_for(secs, [&]
    {
        std::cerr << "waking up for perf data\n";
        std::size_t event_count = 0;

        session.read_some([&](const auto& sample)
        {
            auto it = pid_to_maps.find(sample.pid);
            if (it != pid_to_maps.end())
            {
                std::cerr << std::dec << it->first << " " << it->second.comm
                        << ": " << it->second.find_dso(sample.ip) << '\n';
            }

            event_count++;
        });

        std::cerr << "captured " << std::dec << event_count << " events\n";
    });
}

int main(int argc, char **argv)
{
    auto pid_to_maps = parse_maps();

    while (true)
    {
        event_loop loop;

        fifo control_fifo{CONTROL_FIFO_PATH};
        std::cerr << "control fifo created at " << CONTROL_FIFO_PATH << '\n';

        loop.add_fd(control_fifo.fd());
        loop.run_forever([&]
        {
            std::cerr << "woke up by control fifo\n";
            loop.stop();
        });

        profile_for(pid_to_maps, std::chrono::seconds(3));
    }
}

