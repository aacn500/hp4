#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <event2/event.h>

#include "event_handlers.h"
#include "debug.h"
#include "parser.h"
#include "pipe.h"
#include "stats.h"

#ifndef MAX_BYTES_TO_SPLICE
#define MAX_BYTES_TO_SPLICE 65536
#endif /* MAX_BYTES_TO_SPLICE */

int fd_dev_null = -1;

int open_dev_null(void) {
    fd_dev_null = open("/dev/null", O_WRONLY|O_NONBLOCK);
    return fd_dev_null;
}

void close_dev_null(void) {
    if (fd_dev_null >= 0) {
        // ignore return value as we do not need to ensure writes
        // to /dev/null are successful
        close(fd_dev_null);
        fd_dev_null = -1;
    }
}

struct event_array *event_array_new(void) {
    struct event_array *ev_arr = malloc(sizeof(*ev_arr));
    if (ev_arr == NULL) {
        return NULL;
    }
    ev_arr->length = 0u;
    ev_arr->events = NULL;
    return ev_arr;
}

int event_array_append(struct event_array *ev_arr, struct event *ev) {
    if (++ev_arr->length == 1u) {
        ev_arr->events = malloc(sizeof(*ev_arr->events));
        if (ev_arr->events == NULL) {
            return -1;
        }
    }
    else {
        struct event **realloced_events = realloc(ev_arr->events,
                ev_arr->length * sizeof(*ev_arr->events));
        if (realloced_events == NULL) {
            return -1;
        }
        ev_arr->events = realloced_events;
    }
    ev_arr->events[ev_arr->length - 1] = ev;
    return 0;
}

void event_array_free(struct event_array *ev_arr) {
    for (int i = 0; i < (int)ev_arr->length; i++) {
        event_free(ev_arr->events[i]);
    }
}

void sigint_handler(evutil_socket_t fd, short what, void *arg) {
    PRINT_DEBUG("\b\bHandling sigint...\n");
    struct event_base *eb = arg;
    event_base_loopbreak(eb);
}

void sigchld_handler(evutil_socket_t fd, short what, void *arg) {
    PRINT_DEBUG("killing child...\n");
    struct sigchld_args *sa = arg;
    /* Handler is only added back onto event queue after returning. If another
     * process exits while handler is running, handler will not be called.
     * So we loop until error (p == -1) or no processes have terminated (p == 0)
     */
    while (1) {
        int status;
        pid_t p = waitpid(-1, &status, WNOHANG);
        if (p == -1) {
            if (errno == ECHILD) {
                PRINT_DEBUG("Waited for a process to terminate, but all "
                            "child processes have already terminated.\n");
            } else {
                PRINT_DEBUG("Got an unexpected error while waiting for "
                            "child to terminate: %s\n", strerror(errno));
            }
            break;
        }
        else if (p == 0) {
            PRINT_DEBUG("Waited for a process to terminate, but none are "
                        "finished. Exiting event handler...\n");
            break;
        }
        else if (WIFEXITED(status) || (WIFSIGNALED(status) && WTERMSIG(status) == 13)) {
            ++sa->n_children_exited;
            PRINT_DEBUG("%dth child process ended\n", sa->n_children_exited);
            struct p4_node *pn = find_node_by_pid(sa->pf, p);
            if (pn == NULL) {
                PRINT_DEBUG("No node found with pid %u\n", p);
                return;
            }
            if (pn->in_pipes && pipe_array_close(pn->in_pipes) < 0) {
                PRINT_DEBUG("Closing all incoming pipes to node %s failed: %s\n",
                        pn->id, strerror(errno));
            }
            pn->ended = 1;

            if (pn->out_pipes) {
                for (int i = 0; i < (int)pn->out_pipes->length; i++) {
                    if (close(pn->out_pipes->pipes[i]->write_fd) < 0) {
                        PRINT_DEBUG("Closing outgoing pipe from node %s on edge %s failed: %s\n",
                                pn->id, pn->out_pipes->pipes[i]->edge_id, strerror(errno));
                    }
                }
            }

            for (int j = 0; j < (int)sa->pf->edges->length; j++) {
                if (strcmp(sa->pf->edges->edges[j]->to, pn->id) == 0) {
                    PRINT_DEBUG("edge %s finished after splicing %ld bytes\n",
                           sa->pf->edges->edges[j]->id,
                           sa->pf->edges->edges[j]->bytes_spliced);
                }
            }

            if (sa->n_children_exited == (int)sa->pf->nodes->length) {
                event_base_loopexit(sa->eb, NULL);
            }
        }
        else if (WIFSIGNALED(status)) {
            PRINT_DEBUG("child was signaled by %d\n", WTERMSIG(status));
            break;
        }
        else {
            PRINT_DEBUG("process did not exit normally and was not terminated "
                        "by a signal.\n");
            break;
        }
    }
}

