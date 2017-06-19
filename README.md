# HP4

## Requirements

hp4 makes heavy use of the splice(2) and tee(2) system calls, which are both only available on Linux kernels >= 2.6.17.

hp4 requires the following libraries:
 * [libjansson](https://github.com/akheron/jansson)
 * [libevent](http://libevent.org)

To run tests, it additionally requires
 * [libcheck](https://github.com/libcheck/check)

Confirmed compatible with libjansson 2.7, libevent 2.0.21 and libcheck 0.10.0.
These versions are available in xenial(ubuntu 16.04)'s apt repository

 `# apt-get install libevent-dev libjansson-dev check`

## Installation

```bash
autoreconf -fi
./configure
make
make check
sudo make install
```

## Description

hp4 reads in a json description of a pipeline graph in a similar format to p4.
This json file contains two arrays; nodes and edges.
Currently, hp4 accepts only EXEC nodes, although the structs and functions in parser.c will parse all node types currently used in p4.

An EXEC node describes a process.

An edge describes the flow of data from one process to another.
By default, data will flow from one process' stdout to another's stdin.
Data can be tee'd by creating multiple edges from the same node.
hp4 can create pipes to send data without using stdout and/or stdin by specifying a port on the edge.
**All** instances of a port will use the same temporary file.
Setting a port to `-` will use stdin/stdout.
See the below example.

```json
{
    "nodes": [
        {
            "id": "wget",
            "type": "EXEC",
            "cmd": "wget -o WGET_OUT http://example.com"
        },
        ...
    ],
    "edges": [
        {
            "id": "read-example",
            "from": "wget:WGET_OUT",
            "to": "parser"
        },
        ...
    ]
}
```

## TODO

 * If data is being tee'd to multiple edges, the destination nodes must currently read at the same speed, otherwise the faster will be blocked by the slower.
 * DONE ~~Need to output stats about data flow at regular intervals.~~
