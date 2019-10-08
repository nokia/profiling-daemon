#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>

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
        : _start(start), _size(size)
    {
    }

private:
    void* _start;
    std::size_t _size;
};

int main(int argc, char **argv)
{
    struct perf_event_attr pe;
    long long count;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.sample_freq = 7000;
    pe.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TIME;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.mmap = 1;
    pe.freq = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }

    constexpr std::size_t page_size = 4 * 1024;
    constexpr std::size_t mmap_size = page_size * 2;

    void* buffer = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Cannot memory map sampling buffer\n");
        exit(EXIT_FAILURE);
    }


    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    for (int i = 0; i < 1000; i++)
        printf("Measuring instruction count for this printf\n");

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    //read(fd, &count, sizeof(long long));

    const perf_event_mmap_page* metadata_page = reinterpret_cast<perf_event_mmap_page*>(buffer);
    std::cerr << "metadata_page: " << metadata_page << std::endl;
    std::cerr << "metadata_page->data_head: " << metadata_page->data_head << std::endl;
    std::cerr << "metadata_page->data_offset: " << metadata_page->data_offset<< std::endl;
    std::cerr << "metadata_page->data_size: " << metadata_page->data_size<< std::endl;
    void* data = buffer + metadata_page->data_offset;

    std::cerr << "data: " << data << std::endl;

    // man says that after reading data_head, rmb should be issued
    __sync_synchronize();

    std::size_t read = 0;
    while (read < metadata_page->data_head)
    {
        std::cerr << "reading from: " << data << ' ';
        const perf_event_header* event_header = reinterpret_cast<perf_event_header*>(data);
        //data += sizeof(perf_event_header);
        //read += sizeof(perf_event_header);
        std::cerr << "event type: " << event_header->type << ", size: " << event_header->size << std::endl;
        data += event_header->size;
        read += event_header->size;
    }


    //printf("Used %lld instructions\n", count);

    close(fd);
}

