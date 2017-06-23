#!/usr/bin/env python3

import os

LOREM = "Lorem ipsum dolor sit amet, consectetur volutpat.\n"
LENGTH = 524288000

def generate_largefile(into):
    with open(into + '/largefile', 'w') as f:
        for i in range(LENGTH // len(LOREM)):
            f.write(LOREM)

if __name__ == "__main__":
    generate_largefile()
