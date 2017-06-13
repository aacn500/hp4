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

void readableCb(evutil_socket_t fd, short what, void *arg) {
    struct event_args *ea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }
    size_t lowest_bytes_written = INT_MAX;
    int got_eof = 1;
    ssize_t bytes;
    if (ea->in_pipes->length == 1u) {
        bytes = splice(ea->out_pipe->read_fd,
                       NULL,
                       ea->in_pipes->pipes[0]->write_fd,
                       NULL,
                       4096,
                       SPLICE_F_NONBLOCK);
        if (bytes < 0) {
            got_eof = 0;
            if (errno != EAGAIN) {
                PRINT_DEBUG("Failed to splice data: %s\n", strerror(errno));
                return;
            }
        }
        else if (bytes > 0) {
            got_eof = 0;
            **ea->bytes_spliced += bytes;
            (*ea->in_pipes->pipes)->bytes_written = (size_t)bytes;
        }
    }
    else {
        for (int i = 0; i < (int)ea->in_pipes->length; i++) {
            // tee/splice algorithm based on answer in
            // https://stackoverflow.com/a/14200975
            // FIXME This will hang if processes do not read at same rate...
            // both processes will need to be within 4KB! Not good...
            if (ea->in_pipes->pipes[i]->bytes_written == 0) {
                ssize_t bytes = tee(ea->out_pipe->read_fd,
                                    ea->in_pipes->pipes[i]->write_fd,
                                    4096,
                                    SPLICE_F_NONBLOCK);

                if (bytes < 0) {
                    /* TODO Write logic to handle errno... */
                    got_eof = 0;
                    if (errno != EAGAIN) {
                        PRINT_DEBUG("Failed to tee data: %s\n", strerror(errno));
                        return;
                    }
                }
                else if (bytes > 0) {
                    got_eof = 0;
                    ea->in_pipes->pipes[i]->bytes_written = (size_t)bytes;
                    *ea->bytes_spliced[i] += bytes;
                }
            }
            if (ea->in_pipes->pipes[i]->bytes_written < lowest_bytes_written) {
                lowest_bytes_written = ea->in_pipes->pipes[i]->bytes_written;
            }
        }
        /* tee duplicates input... consume input by splicing to dev null
         * TODO detect when there is exactly one output pipe and splice directly.
         * This is likely to be very common, and we can then go without the overhead
         * of duplicating the data and then discarding the original. */
        bytes = splice(ea->out_pipe->read_fd,
                               NULL,
                               fd_dev_null,
                               NULL,
                               lowest_bytes_written,
                               SPLICE_F_NONBLOCK);

        if (bytes < 0 && errno != EAGAIN) {
            return;
        }
    }
    if (got_eof && bytes == 0) {
        PRINT_DEBUG("Edge at EOF... closing pipes for edge %s\n", ea->out_pipe->edge_id);
        close(ea->out_pipe->read_fd);
        for (int k = 0; k < (int)ea->in_pipes->length; k++) {
            close(ea->in_pipes->pipes[k]->write_fd);
        }
    }
    for (int j = 0; j < (int)ea->in_pipes->length; j++) {
        ea->in_pipes->pipes[j]->bytes_written -= bytes;
    }
}

void statsCb(evutil_socket_t fd, short what, void *arg) {
    struct stats_ev_args *sa = arg;
    struct p4_file *pf = sa->pf;
    create_stats_file(pf);
}
