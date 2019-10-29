#!/usr/bin/env python3
import os
import sys
import operator
from collections import defaultdict, Counter
from typing import NamedTuple
from copy import copy
import subprocess
from functools import lru_cache


class Sample:
    def __init__(self, line):
        tokens = line.rstrip().split(';')
        self.time, self.pid, self.comm, self.dso, self.addr, self.sym = tokens

    def __str__(self):
        return f'{self.time} {self.pid} {self.comm} {self.dso} {self.addr} {self.sym}'


def parse_file(path):
    with open(path) as f:
        for line in f:
            if line and line[0] != '#':
                yield Sample(line)


@lru_cache(1024)
def _read_symbol_name(dso, addr):
    out = subprocess.check_output(f'addr2line -fe {dso} {addr}', shell=True)
    name = out.decode('utf-8').split('\n')[0]
    return name


def _group(rng, max_size):
    """Split range into chunks no longer than max_size."""
    for i in range(0, len(rng), max_size):
            yield rng[i:i+max_size]


def _read_symbol_names(dso, addrs):
    for part in _group(list(addrs), 100):
        out = subprocess.check_output(f'addr2line -fe {dso} {" ".join(part)}', shell=True)
        for addr, name in zip(addrs, out.decode('utf-8').split('\n')[::2]):
            yield addr, name


def read_symbols(samples):
    ret = list(samples)

    addrs = defaultdict(set)

    for s in ret:
        if s.sym == '-' and os.path.exists(s.dso):
            addrs[s.dso].add(s.addr)

    names = defaultdict(dict)

    for dso, a in addrs.items():
        names[dso] = dict(_read_symbol_names(dso, a))

    for s in ret:
        c = copy(s)
        if c.sym == '-' and os.path.exists(c.dso):
            c.sym = names[c.dso][c.addr]
        yield c


def show_top(path):
    # 25733629246420;0;<swapper>;-;0xffffffffbe3dd34a;-
    # 25733636247831;0;<swapper>;-;0xffffffffbe3d916d;-
    # 25733636525490;2375;pulseaudio;<kernelmain>;0xffffffffbdcd3888;update_blocked_averages

    class TopSample:
        def __init__(self, sample=None):
            self.sample = sample

        def __eq__(self, other):
            return self.sample.pid, self.sample.addr == other.sample.pid, other.sample.addr

        def __hash__(self):
            return hash(self._tuple)

        @property
        def _tuple(self):
            return self.sample.pid, self.sample.addr

    samples = parse_file(path)
    samples = read_symbols(samples)

    c = Counter([TopSample(s) for s in samples])
    for s, n in c.most_common(50):
        print(f'{n} {s.sample.comm} {s.sample.dso} {s.sample.addr} {s.sample.sym}')


def show(path):
    samples = parse_file(path)
    for s in read_symbols(samples):
        print(f'{s.pid} {s.comm} {s.dso} {s.addr} {s.sym}')


def _main():
    for path in sys.argv[1:]:
        show(path)


if __name__ == "__main__":
    _main()
