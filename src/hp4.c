#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event2/event.h>

#include "debug.h"
#include "event_handlers.h"
#include "hp4.h"
#include "parser.h"
#include "pipe.h"
#include "stats.h"
#include "strutil.h"

#define DEFAULT_INTERVAL 1000

int build_edges(struct p4_file *pf) {
    for (int i=0; i < (int)pf->edges->length; i++) {
        struct p4_edge *pe = pf->edges->edges[i];
        struct p4_node *from = find_node_by_id(pf, pe->from);
        if (from == NULL) {
            fprintf(stderr, " ERROR: No node found with id %s\n", pe->from);
            return -1;
        }
        struct p4_node *to = find_node_by_id(pf, pe->to);
        if (to == NULL) {
            fprintf(stderr, " ERROR: No node found with id %s\n", pe->to);
            return -1;
        }

        if (strncmp(from->type, "EXEC\0", 5) == 0 && strncmp(to->type, "EXEC\0", 5) == 0) {
            /* if from->out_pipes does NOT have a port named pe->from_port */
            if (pipe_array_has_pipe_with_port(from->out_pipes, pe->from_port) == 0) {
                if (pipe_array_append_new(from->out_pipes, pe->from_port, pe->id) < 0) {
                    return -1;
                }
            }
            /* if to->in_pipes does NOT have a port named pe->to_port */
            if (pipe_array_has_pipe_with_port(to->in_pipes, pe->to_port) == 0) {
                if (pipe_array_append_new(to->in_pipes, pe->to_port, pe->id) < 0) {
                    return -1;
                }
            }

            if (from->listening_edges == NULL) {
                from->listening_edges = malloc(sizeof(*from->listening_edges));
                if (from->listening_edges == NULL) {
                    REPORT_ERROR(strerror(errno));
                    return -1;
                }
                from->listening_edges->edges = NULL;
                from->listening_edges->length = 0u;
            }
            if (from->listening_edges->length == 0u) {
                from->listening_edges->edges = malloc(sizeof(*from->listening_edges->edges));
                if (from->listening_edges->edges == NULL) {
                    REPORT_ERROR(strerror(errno));
                    return -1;
                }
                from->listening_edges->edges[0] = pe;
                from->listening_edges->length = 1;
            }
            else {
                from->listening_edges->length++;
                struct p4_edge **newmem = realloc(from->listening_edges->edges,
                        (from->listening_edges->length) * sizeof(*from->listening_edges->edges));
                if (newmem == NULL) {
                    REPORT_ERROR(strerror(errno));
                    return -1;
                }
                from->listening_edges->edges = newmem;
                from->listening_edges->edges[from->listening_edges->length - 1] = pe;
            }
        }
        else {
            // TODO support non-EXEC nodes maybe.
            // TODO If not a known node type, this is an invalid graph.
            free_p4_file(pf);
            fprintf(stderr, "Non-EXEC nodes are not yet supported\n");
            return -1;
        }
    }
    return 0;
}

