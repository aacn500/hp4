#!/bin/bash

set -e -u -x

sudo apt-get -qqy update
sudo apt-get -qqy install texinfo libsubunit-dev

# libjansson
wget -q https://github.com/akheron/jansson/archive/v${JANSSON_VERSION}.tar.gz -O /tmp/jansson-${JANSSON_VERSION}.tar.gz
tar xfz /tmp/jansson-${JANSSON_VERSION}.tar.gz -C /tmp
pushd /tmp/jansson-${JANSSON_VERSION}
autoreconf -fi
./configure
make
sudo make install
popd

# libevent
wget -q https://github.com/libevent/libevent/archive/release-${EVENT_VERSION}-stable.tar.gz -O /tmp/event-${EVENT_VERSION}.tar.gz
tar xfz /tmp/event-${EVENT_VERSION}.tar.gz -C /tmp
pushd /tmp/libevent-release-${EVENT_VERSION}-stable
./autogen.sh
./configure
make
sudo make install
popd

# libcheck
wget -q https://github.com/libcheck/check/archive/${CHECK_VERSION}.tar.gz -O /tmp/check-${CHECK_VERSION}.tar.gz
tar xfz /tmp/check-${CHECK_VERSION}.tar.gz -C /tmp
pushd /tmp/check-${CHECK_VERSION}
autoreconf -i
./configure
make
sudo make install

sudo ldconfig
