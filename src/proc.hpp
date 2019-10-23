#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <cctype>
#include <boost/filesystem.hpp>

struct kernel_symbols
{
    struct kernel_symbol
    {
        kernel_symbol(const std::string& s)
        {
            std::stringstream{s} >> addr >> mode >> name >> module;
        }

        kernel_symbol()
        {
        }

        std::uintptr_t addr;
        char mode = '?';
        std::string name = "??";
        std::string module = "??";

        bool operator<(const kernel_symbol& other)
        {
            return addr < other.addr;
        }
    };

    kernel_symbols()
    {
        std::ifstream f{"/proc/kallsyms"};
        std::string line;
        while (std::getline(f, line))
            _symbols.emplace_back(line);
        std::sort(_symbols.begin(), _symbols.end());
        std::cerr << "read " << _symbols.size() << " kernel symbols\n";
    }

    kernel_symbol find(std::uintptr_t ip) const
    {
        kernel_symbol ksym;
        ksym.addr = ip;
        ksym.module = "??";
        ksym.name = "??";
        return ksym;
    }

private:
    std::vector<kernel_symbol> _symbols;
};

struct map_entry_t
{
    explicit map_entry_t(const std::string& s)
    {
        std::istringstream ss{s};
        char _;
        std::string _s;
        ss >> std::hex >> start >> _ >> end >> perms >> offset >> _s >> _s >> pathname;
    }

    std::uintptr_t start;
    std::uintptr_t end;
    std::string perms;
    std::uintptr_t offset;
    std::string pathname;

    bool contains(std::uintptr_t ip) const
    {
        // TODO: not sure about this range
        return ip >= start && ip < end;
    }

    bool exec() const
    {
        return perms.at(2) == 'x';
    }
};

struct dso_info
{
    std::string comm;
    std::string pathname;
    std::uintptr_t addr;

    // in case of kallsyms, we have the name of the symbol
    std::string name;
};

inline std::ostream& operator<<(std::ostream& os, const dso_info& dso)
{
    return os << dso.pathname << " 0x" << std::hex << dso.addr << ' ' << dso.name;
}

auto read_maps(const std::string& path)
{
    std::vector<map_entry_t> ret;
    std::ifstream f{path};
    std::string line;
    while (std::getline(f, line))
    {
        map_entry_t e{line};
        if (e.exec())
            ret.push_back(e);
    }
    return ret;
}

bool is_number(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(c); });
}

std::string read_first_line(const std::string& path)
{
    std::ifstream f{path};
    std::string line;
    std::getline(f, line);
    return line;
}

struct process_info
{
    process_info(const kernel_symbols& syms) : _kernel_symbols{syms}
    {
    }

    dso_info find_dso(std::uintptr_t ip) const
    {
        for (const auto& e : maps)
        {
            // TODO: not sure about this range
            if (ip >= e.start && ip < e.end)
            {
                dso_info dso;
                dso.pathname = e.pathname;
                dso.addr = ip - e.start + e.offset;
                return dso;
            }
        }

        // assuming it is a kernel sym
        const auto& ksym = _kernel_symbols.find(ip);

        dso_info dso;
        dso.pathname = ksym.module;
        dso.addr = ip;
        dso.name = ksym.name;
        return dso;
    }

    std::string comm = "??";
    std::vector<map_entry_t> maps;
    const kernel_symbols& _kernel_symbols;
};

struct running_processes_snapshot
{
    running_processes_snapshot()
    {
        load_processes_map();
    }

    process_info find(std::uint32_t pid) const
    {
        auto it = _processes.find(pid);
        if (it != _processes.end())
            return it->second;
        return process_info{_kernel_symbols};
    }

    dso_info find_symbol(std::uint32_t pid, std::uintptr_t ip) const
    {
        if (pid == 0)
        {
            // https://en.wikipedia.org/wiki/Parent_process#Unix-like_systems
            dso_info ret;
            ret.comm = "<swapper>";
            ret.pathname = "-";
            ret.addr = ip;

            // symbol is most likely some arch-dependent hlt instruction so
            // we do not bother about looking for it
            ret.name = "-";
            return ret;
        }

        auto proc_it = _processes.find(pid);

        if (proc_it == _processes.end())
        {
            // this process was probably started after poor-perf
            dso_info ret;
            ret.comm = "<no maps>";
            ret.pathname = "-";
            ret.addr = ip;
            ret.name = "-";
            return ret;
        }

        const auto& proc = proc_it->second;

        dso_info ret;
        ret.comm = proc.comm;
        ret.name = "-";

        auto region = std::find_if(proc.maps.begin(), proc.maps.end(), [&](const auto& r) { return r.contains(ip); });

        if (region == proc.maps.end())
        {
            // TODO: don't know what to do with it
            ret.pathname = "<no region>";
            ret.addr = ip;
            return ret;
        }

        ret.pathname = region->pathname;
        ret.addr = ip - region->start + region->offset;
        return ret;
    }

private:
    void load_processes_map()
    {
        for (const auto& process_directory : boost::filesystem::directory_iterator{"/proc/"})
        {
            auto directory_name = process_directory.path().filename().string();
            if (is_number(directory_name))
            {
                std::uint32_t pid;
                std::stringstream{directory_name} >> pid;
                process_info p{_kernel_symbols};
                p.comm = read_first_line((process_directory.path() / "comm").string());
                p.maps = read_maps((process_directory.path() / "maps").string());
                _processes.emplace(pid, p);
            }
        }
        std::cerr << "took map snapshot of " << _processes.size() << " running processes\n";
    }

    std::unordered_map<std::uint32_t, process_info> _processes;
    kernel_symbols _kernel_symbols;
};
