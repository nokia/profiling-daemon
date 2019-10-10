#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <cctype>
#include <boost/filesystem.hpp>

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

    bool exec() const
    {
        return perms.at(2) == 'x';
    }
};

struct dso_info
{
    std::string pathname;
    std::uintptr_t addr;
};

inline std::ostream& operator<<(std::ostream& os, const dso_info& dso)
{
    return os << dso.pathname << " 0x" << std::hex << dso.addr;
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

        dso_info dso;
        dso.pathname = "??";
        dso.addr = ip;
        return dso;
    }

    std::string comm = "??";
    std::vector<map_entry_t> maps;
};

struct running_processes_snapshot
{
    running_processes_snapshot()
    {
        load_exec_maps();
    }

    process_info find(std::uint32_t pid) const
    {
        auto it = _processes.find(pid);
        if (it != _processes.end())
        {
            return it->second;
        }

        return process_info{};
    }

private:
    void load_exec_maps()
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
};
