#!/usr/bin/env python3

import json
import os
import sys

import pexpect
import pytest

import file_gen

script_path = os.path.realpath(__file__)
script_dir = os.path.dirname(script_path)


def test_smallfile():
    child = pexpect.spawn(script_dir + "/../src/hp4 -f " +
                          script_dir + "/data/smallfile.json")
    out = []
    for line in child:
        out.append(json.loads(line.decode()))

    assert out[-1]["cat-to-sed"] == 50
    assert out[-1]["sed-to-save"] == 50

    with open(script_dir + "/data/smallfile_A.txt", 'r') as f:
        for line in f:
            # All 'a's have been capitalised to 'A'.
            assert "Lorem ipsum dolor sit Amet, consectetur volutpAt." in line

    os.remove(script_dir + "/data/smallfile_A.txt")


def test_largefile():
    child = pexpect.spawn(script_dir + "/../src/hp4 -f " +
                          script_dir + "/data/largefile.json")
    out = []
    for line in child:
        out.append(json.loads(line.decode()))

    assert out[-1]["cat-to-sed"] == 524288000
    assert out[-1]["sed-to-save"] == 524288000

    with open(script_dir + "/data/largefile_A.txt", 'r') as f:
        for line in f:
            # All 'a's have been capitalised to 'A'.
            assert "Lorem ipsum dolor sit Amet, consectetur volutpAt." in line

    os.remove(script_dir + "/data/largefile_A.txt")


def test_split_data_large():
    child = pexpect.spawn(script_dir + "/../src/hp4 -f " +
                          script_dir + "/data/split_data_large.json")

    out = []
    for line in child:
        out.append(json.loads(line.decode()))

    assert out[-1]["cat-to-seda"] == 524288000
    assert out[-1]["seda-to-savea"] == 524288000
    assert out[-1]["cat-to-sedt"] == 524288000
    assert out[-1]["sedt-to-savet"] == 524288000

    with open(script_dir + "/data/split_data_large_A.txt", 'r') as f:
        for line in f:
            # All 'a's have been capitalised to 'A'.
            assert "Lorem ipsum dolor sit Amet, consectetur volutpAt." in line

    with open(script_dir + "/data/split_data_large_T.txt", 'r') as f:
        for line in f:
            # All 't's have been capitalised to 'T'.
            assert "Lorem ipsum dolor siT ameT, consecTeTur voluTpaT." in line

    os.remove(script_dir + "/data/split_data_large_A.txt")
    os.remove(script_dir + "/data/split_data_large_T.txt")


if __name__ == "__main__":
    file_gen.generate_largefile(script_dir + "/data/")
    file_gen.generate_testfile(script_dir + "/data/smallfile.txt", 51)

    i = pytest.main(["-v", script_path])

    os.remove(script_dir + "/data/largefile.txt")
    os.remove(script_dir + "/data/smallfile.txt")

    sys.exit(i)
