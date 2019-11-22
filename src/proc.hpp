/**
 * Copyright 2019 Nokia
 *
 * Licensed under the BSD 3 Clause license
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <cctype>

#include <boost/filesystem.hpp>

namespace poor_perf
{

struct kernel_symbols
{
    struct kernel_symbol
    {
        kernel_symbol(const std::string& s)
        {
            std::stringstream{s} >> std::hex >> addr >> mode >> name >> module;
        }

        kernel_symbol()
        {
        }

        std::uintptr_t addr;
        char mode;
        std::string name;
        std::string module = "<kernelmain>";

        bool operator<(const kernel_symbol& other) const
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
        auto less = [](std::uintptr_t a, kernel_symbol b)
        {
            return a < b.addr;
        };

        auto it = std::upper_bound(std::next(_symbols.begin()), _symbols.end(), ip, less);

        if (it == _symbols.end())
        {
            kernel_symbol ksym;
            ksym.addr = ip;
            ksym.module = "<nokernel>";
            ksym.name = "-";
            return ksym;
        }

        return *std::prev(it);
    }

private:
    std::vector<kernel_symbol> _symbols;
};

struct region_t
{
    explicit region_t(const std::string& s)
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

struct symbol_t
{
    std::string comm;
    std::string pathname;
    std::uintptr_t addr;

    // in case of kallsyms, we have the name of the symbol
    std::string name;
};

auto read_maps(const std::string& path)
{
    std::vector<region_t> ret;
    std::ifstream f{path};
    std::string line;
    while (std::getline(f, line))
    {
        region_t e{line};
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
    std::string comm = "??";
    std::vector<region_t> maps;
};

struct running_processes_snapshot
{
    running_processes_snapshot()
    {
        load_processes_map();
    }

    symbol_t find_symbol(std::uint32_t pid, std::uintptr_t ip) const
    {
        if (pid == 0)
        {
            // https://en.wikipedia.org/wiki/Parent_process#Unix-like_systems
            symbol_t ret;
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
            symbol_t ret;
            ret.comm = "<no maps>";
            ret.pathname = "-";
            ret.addr = ip;
            ret.name = "-";
            return ret;
        }

        const auto& proc = proc_it->second;

        symbol_t ret;
        ret.comm = proc.comm;
        ret.name = "-";

        auto region = std::find_if(proc.maps.begin(), proc.maps.end(), [&](const auto& r) { return r.contains(ip); });

        if (region == proc.maps.end())
        {
            // last chance is to get it from kallsyms
            auto s = _kernel_symbols.find(ip);
            ret.pathname = s.module;
            ret.addr = ip;
            ret.name = s.name;
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
                process_info p;
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

} // namespace
