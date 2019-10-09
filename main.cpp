#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <linux/perf_event.h>

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                    group_fd, flags);
    return ret;
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
    constexpr static std::uint32_t type = PERF_SAMPLE_IP | PERF_SAMPLE_TIME;
    std::uint64_t ip;
    std::uint64_t time;
};

struct perf_fd
{
    perf_fd()
    {
        perf_event_attr pe{};
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        pe.sample_freq = 7000;
        pe.sample_type = sample_t::type;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.mmap = 1;
        pe.freq = 1;

        fd = perf_event_open(&pe, -1, 0, -1, 0);

        if (fd == -1)
            throw std::runtime_error("perf_event_open failed, perhaps you do not have enough permissions");

        constexpr std::size_t page_size = 4 * 1024;
        constexpr std::size_t mmap_size = page_size * 2;

        _buffer = reinterpret_cast<char*>(mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (_buffer == MAP_FAILED)
            throw std::runtime_error("mmap failed, I did never wonder why would it fail");

        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    ~perf_fd()
    {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        ::close(fd);
    }

    char* buffer()
    {
        return _buffer;
    }

private:
    int fd;
    char* _buffer;
};

struct perf_session
{
    perf_session()
        : metadata(reinterpret_cast<perf_event_mmap_page*>(_fd.buffer())),
          _data_view{_fd.buffer() + metadata->data_offset, metadata->data_size}
    {
    }

    template<class F>
    void read_some(F&& f)
    {
        // man says that after reading data_head, rmb should be issued
        auto data_head = metadata->data_head;
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
        metadata->data_tail = _data_view.total_read_size();
    }

private:
    perf_fd _fd;
    perf_event_mmap_page* metadata;
    cyclic_buffer_view _data_view;
};

int main(int argc, char **argv)
{
    perf_session session;

    while(true)
    {
        session.read_some([](const auto& sample)
        {
            std::cerr << "ip: " << std::hex << sample.ip << '\n';
        });
    }
}

