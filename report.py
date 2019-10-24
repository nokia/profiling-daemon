#!/usr/bin/env python3
import sys


def parse_file(path):
    with open(path) as f:
        for line in f:
            tokens = line.rstrip().split(';')
            print(tokens)


def show_top(path):
    parse_file(path)


def _main():
    for path in sys.argv[1:]:
        show_top(path)


if __name__ == "__main__":
    _main()
