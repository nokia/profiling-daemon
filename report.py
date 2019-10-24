#!/usr/bin/env python3
import sys


def _main():
    for path in sys.argv[1:]:
        show_top(path)


if __name__ == "__main__":
    _main()