void write_single(struct writable_ev_args *wea) {
    struct pipe *to_pipe = wea->to_pipes->pipes[0];
    ssize_t bytes = splice(wea->from_pipe->read_fd,
                           NULL,
                           to_pipe->write_fd,
                           NULL,
                           MAX_BYTES_TO_SPLICE,
                           SPLICE_F_NONBLOCK);
    if (bytes < 0) {
        *wea->got_eof = 0;
        if (errno != EAGAIN) {
            PRINT_DEBUG("Failed to splice data: %s\n", strerror(errno));
            return;
        }
    }
    else if (bytes > 0) {
        *wea->got_eof = 0;
        **wea->bytes_spliced += bytes;
        to_pipe->bytes_written = (size_t)bytes;
    }

}

void write_multiple(struct writable_ev_args *wea) {
    int i = wea->to_pipe_idx;
    struct pipe *to_pipe = wea->to_pipes->pipes[i];
    if (to_pipe->bytes_written == 0) {
        ssize_t bytes = tee(wea->from_pipe->read_fd,
                            to_pipe->write_fd,
                            MAX_BYTES_TO_SPLICE,
                            SPLICE_F_NONBLOCK);
        if (bytes < 0) {
            *wea->got_eof = 0;
            if (errno != EAGAIN) {
                PRINT_DEBUG("Failed to tee data: %s\n", strerror(errno));
                return;
            }
        }
        else if (bytes > 0) {
            *wea->got_eof = 0;
            to_pipe->bytes_written = (size_t)bytes;
            *wea->bytes_spliced[i] += bytes;
        }
    }
    if (wea->to_pipes->pipes[i]->bytes_written < *wea->lowest_bytes_written) {
        *wea->lowest_bytes_written = wea->to_pipes->pipes[i]->bytes_written;
    }
    to_pipe->visited = 1;
}

void writableCb(evutil_socket_t fd, short what, void *arg) {
    struct writable_ev_args *wea = arg;
    if ((what & EV_WRITE) == 0) {
        return;
    }
    if (wea->to_pipes->length == 1u) {
        write_single(wea);
    }
    else {
        // tee/splice algorithm based on answer in
        // https://stackoverflow.com/a/14200975
        write_multiple(wea);

        for (int i = 0; i < (int)wea->to_pipes->length; i++) {
            if (wea->to_pipes->pipes[i]->visited == 0) {
                /* Not all pipes' writable events have fired; do not yet
                 * splice to /dev/null or add readable event. */
                return;
            }
        }
        /* All pipes' writable events have fired; splice to /dev/null all data
         * which has been read to every pipe. */
        ssize_t bytes = splice(wea->from_pipe->read_fd,
                               NULL,
                               fd_dev_null,
                               NULL,
                               *wea->lowest_bytes_written,
                               SPLICE_F_NONBLOCK);
        if (bytes < 0) {
            *wea->got_eof = 0;
            return;
        }
        if (bytes > 0) {
            for (int j = 0; j < (int)wea->to_pipes->length; j++) {
                wea->to_pipes->pipes[j]->bytes_written -= bytes;
            }
            *wea->got_eof = 0;
        }
    }
    if (*wea->got_eof == 1) {
        PRINT_DEBUG("Edge %s at EOF; closing pipes...\n", wea->from_pipe->edge_id);
        close(wea->from_pipe->read_fd);
        for (int k = 0; k < (int)wea->to_pipes->length; k++) {
            close(wea->to_pipes->pipes[k]->write_fd);
        }
    }
    else {
        event_add(wea->readable_event, NULL);
    }
}

void readableCb(evutil_socket_t fd, short what, void *arg) {
    struct readable_ev_args *rea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }

    *rea->lowest_bytes_written = SIZE_MAX;
    *rea->got_eof = 1;
    for (int i = 0; i < (int)rea->to_pipes->length; i++) {
        rea->to_pipes->pipes[i]->visited = 0;
    }
    for (int j = 0; j < (int)rea->writable_events->length; j++) {
        event_add(rea->writable_events->events[j], NULL);
    }
}

void statsCb(evutil_socket_t fd, short what, void *arg) {
    struct stats_ev_args *sa = arg;
    struct p4_file *pf = sa->pf;
    create_stats_file(pf);
}