int run_node(struct p4_file *pf, struct p4_node *pn) {
    struct argstruct *pa = malloc(sizeof(*pa));
    if (pa == NULL)
        return -1;

    if (parse_argstring(pa, pn->cmd) < 0)
        return -1;

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
                REPORT_ERROR("Tried to change a node's stdout a second time");
                return -1;
            }
            else if (dup2(out_pipe->write_fd, STDOUT_FILENO) < 0) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            def_sout = 0;
        }
        else {
            /* TODO can required string length be calculated? 50 *should* be enough */
            char *out_port_fs = calloc(50u, sizeof(*out_port_fs));
            if (out_port_fs == NULL) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            if (sprintf(out_port_fs, "/proc/%u/fd/%d", ppid, out_pipe->write_fd) < 0) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            for (int n = 0; n < pa->argc; n++) {
                char *replaced = strrep(pa->argv[n], out_pipe->port, out_port_fs);
                if (replaced == NULL)
                    return -1;
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
                REPORT_ERROR("Tried to change a node's stdin a second time");
                return -1;
            }
            else if (dup2(in_pipe->read_fd, STDIN_FILENO) < 0) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            def_sin = 0;
        }
        else {
            /* TODO can required string length be calculated? 50 *should* be enough */
            char *in_port_fs = calloc(50u, sizeof(*in_port_fs));
            if (in_port_fs == NULL) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            if (sprintf(in_port_fs, "/proc/%u/fd/%d", ppid, in_pipe->read_fd) < 0) {
                REPORT_ERROR(strerror(errno));
                return -1;
            }
            for (int p = 0; p < pa->argc; p++) {
                char *replaced = strrep(pa->argv[p], in_pipe->port, in_port_fs);
                if (replaced == NULL)
                    return -1;
                pa->argv[p] = replaced;
            }
        }
    }
    for (int q = 0; q < (int)pf->nodes->length; q++) {
        struct p4_node *po = pf->nodes->nodes[q];
        if (po->in_pipes && pipe_array_close(po->in_pipes) < 0) {
            return -1;
        }
        if (po->out_pipes && pipe_array_close(po->out_pipes) < 0) {
            return -1;
        }
    }
    /* TODO do we care if these closes fail? */
    if (def_sin == 1) {
        close(STDIN_FILENO);
    }
    if (def_sout == 1) {
        close(STDOUT_FILENO);
    }
    /* TODO should stderr be closed? */
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
                    struct readable_ev_args *rea = malloc(sizeof(*rea));
                    if (rea == NULL) {
                        REPORT_ERROR(strerror(errno));
                        return -1;
                    }
                    struct pipe *from_pipe = pn->out_pipes->pipes[j];
                    int read_fd = from_pipe->read_fd;
                    int current_read_flags = fcntl(read_fd, F_GETFL, NULL);
                    if (current_read_flags < 0) {
                        REPORT_ERROR(strerror(errno));
                        return -1;
                    }
                    if (fcntl(read_fd, F_SETFL, current_read_flags | O_NONBLOCK) < 0) {
                        REPORT_ERROR(strerror(errno));
                        return -1;
                    }
                    struct pipe_array *to_pipes = pipe_array_new();
                    if (to_pipes == NULL) {
                        return -1;
                    }
                    rea->to_pipes = to_pipes;
                    ssize_t **bytes_spliced = calloc(pn->listening_edges->length,
                            sizeof(&pn->listening_edges->edges[0]->bytes_spliced));
                    if (bytes_spliced == NULL) {
                        REPORT_ERROR(strerror(errno));
                        return -1;
                    }

                    struct event *readable = event_new(eb, from_pipe->read_fd,
                                                       EV_READ, readable_handler,
                                                       rea);
                    if (readable == NULL) {
                        REPORT_ERROR("Failed to create new readable event");
                        return -1;
                    }

                    rea->writable_events = event_array_new();
                    if (rea->writable_events == NULL) {
                        return -1;
                    }

                    size_t *bytes_safely_written = malloc(sizeof(*bytes_safely_written));
                    if (bytes_safely_written == NULL) {
                        REPORT_ERROR(strerror(errno));
                        return -1;
                    }

                    *bytes_safely_written = SIZE_MAX;
                    rea->bytes_safely_written = bytes_safely_written;

                    for (int k = 0; k < (int)pn->listening_edges->length; k++) {
                        struct writable_ev_args *wea = malloc(sizeof(*wea));
                        if (wea == NULL) {
                            REPORT_ERROR(strerror(errno));
                            return -1;
                        }
                        wea->from_pipe = from_pipe;
                        wea->to_pipes = to_pipes;
                        wea->bytes_spliced = bytes_spliced;
                        wea->to_pipe_idx = k;
                        wea->readable_event = readable;
                        wea->bytes_safely_written = bytes_safely_written;
                        struct p4_edge *edge = pn->listening_edges->edges[k];
                        bytes_spliced[k] = &edge->bytes_spliced;
                        struct p4_node *dest = find_node_by_id(pf, edge->to);
                        if (dest == NULL) {
                            fprintf(stderr, "No node found with id %s\n", edge->to);
                            event_array_free(rea->writable_events);
                            free(rea);
                            pipe_array_free(to_pipes);
                            event_free(readable);
                            free(bytes_spliced);
                            free(wea);
                            return -1;
                        }
                        struct pipe *to_pipe = find_pipe_by_edge_id(dest->in_pipes, edge->id);
                        if (to_pipe == NULL) {
                            fprintf(stderr, "No edge found with id %s\n", edge->id);
                            return -1;
                        }
                        int write_fd = to_pipe->write_fd;
                        int current_write_flags = fcntl(write_fd, F_GETFL, NULL);
                        if (current_write_flags < 0) {
                            REPORT_ERROR(strerror(errno));
                            return -1;
                        }

                        if (fcntl(write_fd, F_SETFL, current_write_flags | O_NONBLOCK) < 0) {
                            REPORT_ERROR(strerror(errno));
                            return -1;
                        }
                        if (pipe_array_append(wea->to_pipes, to_pipe) < 0) {
                            event_array_free(rea->writable_events);
                            free(rea);
                            pipe_array_free(wea->to_pipes);
                            event_free(readable);
                            free(bytes_spliced);
                            free(wea);
                            return -1;
                        }
                        struct event *writable = event_new(eb, to_pipe->write_fd,
                                                           EV_WRITE, writable_handler,
                                                           wea);
                        if (writable == NULL) {
                            REPORT_ERROR("Failed to create new writable event");
                            return -1;
                        }
                        if (event_array_append(rea->writable_events, writable) < 0) {
                            event_array_free(rea->writable_events);
                            free(rea);
                            pipe_array_free(to_pipes);
                            event_free(readable);
                            free(bytes_spliced);
                            free(wea);
                            return -1;
                        }
                    }

                    if (event_add(readable, NULL) < 0) {
                        REPORT_ERROR("Failed to add readable event");
                        event_array_free(rea->writable_events);
                        free(rea);
                        pipe_array_free(to_pipes);
                        free(bytes_spliced);
                        event_free(readable);
                        return -1;
                    }
                }
            }
            pid_t pid = fork();
            if (pid < 0) {
                REPORT_ERROR(strerror(errno));
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
            fprintf(stderr, "Non-EXEC nodes not yet supported\n");
            return -1;
        }
    }
    return 0;
}

