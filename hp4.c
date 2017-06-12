#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <event2/event.h>

#include "hp4.h"
#include "parser.h"
#include "pipe.h"
#include "stats.h"

#define DEFAULT_INTERVAL 1000

int fd_dev_null = -1;

void close_dev_null(void) {
    if (fd_dev_null >= 0) {
        // ignore return value as we do not need to ensure writes
        // to /dev/null are successful
        close(fd_dev_null);
        fd_dev_null = -1;
    }
}

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
                PRINT_DEBUG("Got an unexpected error while waiting for child to terminate: %s\n", strerror(errno));
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

/**
 * Returns a _new_ string which is the same as `original`, except that all
 * occurrences of `replace` are replaced with `with`.
 */
char *strrep(const char *original, const char *replace, const char *with) {
    if (original == NULL || replace == NULL || with == NULL) {
        return NULL;
    }
    if (strlen(original) == 0 || strlen(replace) == 0) {
        return NULL;
    }

    /* Pointer to track current location in `original`. */
    const char *strp = original;
    size_t result_len = 1;
    char *result = malloc(result_len);
    if (result == NULL) {
        return NULL;
    }
    *result = 0;

    char *next = NULL;
    while ((next = strstr(strp, replace)) != NULL) {
        result_len += next - strp + strlen(with);
        result = realloc(result, result_len);
        if (result == NULL) {
            return NULL;
        }
        strncat(result, strp, (next - strp));
        strcat(result, with);
        strp = next + strlen(replace);
    }

    result_len += strlen(strp);
    result = realloc(result, result_len);
    if (result == NULL) {
        return NULL;
    }
    strcat(result, strp);

    return result;
}

