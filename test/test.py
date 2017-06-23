#!/usr/bin/env python3

import os
import sys

import pexpect
import pytest

from largefile_gen import generate_largefile

def test_largefile():
    script_dir = os.path.dirname(os.path.realpath(__file__))
    generate_largefile(script_dir + '/data/')
    child = pexpect.spawn(script_dir + "/../src/hp4 -f " + \
                          script_dir + "/data/largefile.json");
    out = ""
    for line in child:
        out += line.decode()

    assert '"cat-to-sed": 524288000' in out
    assert '"sed-to-save": 524288000' in out

    with open('d', 'r') as f:
        for line in f:
            assert "Lorem ipsum dolor sit Amet, consectetur volutpAt." in line

if __name__ == "__main__":
    i = pytest.main(['-v', os.path.realpath(__file__)])
    sys.exit(i)
