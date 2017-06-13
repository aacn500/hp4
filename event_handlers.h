#ifndef HP4_EVENT_HANDLERS_H
#define HP4_EVENT_HANDLERS_H

#include <event2/event.h>

#include "parser.h"

struct event_args {
    struct pipe_array *in_pipes;
    struct pipe *out_pipe;
    ssize_t **bytes_spliced;
};

struct sigchld_args {
    struct p4_file *pf;
    struct event_base *eb;
    int n_children_exited;
};

struct stats_ev_args {
    struct p4_file *pf;
};

void sigint_handler(evutil_socket_t fd, short what, void *arg);

void sigchld_handler(evutil_socket_t fd, short what, void *arg);

void readableCb(evutil_socket_t fd, short what, void *arg);

void statsCb(evutil_socket_t fd, short what, void *arg);

int open_dev_null(void);

void close_dev_null(void);

#endif /* HP4_EVENT_HANDLERS_H */
