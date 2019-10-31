#!/usr/bin/env python3
import os
import sys
import operator
from collections import defaultdict, Counter
from typing import NamedTuple
from copy import copy
import subprocess
from functools import lru_cache
from itertools import groupby
from operator import attrgetter


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


def _group(rng, max_size):
    """Split range into chunks no longer than max_size."""
    for i in range(0, len(rng), max_size):
            yield rng[i:i+max_size]


def test_group():
    assert [[1], [2], [3]] == list(_group([1, 2, 3], 1))
    assert [[1, 2], [3]] == list(_group([1, 2, 3], 2))
    assert [[1, 2], [3, 4]] == list(_group([1, 2, 3, 4], 2))
    assert [[1, 2, 3], [4]] == list(_group([1, 2, 3, 4], 3))


def _build_symbol_mapping(samples):
    '''Builds a nested of dso -> addr -> symbol.

    We are doing it in a bulk because invoking addr2line for just
    one address is very slow.'''

    dso = attrgetter('dso')

    def _read_symbol_names(dso, addrs):
        for part in _group(list(addrs), 200):
            out = subprocess.check_output(f'addr2line -Cfe {dso} {" ".join(part)}', shell=True)
            for addr, name in zip(part, out.decode('utf-8').split('\n')[::2]):
                yield addr, name

    def _read_symbol_mapping_per_dso():
        '''This should return something like: DSO, ((ADDR, NAME), (ADDR2, NAME2))'''
        for k, g in groupby(sorted(samples, key=dso), dso):
            if os.path.exists(k):
                addrs = [s.addr for s in g]
                yield k, _read_symbol_names(k, addrs)

    return {dso: dict(syms) for dso, syms in _read_symbol_mapping_per_dso()}


def read_symbols(samples):
    ret = list(samples)

    mapping = _build_symbol_mapping(ret)

    for s in ret:
        try:
            s.sym = mapping[s.dso][s.addr]
        except KeyError:
            # that is ok, some symbols come from the kernel instead of dso
            pass

    return ret


def show_top(path):
    # 25733629246420;0;<swapper>;-;0xffffffffbe3dd34a;-
    # 25733636247831;0;<swapper>;-;0xffffffffbe3d916d;-
    # 25733636525490;2375;pulseaudio;<kernelmain>;0xffffffffbdcd3888;update_blocked_averages

    class TopSample:
        def __init__(self, sample=None):
            self.sample = sample

        def __eq__(self, other):
            return self._tuple == other._tuple

        def __hash__(self):
            return hash(self._tuple)

        @property
        def _tuple(self):
            return self.sample.pid, self.sample.sym

    samples = parse_file(path)
    samples = read_symbols(samples)

    c = Counter([TopSample(s) for s in samples])
    for s, n in c.most_common(50):
        print(f'{n} {s.sample.comm} {s.sample.dso} {s.sample.addr} {s.sample.sym}')


def show(path):
    samples = parse_file(path)
    samples = read_symbols(samples)
    for s in samples:
        print(f'{s.pid} {s.comm} {s.dso} {s.addr} {s.sym}')


def _main():
    for path in sys.argv[1:]:
        show_top(path)


if __name__ == "__main__":
    _main()
