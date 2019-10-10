#pragma once

#include <fcntl.h>
#include <sys/stat.h>

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

