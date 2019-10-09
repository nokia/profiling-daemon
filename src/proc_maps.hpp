#pragma once

#include <string>
#include <sstream>

// 56373c46a000-56373c46b000 r-xp 00000000 103:02 10628889                  /usr/bin/python2.7
// 56373c66a000-56373c66b000 r--p 00000000 103:02 10628889                  /usr/bin/python2.7
// 56373c66b000-56373c66c000 rw-p 00001000 103:02 10628889                  /usr/bin/python2.7
// 56373c982000-56373d172000 rw-p 00000000 00:00 0                          [heap]
// 7fe9d0fb9000-7fe9d124e000 r-xp 00000000 103:02 10630503                  /usr/lib/python2.7/lib-dynload/unicodedata.so
// 7fe9d124e000-7fe9d124f000 r--p 00095000 103:02 10630503                  /usr/lib/python2.7/lib-dynload/unicodedata.so
// 7fe9d124f000-7fe9d1262000 rw-p 00096000 103:02 10630503                  /usr/lib/python2.7/lib-dynload/unicodedata.so
// 7fe9d1262000-7fe9d12e2000 rw-p 00000000 00:00 0
// ...
// 7fe9d57ae000-7fe9d57af000 rw-p 00089000 103:02 10628867                  /lib/ld-musl-x86_64.so.1
// 7fe9d57af000-7fe9d57b2000 rw-p 00000000 00:00 0
// 7ffdee0e9000-7ffdee10a000 rw-p 00000000 00:00 0                          [stack]
// 7ffdee1e1000-7ffdee1e4000 r--p 00000000 00:00 0                          [vvar]
// 7ffdee1e4000-7ffdee1e5000 r-xp 00000000 00:00 0                          [vdso]
// ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]

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
