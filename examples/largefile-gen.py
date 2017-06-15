#!/usr/bin/env python3

import os

LOREM = "Lorem ipsum dolor sit amet, consectetur volutpat.\n"
LENGTH = 524288000

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    with open(script_dir + '/largefile', 'w') as f:
        for i in range(LENGTH // len(LOREM)):
            f.write(LOREM)

if __name__ == "__main__":
    main()
