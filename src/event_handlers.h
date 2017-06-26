#ifndef HP4_EVENT_HANDLERS_H
#define HP4_EVENT_HANDLERS_H

#include <event2/event.h>

#include "parser.h"

struct event_array {
    struct event **events;
    size_t length;
};

struct writable_ev_args {
    struct pipe *from_pipe;
    struct pipe_array *to_pipes;
    ssize_t **bytes_spliced;

    size_t *bytes_safely_written;

    int to_pipe_idx;

    struct event *readable_event;
};

struct readable_ev_args {
    struct event_array *writable_events;
    struct pipe *from_pipe;
    struct pipe_array *to_pipes;

    size_t *bytes_safely_written;
};

struct sigchld_args {
    struct p4_file *pf;
    struct event_base *eb;
    int n_children_exited;
};

struct stats_ev_args {
    struct p4_file *pf;
};

struct event_array *event_array_new(void);

int event_array_append(struct event_array *ev_arr, struct event *ev);

void event_array_free(struct event_array *ev_arr);

void sigint_handler(evutil_socket_t fd, short what, void *arg);

void sigchld_handler(evutil_socket_t fd, short what, void *arg);

void writable_handler(evutil_socket_t fd, short what, void *arg);

void readable_handler(evutil_socket_t fd, short what, void *arg);

void stats_handler(evutil_socket_t fd, short what, void *arg);

int open_dev_null(void);

void close_dev_null(void);

#endif /* HP4_EVENT_HANDLERS_H */
