#!/usr/bin/env python3
import os
import sys
import operator
from collections import defaultdict, Counter
from typing import NamedTuple
from copy import copy
import subprocess
from itertools import groupby
from operator import attrgetter
import fileinput


class Sample:
    def __init__(self, line, *, fmt):
        tokens = line.rstrip().split(';')
        self._data = dict(zip(fmt, tokens))

    def __str__(self):
        return f'{self.time} {self.pid} {self.comm} {self.pathname} {self.addr} {self.name}'

    def __getattr__(self, name):
        try:
            return self._data[name]
        except KeyError:
            raise KeyError(f'there is no \'{name}\' in: {",".join(self._data.keys())}')


def parse_file(f):
    fmt = None
    for line in f:
        if not line or line[0] == '#':
            continue

        if line[0] == '$':
            fmt = line[1:].strip().split(';')
        elif fmt is None:
            raise RuntimeError('no format line found in profile file')
        else:
            yield Sample(line, fmt=fmt)


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

    dso = attrgetter('pathname')

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
    '''Updates `sym` attribute.

    Returns new iterable of `Sample` but with `sym` changed
    to the symbol name got from `addr2line`.'''

    ret = list(samples)

    mapping = _build_symbol_mapping(ret)

    for s in ret:
        try:
            s.sym = mapping[s.dso][s.addr]
        except KeyError:
            # that is ok, some symbols come from the kernel instead of dso
            pass

    return ret


def _top(f):

    class TopSample:
        def __init__(self, sample=None):
            self.sample = sample

        def __eq__(self, other):
            return self._tuple == other._tuple

        def __hash__(self):
            return hash(self._tuple)

        @property
        def _tuple(self):
            return self.sample.pid, self.sample.name

    samples = parse_file(f)
    samples = read_symbols(samples)

    duration = int(samples[-1].time) - int(samples[0].time)
    print(f'duration: {duration/1000000000} secs')

    c = Counter([TopSample(s) for s in samples])
    for s, n in c.most_common(50):
        print(f'{n} {s.sample.comm} {s.sample.pathname} {s.sample.addr} {s.sample.name}')


def _show(f):
    samples = parse_file(f)
    samples = read_symbols(samples)
    for s in samples:
        print(f'{s.pid} {s.comm} {s.pathname} {s.addr} {s.name}')


def _main():
    commands = {'top': _top, 'show': _show}

    try:
        command = commands[sys.argv[1]]
    except KeyError:
        sys.exit(f'unkonwn or no command was given, valid ones are: {", ".join(commands.keys())}')

    command(fileinput.input(files=sys.argv[2:]))


if __name__ == "__main__":
    _main()
