#include <errno.h>
#include <fcntl.h>
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

int children_gone = 0;

struct event_args {
    struct pipe_array *in_pipes;
    struct pipe *out_pipe;
    ssize_t **bytes_spliced;
};

struct sigchld_args {
    struct p4_file *pf;
    struct event_base *eb;
};


struct p4_node *find_node_by_id(struct p4_file *pf, const char *id);
struct p4_node *find_node_by_pid(struct p4_file *pf, pid_t pid);

void sigint_handler(evutil_socket_t fd, short what, void *arg) {
    PRINT_DEBUG("\b\bHandling sigint...\n");
    struct event_base *eb = arg;
    event_base_loopbreak(eb);
}

void sigchld_handler(evutil_socket_t fd, short what, void *arg) {
    PRINT_DEBUG("killing child...\n");
    struct sigchld_args *sa = arg;
    int status;
    while (1) {
        pid_t p = waitpid(-1, &status, WNOHANG);
        if (p == -1) {
            if (errno == EINTR) {
                continue;
            }
            PRINT_DEBUG("Got an unexpected error while waiting for child to terminate: %d\n", errno);
            break;
        }
        else if (p == 0) {
            break;
        }
        else if (WIFEXITED(status)) {
            children_gone++;
            PRINT_DEBUG("%dth child process ended\n", children_gone);
            struct p4_node *pn = find_node_by_pid(sa->pf, p);
            if (pn == NULL) {
                PRINT_DEBUG("No node found with pid %u\n", p);
                return;
            }
            if (pn->in_pipes && pipe_array_close(pn->in_pipes) < 0) {
                perror("close in pipe");
            }

            if (pn->out_pipes && pipe_array_close(pn->out_pipes) < 0) {
                perror("close out pipe");
            }

            if (pn->listening_edges != NULL) {
                for (int i = 0; i < (int)pn->listening_edges->length; i++) {
                    struct p4_edge *edge = pn->listening_edges->edges[i];
                    struct p4_node *downstream = find_node_by_id(sa->pf,
                            edge->to);
                    if (downstream == NULL) {
                        PRINT_DEBUG("No node found with id %s\n", edge->to);
                        return;
                    }
                    if (downstream->in_pipes) {
                        PRINT_DEBUG("closing in pipe for downstream\n");
                        for (int j = 0; j < (int)downstream->in_pipes->length; j++) {
                            if (strcmp(downstream->in_pipes->pipes[j]->edge_id, edge->id) == 0) {
                                close_pipe(downstream->in_pipes->pipes[j]);
                            }
                        }
                    }
                }
            }
            if (children_gone == (int)sa->pf->nodes->length) {
                event_base_loopexit(sa->eb, NULL);
            }
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
    *result = 0;
    if (result == NULL) {
        return NULL;
    }

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

struct p4_edge *find_edge_by_id(struct p4_file *pf, const char *id) {
    for (int i = 0; i < (int)pf->edges->length; i++) {
        struct p4_edge *pe = pf->edges->edges[i];
        if (strncmp(pe->id, id, strlen(id) + 1) == 0) {
            return pe;
        }
    }
    return NULL;
}

struct p4_node *find_node_by_id(struct p4_file *pf, const char *id) {
    for (int i = 0; i < (int)pf->nodes->length; i++) {
        struct p4_node *pn = pf->nodes->nodes[i];
        if (strncmp(pn->id, id, strlen(id) + 1) == 0) {
            return pn;
        }
    }
    return NULL;
}

struct p4_node *find_node_by_pid(struct p4_file *pf, pid_t pid) {
    for (int i = 0; i < (int)pf->nodes->length; i++) {
        struct p4_node *pn = pf->nodes->nodes[i];
        if (pid == pn->pid) {
            return pn;
        }
    }
    return NULL;
}

struct pipe *find_pipe_by_edge_id(struct pipe_array *pa, char *edge_id) {
    for (int i = 0; i < (int)pa->length; i++) {
        if (strcmp(pa->pipes[i]->edge_id, edge_id) == 0) {
            return pa->pipes[i];
        }
    }
    return NULL;
}

void pipeCb(evutil_socket_t fd, short what, void *arg) {
    struct event_args *ea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }
    for (int i = 0; i < (int)ea->in_pipes->length - 1; i++) {
        ssize_t bytes = tee(ea->out_pipe->read_fd,
                            ea->in_pipes->pipes[i]->write_fd,
                            4096,
                            SPLICE_F_NONBLOCK);
        if (bytes < 0) {
            return;
        }
        *ea->bytes_spliced[i] += bytes;
    }
    ssize_t bytes = splice(ea->out_pipe->read_fd,
                           NULL,
                           ea->in_pipes->pipes[ea->in_pipes->length - 1]->write_fd,
                           NULL,
                           4096,
                           SPLICE_F_NONBLOCK);
    if (bytes < 0) {
        return;
    }
    *ea->bytes_spliced[ea->in_pipes->length - 1] += bytes;
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
                    ea->in_pipes = pipe_array_new();
                    ea->bytes_spliced = calloc(pn->listening_edges->length,
                                               sizeof(&pn->listening_edges->edges[0]->bytes_spliced));
                    for (int k = 0; k < (int)pn->listening_edges->length; k++) {
                        struct p4_edge *edge = pn->listening_edges->edges[k];
                        ea->bytes_spliced[k] = &edge->bytes_spliced;
                        struct p4_node *dest = find_node_by_id(pf, edge->to);
                        if (dest == NULL) {
                            PRINT_DEBUG("No node found with id %s\n", edge->to);
                            free(ea->bytes_spliced);
                            pipe_array_free(ea->in_pipes);
                            free(ea);
                            return -1;
                        }
                        struct pipe *in_pipe = find_pipe_by_edge_id(dest->in_pipes, edge->id);
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
                struct p4_args *pa = args_list_new(pn->cmd);
                pid_t ppid = getppid();
                int def_sin = 1;
                int def_sout = 1;
                for (int m = 0; m < (int)pn->out_pipes->length; m++) {
                    struct pipe *out_pipe = pn->out_pipes->pipes[m];
                    if (strcmp(out_pipe->port, "-") == 0) {
                        if (def_sout == 0) {
                            // TODO this means that another edge has already changed this node's
                            // stdout. This is an invalid graph.
                        }
                        else if (dup2(out_pipe->write_fd, STDOUT_FILENO) < 0) {
                            perror("dup2 failed");
                        }
                        def_sout = 0;
                    }
                    else {
                        char *out_port_fs = calloc(50u, sizeof(*out_port_fs));
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
                            // TODO this means that another edge has already changed this node's
                            // stdout. This is an invalid graph.
                        }
                        else if (dup2(in_pipe->read_fd, STDIN_FILENO) < 0) {
                            perror("dup2 failed");
                        }
                        def_sin = 0;
                    }
                    else {
                        char *in_port_fs = calloc(20u, sizeof(*in_port_fs));
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
                    }
                    if (po->out_pipes && pipe_array_close(po->out_pipes) < 0) {
                        perror("close_pipe failed");
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
            } // end child
            else {
                pn->pid = pid;
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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "specify json filename\n");
        return 1;
    }

    struct p4_file *pf = p4_file_new(argv[1]);
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

    struct sigchld_args *sa = malloc(sizeof(*sa));
    if (sa == NULL) {
        PRINT_DEBUG("Failed to allocate memory for sigchld_args\n");
        event_free(sigintev);
        event_base_free(eb);
        free_p4_file(pf);
        return 1;
    }
    sa->pf = pf;
    sa->eb = eb;

    struct event *sigchldev = evsignal_new(eb, SIGCHLD, sigchld_handler, sa);
    if (sigchldev == NULL) {
        PRINT_DEBUG("Failed to create sigchld event\n");
        free(sa);
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

    event_base_dispatch(eb);

    event_free(sigintev);
    event_free(sigchldev);
    event_base_free(eb);
    free_p4_file(pf);
    free(sa);

    return 0;
}