void pipeCb(evutil_socket_t fd, short what, void *arg) {
    struct event_args *ea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }
    size_t lowest_bytes_written = INT_MAX;
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
                ea->in_pipes->pipes[i]->bytes_written = 0;
            }
            else if (bytes > 0) {
                ea->in_pipes->pipes[i]->bytes_written = (size_t)bytes;
                *ea->bytes_spliced[i] += bytes;
            }
        }
        if (ea->in_pipes->pipes[i]->bytes_written < lowest_bytes_written) {
            lowest_bytes_written = ea->in_pipes->pipes[i]->bytes_written;
        }
    }
    ssize_t bytes = splice(ea->out_pipe->read_fd,
                           NULL,
                           fd_dev_null,
                           NULL,
                           lowest_bytes_written,
                           SPLICE_F_NONBLOCK);
    if (bytes < 0 && errno != EAGAIN) {
        return;
    }
    if (bytes == 0) {
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

int build_edges(struct p4_file *pf) {
    for (int i=0; i < (int)pf->edges->length; i++) {
        struct p4_edge *pe = pf->edges->edges[i];
        struct p4_node *from = find_node_by_id(pf, pe->from);
        if (from == NULL) {
            PRINT_DEBUG("No node found with id %s\n", pe->from);
            return -1;
        }
        struct p4_node *to = find_node_by_id(pf, pe->to);
        if (to == NULL) {
            PRINT_DEBUG("No node found with id %s\n", pe->to);
            return -1;
        }

        if (strncmp(from->type, "EXEC\0", 5) == 0 && strncmp(to->type, "EXEC\0", 5) == 0) {
            if (pipe_array_has_pipe_with_port(from->out_pipes, pe->from_port) == 0) {
                pipe_array_append_new(from->out_pipes, pe->from_port, pe->id);
            }
            if (pipe_array_has_pipe_with_port(to->in_pipes, pe->to_port) == 0) {
                pipe_array_append_new(to->in_pipes, pe->to_port, pe->id);
            }
            if (from->listening_edges == NULL) {
                from->listening_edges = malloc(sizeof(*from->listening_edges));
                if (from->listening_edges == NULL) {
                    PRINT_DEBUG("Failed to allocate memory\n");
                    return -1;
                }
                from->listening_edges->edges = NULL;
                from->listening_edges->length = 0u;
            }
            if (from->listening_edges->length == 0u) {
                from->listening_edges->edges = malloc(sizeof(*from->listening_edges->edges));
                if (from->listening_edges->edges == NULL) {
                    PRINT_DEBUG("Failed to allocate memory\n");
                    return -1;
                }
                from->listening_edges->edges[0] = pe;
                from->listening_edges->length = 1;
            }
            else {
                from->listening_edges->length++;
                from->listening_edges->edges = realloc(from->listening_edges->edges,
                        (from->listening_edges->length) * sizeof(*from->listening_edges->edges));
                if (from->listening_edges->edges == NULL) {
                    PRINT_DEBUG("Failed to reallocate memory\n");
                    return -1;
                }
                from->listening_edges->edges[from->listening_edges->length - 1] = pe;
            }
        }
        else {
            // TODO support non-EXEC nodes maybe.
            // TODO If not a known node type, this is an invalid graph.
            free_p4_file(pf);
            PRINT_DEBUG("Non-EXEC nodes are not yet supported\n");
            return -1;
        }
    }
    return 0;
}

int run_node(struct p4_file *pf, struct p4_node *pn) {
    struct p4_args *pa = args_list_new(pn->cmd);
    pid_t ppid = getppid();
    int def_sin = 1;
    int def_sout = 1;
    for (int m = 0; m < (int)pn->out_pipes->length; m++) {
        struct pipe *out_pipe = pn->out_pipes->pipes[m];
        if (strcmp(out_pipe->port, "-") == 0) {
            if (def_sout == 0) {
                // TODO this means that another pipe has already changed this node's
                // stdout. It should not happen; two edges which read stdout from
                // same node should share an out_pipe.
            }
            else if (dup2(out_pipe->write_fd, STDOUT_FILENO) < 0) {
                perror("dup2 failed");
                return 1;
            }
            def_sout = 0;
        }
        else {
            char *out_port_fs = calloc(50u, sizeof(*out_port_fs));
            if (out_port_fs == NULL) {
                return 1;
            }
            sprintf(out_port_fs, "/proc/%u/fd/%d", ppid, out_pipe->write_fd);
            for (int n = 0; n < pa->argc; n++) {
                char *replaced = strrep(pa->argv[n], out_pipe->port, out_port_fs);
                pa->argv[n] = replaced;
            }
        }
    }

    for (int o = 0; o < (int)pn->in_pipes->length; o++) {
        struct pipe *in_pipe = pn->in_pipes->pipes[o];
        if (strcmp(in_pipe->port, "-") == 0) {
            if (def_sin == 0) {
                // TODO this means that another pipe has already changed this node's
                // stdin. This should not happen; two edges which read stdin from the
                // same node should share in_pipe.
                return 1;
            }
            else if (dup2(in_pipe->read_fd, STDIN_FILENO) < 0) {
                perror("dup2 failed");
                return 1;
            }
            def_sin = 0;
        }
        else {
            char *in_port_fs = calloc(20u, sizeof(*in_port_fs));
            if (in_port_fs == NULL) {
                return 1;
            }
            sprintf(in_port_fs, "/proc/%u/fd/%d", ppid, in_pipe->read_fd);
            for (int p = 0; p < pa->argc; p++) {
                char *replaced = strrep(pa->argv[p], in_pipe->port, in_port_fs);
                pa->argv[p] = replaced;
            }
        }
    }
    for (int q = 0; q < (int)pf->nodes->length; q++) {
        struct p4_node *po = pf->nodes->nodes[q];
        if (po->in_pipes && pipe_array_close(po->in_pipes) < 0) {
            perror("close_pipe failed");
            return 1;
        }
        if (po->out_pipes && pipe_array_close(po->out_pipes) < 0) {
            perror("close_pipe failed");
            return 1;
        }
    }
    if (def_sin == 1) {
        close(STDIN_FILENO);
    }
    if (def_sout == 1) {
        close(STDOUT_FILENO);
    }
    PRINT_DEBUG("Node %s about to exec\n", pn->id);
    execvp(pa->argv[0], pa->argv);
    return 0;
}

int build_nodes(struct p4_file *pf, struct event_base *eb) {
    for (int i=0; i < (int)pf->nodes->length; i++) {
        struct p4_node *pn = pf->nodes->nodes[i];
        if (strncmp(pn->type, "EXEC\0", 5) == 0) {
            if (pn->in_pipes->length == 0u && pn->out_pipes->length == 0u) {
                // node is not joined to the graph, skip
                // TODO this is not necessarily true if cmd writes to a file directly
                // with no input. But then, why run that program through (h)p4?
                // Just run directly to get same result.
                // TODO this is an invalid graph
                PRINT_DEBUG("EXEC node %s is not connected to graph\n", pn->id);
                continue;
            }
            if (pn->out_pipes->length != 0u) {
                for (int j = 0; j < (int)pn->out_pipes->length; j++) {
                    struct event_args *ea = malloc(sizeof(*ea));
                    if (ea == NULL) {
                        return -1;
                    }
                    ea->out_pipe = pn->out_pipes->pipes[j];
                    int read_fd = ea->out_pipe->read_fd;
                    fcntl(read_fd, F_SETFL, fcntl(read_fd, F_GETFL, NULL) & O_NONBLOCK);
                    ea->in_pipes = pipe_array_new();
                    ea->bytes_spliced = calloc(pn->listening_edges->length,
                            sizeof(&pn->listening_edges->edges[0]->bytes_spliced));
                    if (ea->bytes_spliced == NULL) {
                        return -1;
                    }

                    for (int k = 0; k < (int)pn->listening_edges->length; k++) {
                        struct p4_edge *edge = pn->listening_edges->edges[k];
                        ea->bytes_spliced[k] = &edge->bytes_spliced;
                        struct p4_node *dest = find_node_by_id(pf, edge->to);
                        if (dest == NULL) {
                            PRINT_DEBUG("No node found with id %s\n", edge->to);
                            pipe_array_free(ea->in_pipes);
                            free(ea->bytes_spliced);
                            free(ea);
                            return -1;
                        }
                        struct pipe *in_pipe = find_pipe_by_edge_id(dest->in_pipes, edge->id);
                        int write_fd = in_pipe->write_fd;
                        fcntl(write_fd, F_SETFL, fcntl(write_fd, F_GETFL, NULL) & O_NONBLOCK);
                        pipe_array_append(ea->in_pipes, in_pipe);
                    }
                    struct event *readable = event_new(eb, ea->out_pipe->read_fd,
                                                       EV_READ|EV_PERSIST, pipeCb,
                                                       ea);
                    if (event_add(readable, NULL) < 0) {
                        // TODO print something?
                        event_free(readable);
                        pipe_array_free(ea->in_pipes);
                        free(ea->bytes_spliced);
                        free(ea);
                        return -1;
                    }
                }
            }
            pid_t pid = fork();
            if (pid < 0) {
                perror("forking process failed");
            }
            else if (pid == 0) { // child
                return run_node(pf, pn);
            } // end child
            else {
                pn->pid = pid;
                pn->ended = 0;
            }
        }
        else {
            // TODO support *FILE nodes
            free_p4_file(pf);
            PRINT_DEBUG("Non-EXEC nodes not yet supported\n");
            return -1;
        }
    }
    return 0;
}

void usage(char **argv) {
    printf("Usage: %s [OPTIONS] file\n", argv[0]);
    printf("\n");
    printf("  -h, --help      display this help and exit\n");
    printf("  -i, --interval  set time in milliseconds between dumping stats\n");
    printf("                    to stdout; defaults to %d\n", DEFAULT_INTERVAL);
    printf("  -f, --file      file containing json definition of process graph\n");
    return;
}

int get_args(int argc, char **argv, struct hp4_args *args) {
    static struct option long_options[] =
    {
        {"interval", required_argument, 0, 'i'},
        {"file",     required_argument, 0, 'f'},
        {"help",     no_argument,       0, 'h'},
        {0,          0,                 0,  0 }
    };
    char c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "i:f:h", long_options, &option_index)) >= 0) {
        switch (c) {
            case 'i':
                args->stats_interval = optarg;
                break;
            case 'f':
                args->graph_file = optarg;
                break;
            case 'h':
                args->help = 1;
                break;
            default:
                break;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    struct hp4_args args;
    args.stats_interval = NULL;
    args.graph_file = NULL;
    args.help = 0;

    if (get_args(argc, argv, &args) < 0) {
        usage(argv);
        return 1;
    }

    if (args.help == 1) {
        usage(argv);
        return 0;
    }

    if (args.graph_file == NULL) {
        printf("Did not specify a file\n");
        usage(argv);
        return 1;
    }

    fd_dev_null = open("/dev/null", O_WRONLY);
    atexit(close_dev_null);

    struct p4_file *pf = p4_file_new(args.graph_file);
    if (pf == NULL) {
        return 1;
    }

    struct event_base *eb = event_base_new();
    if (eb == NULL) {
        free_p4_file(pf);
        return 1;
    }

    struct event *sigintev = evsignal_new(eb, SIGINT, sigint_handler, eb);
    if (sigintev == NULL) {
        PRINT_DEBUG("Failed to create sigint event\n");
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    if (event_add(sigintev, NULL) < 0) {
        PRINT_DEBUG("Failed to add sigint event\n");
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    struct sigchld_args sa;
    sa.pf = pf;
    sa.eb = eb;
    sa.n_children_exited = 0;

    struct event *sigchldev = evsignal_new(eb, SIGCHLD, sigchld_handler, &sa);
    if (sigchldev == NULL) {
        PRINT_DEBUG("Failed to create sigchld event\n");
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    if (event_add(sigchldev, NULL) < 0) {
        PRINT_DEBUG("Failed to add sigchld event\n");
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    if (pf == NULL) {
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    if (build_edges(pf) == -1) {
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    if (build_nodes(pf, eb) == -1) {
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    struct stats_ev_args sea;
    sea.pf = pf;

    long interval_secs, interval_ms, interval_us;
    if (args.stats_interval) {
        interval_ms = strtol(args.stats_interval, NULL, 10);
    }
    else {
        interval_ms = DEFAULT_INTERVAL;
    }

    interval_secs = interval_ms / 1000;
    interval_us = (interval_ms % 1000) * 1000;

    struct event *dump_stats = event_new(eb, -1, EV_PERSIST, statsCb, &sea);
    if (dump_stats == NULL) {
        PRINT_DEBUG("failed to create stats dump event.\n");
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    struct timeval delay = {interval_secs, interval_us};
    if (event_add(dump_stats, &delay) < 0) {
        event_free(dump_stats);
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    if (event_base_dispatch(eb) < 0) {
        event_free(dump_stats);
        event_free(sigchldev);
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }

    statsCb(0, 0, &sea);

    event_free(dump_stats);
    event_free(sigintev);
    event_free(sigchldev);
    event_base_free(eb);
    free_p4_file(pf);

    return 0;
}
