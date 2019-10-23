#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cassert>
#include <signal.h>

#include <boost/program_options.hpp>

#include "perf.hpp"
#include "fifo.hpp"
#include "proc.hpp"
#include "event_loop.hpp"
#include "utils.hpp"

namespace poor_perf
{

const char* CONTROL_FIFO_PATH = "/run/poor-profiler";

volatile sig_atomic_t signal_status = 0;

void signal_handler(int signal)
{
    signal_status = signal;
}

void stream_to(std::ostream& os)
{
}

template<class Arg, class... Args>
void stream_to(std::ostream& os, Arg&& arg, Args&&... args)
{
    os << arg;
    stream_to(os, std::forward<Args>(args)...);
}

/**
 * Wrapper over fstream that can optionaly hold a reference to cout.
 */
struct output_stream
{
    output_stream(const std::string& output)
    {
        if (output == "-")
            _output = &std::cout;
        else
        {
            _fstream.open(output, std::fstream::out | std::fstream:: app | std::fstream::ate);
            if (!_fstream)
                throw std::runtime_error{"could not open '" + output + "' for writing"};
            _output = &_fstream;
        }
    }

    /**
     * Prints message to both output stream and std::cerr.
     */
    template<class... Args>
    void message(Args&&... args)
    {
        stream_to(stream(), current_time{}, ": ", std::forward<Args>(args)..., '\n');

        if (!streaming_to_stdout())
            stream_to(std::cerr, current_time{}, ": ", std::forward<Args>(args)..., '\n');
    }

    std::ostream& stream()
    {
        return *_output;
    }

private:
    bool streaming_to_stdout() const
    {
        return _output == &std::cout;
    }

    std::ostream* _output;
    std::fstream _fstream;
};

template<class T>
output_stream& operator<<(output_stream& os, T&& t)
{
    os.stream() << t;
    return os;
}

void profile_for(const std::string& output, const running_processes_snapshot& processes, std::chrono::seconds secs)
{
    std::cerr << "starting profile\n";

    event_loop loop{signal_status};
    perf_session session{0};
    loop.add_fd(session.fd());

    output_stream out{output};

    loop.run_for(secs, [&]
    {
        session.read_some([&](const auto& sample)
        {
            auto p = processes.find(sample.pid);
            out << std::dec << sample.pid << ' ' << p.comm << ' ' << p.find_dso(sample.ip) << '\n';
        });

        out.stream().flush();
    });

    out << "done\n\n";
}

enum class trigger
{
    none,
    control_fifo,
    watchdog
};

std::ostream& operator<<(std::ostream& os, trigger trigger)
{
    switch (trigger)
    {
        case trigger::none: return os << "none";
        case trigger::control_fifo: return os << "control fifo";
        case trigger::watchdog: return os << "watchdog";
        default: return os << "<unknown>";
    }
    return os;
}

auto wait_for_trigger(watchdog& wdg)
{
    event_loop loop{signal_status};

    fifo control_fifo{CONTROL_FIFO_PATH};
    loop.add_fd(control_fifo.fd());

    std::cerr << "control fifo created at " << CONTROL_FIFO_PATH << '\n';
    std::cerr << "waiting for trigger\n";

    auto trigger = trigger::none;
    while (trigger == trigger::none && !signal_status)
    {
        auto read_control_fifo = [&]
        {
            std::cerr << "woke up by control fifo\n";
            std::cerr << control_fifo.read();
            loop.stop();
            trigger = trigger::control_fifo;
        };

        auto timeout = [&]
        {
            std::cerr << "watchdog ping\n";
            if (!wdg.ping())
            {
                std::cerr << "watchdog is starved\n";
                loop.stop();
                trigger = trigger::watchdog;
            }
        };

        loop.run_for(std::chrono::seconds{2}, read_control_fifo, timeout);
    }

    return trigger;
}

enum class mode_t
{
    watchdog,
    oneshot
};

std::istream& operator>>(std::istream& is, mode_t& mode)
{
    std::string s;
    is >> s;

    if (s == "watchdog")
        mode = mode_t::watchdog;
    else if (s == "oneshot")
        mode = mode_t::oneshot;
    else
        is.setstate(std::ios_base::failbit);

    return is;
}

std::ostream& operator<<(std::ostream& os, const mode_t& mode)
{
    return os << "mode";
}

auto parse_options(int argc, char **argv)
{
    namespace po = boost::program_options;

    po::options_description desc;
    desc.add_options()
        ("output", po::value<std::string>()->default_value("/rom/profile.txt"))
        ("mode", po::value<mode_t>()->default_value(mode_t::watchdog));

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    return vm;
}

void watchdog_mode(const boost::program_options::variables_map& options)
{
    running_processes_snapshot proc;
    watchdog wdg;

    const auto output = options["output"].as<std::string>();

    {
        output_stream f{output};
        f.message("watchdog mode started");
    }

    // childs inherit sched so set it after watchdog is started
    set_this_thread_into_realtime();

    while (!signal_status)
    {
        auto t = wait_for_trigger(wdg);

        if (t != trigger::none)
        {
            {
                output_stream f{output};
                f.message("woke up by ", t);
            }

            profile_for(output, proc, std::chrono::seconds(3));
        }
    }
}

void oneshot_mode(const boost::program_options::variables_map& options)
{
    running_processes_snapshot proc;
    set_this_thread_into_realtime();
    const auto output = options["output"].as<std::string>();

    {
        output_stream f{output};
        f.message("oneshot profiling");
    }

    profile_for(output, proc, std::chrono::seconds(3));
}

} // namespace

int main(int argc, char **argv)
{
    const auto options = poor_perf::parse_options(argc, argv);

    ::set_this_thread_name("poor-perf");
    ::set_this_thread_affinity(1);
    ::signal(SIGINT, poor_perf::signal_handler);
    ::signal(SIGTERM, poor_perf::signal_handler);

    switch (options["mode"].as<poor_perf::mode_t>())
    {
    case poor_perf::mode_t::watchdog:
        poor_perf::watchdog_mode(options);
        break;
    case poor_perf::mode_t::oneshot:
        poor_perf::oneshot_mode(options);
        break;
    }
}

