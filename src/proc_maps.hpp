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

using maps_t = std::vector<map_entry_t>;

maps_t parse_file(const std::string& path)
{
    std::cerr << path << '\n';
    maps_t ret;
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

std::unordered_map<std::uint32_t, maps_t> parse_maps()
{
    std::unordered_map<std::uint32_t, maps_t> ret;
    for (const auto& process_directory : boost::filesystem::directory_iterator{"/proc/"})
    {
        auto directory_name = process_directory.path().filename().string();
        if (is_number(directory_name))
        {
            std::uint32_t pid;
            std::stringstream{directory_name} >> pid;
            auto maps = parse_file((process_directory.path() / "maps").string());
            ret.emplace(pid, maps);
            std::cerr << maps.size() << '\n';
        }
    }
    std::cerr << "read exec maps from " << ret.size() << " running processes\n";
    return ret;
}