void usage(char **argv) {
    printf("Usage: %s [OPTIONS] file\n", argv[0]);
    printf("\n");
    printf("  -h, --help      display this help and exit\n");
    printf("  -V, --version   display version string and exit\n");
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
        {"version",  no_argument,       0, 'V'},
        {"help",     no_argument,       0, 'h'},
        {0,          0,                 0,  0 }
    };
    char c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "i:f:Vh", long_options, &option_index)) >= 0) {
        switch (c) {
            case 'h':
                args->help = 1;
                break;
            case 'V':
                args->version = 1;
                break;
            case 'i':
                args->stats_interval = optarg;
                break;
            case 'f':
                args->graph_file = optarg;
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
    args.version = 0;

    if (get_args(argc, argv, &args) < 0) {
        usage(argv);
        return 1;
    }

    if (args.help == 1) {
        usage(argv);
        return 0;
    }

    if (args.version == 1) {
        printf("%s\n", PACKAGE_VERSION);
        return 0;
    }

    if (args.graph_file == NULL) {
        printf("A file containing a p4 graph MUST be specified\n");
        usage(argv);
        return 1;
    }

    struct p4_file *pf = p4_file_new(args.graph_file);
    if (pf == NULL) {
        return 1;
    }

    if (open_dev_null() < 0) {
        free_p4_file(pf);
        return 1;
    }

    struct event_base *eb = event_base_new();
    if (eb == NULL) {
        REPORT_ERROR("Failed to create a new event_base");
        free_p4_file(pf);
        return 1;
    }

    struct event *sigintev = evsignal_new(eb, SIGINT, sigint_handler, eb);
    if (sigintev == NULL) {
        REPORT_ERROR("Failed to create sigint event");
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    if (event_add(sigintev, NULL) < 0) {
        REPORT_ERROR("Failed to add sigint event");
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
        REPORT_ERROR("Failed to create sigchld event");
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    if (event_add(sigchldev, NULL) < 0) {
        REPORT_ERROR("Failed to add sigchld event");
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

    unsigned long interval_secs, interval_ms, interval_us;
    if (args.stats_interval) {
        interval_ms = strtoul(args.stats_interval, NULL, 10);
        if (interval_ms == 0)
            interval_ms = DEFAULT_INTERVAL;
    }
    else {
        interval_ms = DEFAULT_INTERVAL;
    }


    interval_secs = interval_ms / 1000;
    interval_us = (interval_ms % 1000) * 1000;

    struct event *dump_stats = event_new(eb, -1, EV_PERSIST, stats_handler, &sea);
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

    stats_handler(0, 0, &sea);

    event_free(dump_stats);
    event_free(sigintev);
    event_free(sigchldev);
    event_base_free(eb);
    free_p4_file(pf);

    close_dev_null();

    return 0;
}
