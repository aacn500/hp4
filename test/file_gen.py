import os

LOREM = "Lorem ipsum dolor sit amet, consectetur volutpat.\n"
LENGTH = 524288000


def generate_largefile(into):
    generate_testfile(into + '/largefile.txt', LENGTH)


def generate_testfile(fname, l):
    with open(fname, 'w') as f:
        for i in range(l // len(LOREM)):
            f.write(LOREM)
