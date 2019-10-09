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
    return os << dso.pathname << "+0x" << std::hex << dso.addr;
}

struct maps
{
    dso_info find_dso(std::uintptr_t ip) const
    {
        for (const auto& e : entries)
        {
            // TODO: not sure about this range
            if (ip >= e.start && ip < e.end)
            {
                dso_info dso;
                dso.pathname = e.pathname;
                dso.addr = 0; // TODO
                return dso;
            }
        }

        dso_info dso;
        dso.pathname = "??";
        dso.addr = ip;
        return dso;
    }

    std::string comm;
    std::vector<map_entry_t> entries;
};

maps read_maps(const std::string& path)
{
    maps ret;
    std::ifstream f{path};
    std::string line;
    while (std::getline(f, line))
    {
        map_entry_t e{line};
        if (e.exec())
            ret.entries.push_back(e);
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

std::unordered_map<std::uint32_t, maps> parse_maps()
{
    std::unordered_map<std::uint32_t, maps> ret;
    for (const auto& process_directory : boost::filesystem::directory_iterator{"/proc/"})
    {
        auto directory_name = process_directory.path().filename().string();
        if (is_number(directory_name))
        {
            std::uint32_t pid;
            std::stringstream{directory_name} >> pid;
            auto m = read_maps((process_directory.path() / "maps").string());
            m.comm = read_first_line((process_directory.path() / "comm").string());
            ret.emplace(pid, m);
        }
    }
    std::cerr << "read exec maps from " << ret.size() << " running processes\n";
    return ret;
}
