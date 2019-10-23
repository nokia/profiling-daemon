#pragma once

namespace poor_perf
{

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


} // namespace
