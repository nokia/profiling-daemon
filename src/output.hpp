#pragma once

namespace poor_perf
{

void stream_to(std::ostream&)
{
}

template<class Arg, class... Args>
void stream_to(std::ostream& os, Arg&& arg, Args&&... args)
{
    os << arg;
    stream_to(os, std::forward<Args>(args)...);
}

/**
 * Wrapper over fstream that can optionaly hold a reference to cout so you can pass "-" as a filename.
 */
struct output_stream
{
    output_stream(const std::string& path)
    {
        if (path == "-")
            _output = &std::cout;
        else
        {
            _fstream.open(path, std::fstream::out | std::fstream:: app | std::fstream::ate);
            if (!_fstream)
                throw std::runtime_error{"could not open '" + path + "' for writing"};
            _output = &_fstream;
        }
    }

    /**
     * Prints message to both output stream and std::cout.
     */
    template<class... Args>
    void message(Args&&... args)
    {
        message_impl(current_time{}, ": ", std::forward<Args>(args)..., '\n');
    }

    std::ostream& stream()
    {
        return *_output;
    }

private:
    template<class... Args>
    void message_impl(Args&&... args)
    {
        stream_to(stream(), std::forward<Args>(args)...);

        if (!streaming_to_stdout())
            stream_to(std::cout, std::forward<Args>(args)...);
    }

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


} // namespace
