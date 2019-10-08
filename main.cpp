#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <cassert>

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                    group_fd, flags);
    return ret;
}

struct cyclic_buffer_view
{
    cyclic_buffer_view(void* start, std::size_t size)
        : _start(start), _pointer(start), _size(size), _read_size(0)
    {
    }

    template<class T>
    T* read()
    {
        return reinterpret_cast<T*>(read(sizeof(T)));
    }

    void* read(std::size_t size)
    {
        if (_pointer + size > _start + _size)
            throw std::runtime_error{"this read would wrap the buffer but this is not implemented yet"};

        auto old = _pointer;
        _pointer += size;
        _read_size += size;
        return old;
    }

    auto total_read_size() const
    {
        return _read_size;
    }

private:
    void* _start;
    void* _pointer;
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

        _buffer = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (_buffer == MAP_FAILED)
            throw std::runtime_error("mmap failed, I did never wonder why would it fail");
    }

    ~perf_fd()
    {
        ::close(fd);
    }

    void* buffer()
    {
        return _buffer;
    }

    void enable()
    {
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    void disable()
    {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    }

private:
    int fd;
    void* _buffer;
};

struct perf_session
{
    perf_session()
        : metadata(reinterpret_cast<perf_event_mmap_page*>(_fd.buffer())),
          _data_view{_fd.buffer() + metadata->data_offset, metadata->data_size}
    {
    }

    void enable()
    {
        _fd.enable();
    }

    void disable()
    {
        _fd.disable();
    }

    template<class F>
    void read_some(F&& f)
    {
        // man says that after reading data_head, rmb should be issued
        auto data_head = metadata->data_head;
        __sync_synchronize();

        while (_data_view.total_read_size() < data_head)
        {
            auto* header = _data_view.read<perf_event_header>();
            auto* sample = _data_view.read<sample_t>();

            if (header->type != PERF_RECORD_SAMPLE)
                throw std::runtime_error("unsupported event type");

            assert(header->size == sizeof(header) + sizeof(sample_t));

            f(*sample);
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
    session.enable();

    while(true)
    {
        session.read_some([](const auto& sample)
        {
            std::cerr << &sample << ", ip: " << sample.ip << '\n';
        });
    }

    session.disable();
}

