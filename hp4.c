#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <event2/event.h>

#include "parser.h"

#ifdef HP4_DEBUG
#define PRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else /* HP4_DEBUG */
#define PRINT_DEBUG(...) while(0)
#endif /* HP4_DEBUG */

int children_gone = 0;

struct pipe {
    int read_fd;
    int write_fd;
};

struct event_args {
    struct pipe **in_pipes;
    struct pipe *out_pipe;
    int n_in_pipes;
    ssize_t **bytes_spliced;
};

struct sigchld_args {
    struct p4_file *pf;
    struct event_base *eb;
};


struct p4_node *find_node_by_id(struct p4_file *pf, const char *id);
struct p4_node *find_node_by_pid(struct p4_file *pf, pid_t pid);

struct pipe *pipe_new(void) {
    struct pipe *new_pipe = malloc(sizeof(*new_pipe));
    if (new_pipe == NULL) {
        return NULL;
    }

    int fds[2] = {0, 0};
    if (pipe(fds) < 0) {
        free(new_pipe);
        return NULL;
    }

    PRINT_DEBUG("pipe_new {%d %d}\n", fds[0], fds[1]);
    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

int close_pipe(struct pipe *pipe_to_close) {
    PRINT_DEBUG("close_pipe {%d %d}\n", pipe_to_close->read_fd, pipe_to_close->write_fd);
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

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
            if (pn->in_pipe && close_pipe(pn->in_pipe) < 0) {
                perror("close in pipe");
            }

            if (pn->out_pipe && close_pipe(pn->out_pipe) < 0) {
                perror("close out pipe");
            }

            if (pn->listening_edges != NULL) {
                for (int i = 0; i < (int)pn->listening_edges->length; i++) {
                    struct p4_node *downstream = find_node_by_id(sa->pf,
                            (pn->listening_edges->edges[i])->to);
                    if (downstream == NULL) {
                        PRINT_DEBUG("No node found with id %s\n",
                                    pn->listening_edges->edges[i]->to);
                        return;
                    }
                    if (downstream->in_pipe) {
                        PRINT_DEBUG("closing in pipe for downstream\n");
#ifdef HP4_DEBUG
                        int is_closed =
#endif /* HP4_DEBUG */
                        close_pipe(downstream->in_pipe);

                        free(downstream->in_pipe);
                        downstream->in_pipe = NULL;
                        PRINT_DEBUG("got %d\n", is_closed);
                    }
                }
            }
            if (children_gone == (int)sa->pf->nodes->length) {
                event_base_loopexit(sa->eb, NULL);
            }
        }
    }
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

void pipeCb(evutil_socket_t fd, short what, void *arg) {
    struct event_args *ea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }
    for (int i = 0; i < ea->n_in_pipes - 1; i++) {
        ssize_t bytes = tee(ea->out_pipe->read_fd,
                            ea->in_pipes[i]->write_fd,
                            4096,
                            SPLICE_F_NONBLOCK);
        if (bytes < 0) {
            return;
        }
        *ea->bytes_spliced[i] += bytes;
    }
    ssize_t bytes = splice(ea->out_pipe->read_fd,
                           NULL,
                           ea->in_pipes[ea->n_in_pipes - 1]->write_fd,
                           NULL,
                           4096,
                           SPLICE_F_NONBLOCK);
    if (bytes < 0) {
        return;
    }
    *ea->bytes_spliced[ea->n_in_pipes - 1] += bytes;
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
            // Each EXEC node should have a max of one pipe in, one pipe out
            if (from->out_pipe == NULL && (from->out_pipe = pipe_new()) == NULL) {
                PRINT_DEBUG("Failed to create pipe\n");
                return -1;
            }
            if (to->in_pipe == NULL && (to->in_pipe = pipe_new()) == NULL) {
                PRINT_DEBUG("Failed to create pipe\n");
                return -1;
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
            // TODO support non-EXEC nodes
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
            if (pn->in_pipe == NULL && pn->out_pipe == NULL) {
                // node is not joined to the graph, skip
                // TODO this is not necessarily true if cmd writes to a file directly.
                // But then, why run that program through (h)p4? Just run directly to
                // get same result.
                PRINT_DEBUG("EXEC node %s is not connected to graph\n", pn->id);
                continue;
            }
            if (pn->out_pipe != NULL) {
                struct event_args *ea = malloc(sizeof(*ea));
                if (ea == NULL) {
                    return -1;
                }
                ea->out_pipe = pn->out_pipe;
                ea->n_in_pipes = (int)pn->listening_edges->length;
                ea->bytes_spliced = calloc(pn->listening_edges->length,
                                           sizeof(*ea->bytes_spliced));
                if (ea->bytes_spliced == NULL) {
                    PRINT_DEBUG("Failed to allocate memory\n");
                    free(ea);
                    return -1;
                }
                ea->in_pipes = calloc(pn->listening_edges->length,
                                      sizeof(*ea->in_pipes));
                if (ea->in_pipes == NULL) {
                    PRINT_DEBUG("Failed to allocate memory\n");
                    free(ea->bytes_spliced);
                    free(ea);
                    return -1;
                }
                for (int j=0; j < (int)pn->listening_edges->length; j++) {
                    ea->bytes_spliced[j] = &pn->listening_edges->edges[j]->bytes_spliced;
                    ea->in_pipes[j] = find_node_by_id(pf,
                                          pn->listening_edges->edges[j]->to)->in_pipe;
                    if (ea->in_pipes[j] == NULL) {
                        PRINT_DEBUG("No node found with id %s\n",
                                pn->listening_edges->edges[j]->to);
                        free(ea->in_pipes);
                        free(ea->bytes_spliced);
                        free(ea);
                        return -1;
                    }
                }
                struct event *readable = event_new(eb, pn->out_pipe->read_fd,
                                                   EV_READ|EV_PERSIST, pipeCb,
                                                   ea);
                if (event_add(readable, NULL) < 0) {
                    // TODO print something?
                    event_free(readable);
                    free(ea->in_pipes);
                    free(ea->bytes_spliced);
                    free(ea);
                    return -1;
                }
            }
            pid_t pid = fork();
            if (pid < 0) {
                perror("forking process failed");
            }
            else if (pid == 0) { // child
                if (pn->out_pipe != NULL) {
                    if (dup2(pn->out_pipe->write_fd, STDOUT_FILENO) < 0) {
                        perror("dup2 failed");
                    }
                }
                if (pn->in_pipe != NULL) {
                    if (dup2(pn->in_pipe->read_fd, STDIN_FILENO) < 0) {
                        perror("dup2 failed");
                    }
                }
                for (int j = 0; j < (int)pf->nodes->length; j++) {
                    struct p4_node *po = pf->nodes->nodes[j];
                    if (po->in_pipe && close_pipe(po->in_pipe) < 0) {
                        perror("close_pipe failed");
                    }
                    if (po->out_pipe && close_pipe(po->out_pipe) < 0) {
                        perror("close_pipe failed");
                    }
                }
                PRINT_DEBUG("Node %s about to exec\n", pn->id);
                p4_args_list_t p4_argv = args_list_new(pn->cmd);
                execvp(p4_argv[0], p4_argv);
                free(p4_argv);
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
        PRINT_DEBUG("specify json filename\n");
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
