/**
 * Copyright 2019 Nokia
 *
 * Licensed under the BSD 3 Clause license
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <sys/mman.h>

namespace
{

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                    group_fd, flags);
    return ret;
}

}

/**
 * Advance the pointer by bytes.
 */
template<class A, class B>
auto trivial_pointer_advance(A a, B b)
{
    char* ca = reinterpret_cast<char*>(a);
    return reinterpret_cast<A>(ca + b);
}

struct cyclic_buffer_view
{
    cyclic_buffer_view(char* start, std::size_t size)
        : _start(start), _pointer(start), _size(size), _read_size(0)
    {
    }

    template<class T>
    T read()
    {
        // value wraps around cyclic buffer
        if (_pointer + sizeof(T) > _start + _size)
        {
            const auto size_at_the_bottom = _start + _size - _pointer;
            const auto remainder_size = sizeof(T) - size_at_the_bottom;
            T value;
            ::memcpy(&value, _pointer, size_at_the_bottom);
            ::memcpy(trivial_pointer_advance(&value, size_at_the_bottom), _start, remainder_size);
            _pointer = _start + remainder_size;
            _read_size += sizeof(T);
            return value;
        }

        auto old = _pointer;
        _pointer += sizeof(T);
        _read_size += sizeof(T);
        return *reinterpret_cast<T*>(old);
    }

    void skip(std::size_t size)
    {
        // value wraps around cyclic buffer
        if (_pointer + size > _start + _size)
        {
            const auto size_at_the_bottom = _start + _size - _pointer;
            const auto remainder_size = size - size_at_the_bottom;
            _pointer = _start + remainder_size;
            _read_size += size;
            return;
        }

        _pointer += size;
        _read_size += size;
    }

    auto total_read_size() const
    {
        return _read_size;
    }

private:
    char* _start;
    char* _pointer;
    std::size_t _size;
    std::size_t _read_size;
};

struct sample_t
{
    constexpr static std::uint32_t type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_CPU;
    std::uint64_t ip;
    std::uint32_t pid, tid;
    std::uint64_t time;
    std::uint32_t cpu, res;
};

struct perf_fd
{
    explicit perf_fd(std::size_t cpu)
    {
        perf_event_attr pe{};
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(perf_event_attr);
        pe.config = PERF_COUNT_HW_CPU_CYCLES;
        pe.sample_freq = 7000;
        pe.sample_type = sample_t::type;
        pe.disabled = 1;
        pe.exclude_kernel = 0;
        pe.exclude_hv = 1;
        pe.mmap = 1;
        pe.freq = 1;

        _fd = perf_event_open(&pe, -1, cpu, -1, 0);

        if (_fd == -1)
            throw std::runtime_error("perf_event_open failed, perhaps you do not have enough permissions");

        std::size_t mmap_size = sysconf(_SC_PAGESIZE) * 2;

        _buffer = reinterpret_cast<char*>(::mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0));
        if (_buffer == MAP_FAILED)
            throw std::runtime_error("mmap failed, I did never wonder why would it fail");

        ioctl(_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    ~perf_fd()
    {
        ioctl(_fd, PERF_EVENT_IOC_DISABLE, 0);
        ::close(_fd);
    }

    char* buffer()
    {
        return _buffer;
    }

    auto fd() const
    {
        return _fd;
    }

private:
    int _fd;
    char* _buffer;
};

struct perf_session
{
    perf_session(std::size_t cpu)
        : _fd{cpu},
          _metadata(reinterpret_cast<perf_event_mmap_page*>(_fd.buffer())),
          _data_view{_fd.buffer() + _metadata->data_offset, _metadata->data_size}
    {
    }

    template<class F>
    void read_some(F&& f)
    {
        // man says that after reading data_head, rmb should be issued
        auto data_head = _metadata->data_head;
        __sync_synchronize();

        while (_data_view.total_read_size() < data_head)
        {
            auto header = _data_view.read<perf_event_header>();

            switch (header.type)
            {
                case PERF_RECORD_SAMPLE:
                {
                    auto sample = _data_view.read<sample_t>();
                    assert(header.size == sizeof(header) + sizeof(sample_t));
                    f(sample);
                    break;
                }
                default:
                    _data_view.skip(header.size - sizeof(perf_event_header));
            }
        }

        // we are done with the reading so we can write the tail to let the kernel know
        // that it can continue with writes
        _metadata->data_tail = _data_view.total_read_size();
    }

    auto fd() const
    {
        return _fd.fd();
    }

private:
    perf_fd _fd;
    perf_event_mmap_page* _metadata;
    cyclic_buffer_view _data_view;
};

