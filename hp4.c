#define _GNU_SOURCE

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

    int fds[2] = {0, 0};
    if (pipe(fds) < 0) {
        free(new_pipe);
        return NULL;
    }

#ifdef HP4_DEBUG
    fprintf(stderr, "pipe_new {%d %d}\n", fds[0], fds[1]);
#endif /* HP4_DEBUG */
    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

int close_pipe(struct pipe *pipe_to_close) {
#ifdef HP4_DEBUG
    fprintf(stderr, "close_pipe {%d %d}\n", pipe_to_close->read_fd, pipe_to_close->write_fd);
#endif /* HP4_DEBUG */
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

void sigint_handler(evutil_socket_t fd, short what, void *arg) {
#ifdef HP4_DEBUG
    fprintf(stderr, "\b\bHandling sigint...\n");
#endif /* HP4_DEBUG */
    struct event_base *eb = arg;
    event_base_loopbreak(eb);
}

void sigchld_handler(evutil_socket_t fd, short what, void *arg) {
#ifdef HP4_DEBUG
    fprintf(stderr, "killing child...\n");
#endif /* HP4_DEBUG */
    struct sigchld_args *sa = arg;
    int *status = malloc(sizeof(*status));
    while (1) {
#ifdef HP4_DEBUG
        fprintf(stderr, "loop\n");
#endif /* HP4_DEBUG */
        pid_t p = waitpid(-1, status, WNOHANG);
#ifdef HP4_DEBUG
        fprintf(stderr, "pid %d\n", p);
#endif /* HP4_DEBUG */
        if (p == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        else if (p == 0) {
            break;
        }
        else if (WIFEXITED(*status)) {
            children_gone++;
#ifdef HP4_DEBUG
            fprintf(stderr, "%d child processes ended\n", children_gone);
#endif /* HP4_DEBUG */
            struct p4_node *pn = find_node_by_pid(sa->pf, p);
            if (pn->in_pipe && close_pipe(pn->in_pipe) < 0) {
                perror("close in pipe");
            }

            if (pn->out_pipe && close_pipe(pn->out_pipe) < 0) {
                perror("close out pipe");
            }

            for (int i = 0; i < (int)pn->n_listening_edges; i++) {
                struct p4_node *downstream = find_node_by_id(sa->pf,
                        pn->listening_edges[i]->to);
                if (downstream->in_pipe) {
#ifdef HP4_DEBUG
                    fprintf(stderr, "closing in pipe for downstream\n");
                    int is_closed = close_pipe(downstream->in_pipe);
#else
                    close_pipe(downstream->in_pipe);
#endif /* HP4_DEBUG */
                    free(downstream->in_pipe);
                    downstream->in_pipe = NULL;
#ifdef HP4_DEBUG
                    fprintf(stderr, "got %d\n", is_closed);
#endif /* HP4_DEBUG */
                }
            }
            if (children_gone == sa->pf->n_nodes) {
                event_base_loopexit(sa->eb, NULL);
            }
        }
        else {
        }
    }
    free(status);
}

struct p4_edge *find_edge_by_id(struct p4_file *pf, const char *id) {
    for (int i = 0; i < pf->n_edges; i++) {
        struct p4_edge *pe = &pf->edges[i];
        if (strncmp(pe->id, id, strlen(id) + 1) == 0) {
            return pe;
        }
    }
    return NULL;
}

struct p4_node *find_node_by_id(struct p4_file *pf, const char *id) {
    for (int i = 0; i < pf->n_nodes; i++) {
        struct p4_node *pn = &pf->nodes[i];
        if (strncmp(pn->id, id, strlen(id) + 1) == 0) {
            return pn;
        }
    }
    return NULL;
}

struct p4_node *find_node_by_pid(struct p4_file *pf, pid_t pid) {
    for (int i = 0; i < pf->n_nodes; i++) {
        struct p4_node *pn = &pf->nodes[i];
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
    //printf("\n");
    for (int i=0; i < pf->n_edges; i++) {
        struct p4_edge *pe = &pf->edges[i];
        struct p4_node *from = find_node_by_id(pf, pe->from);
        struct p4_node *to = find_node_by_id(pf, pe->to);

        if (strncmp(from->type, "EXEC\0", 5) == 0 && strncmp(to->type, "EXEC\0", 5) == 0) {
            // Each EXEC node should have a max of one pipe in, one pipe out
            if (from->out_pipe == NULL) {
                from->out_pipe = pipe_new();
            }
            if (to->in_pipe == NULL) {
                to->in_pipe = pipe_new();
            }
            if ((int)from->n_listening_edges == 0) {
                from->listening_edges = malloc(sizeof(*from->listening_edges));
                *from->listening_edges = pe;
                from->n_listening_edges++;
            }
            else {
                from->n_listening_edges++;
                from->listening_edges = realloc(from->listening_edges, (from->n_listening_edges) * sizeof(*from->listening_edges));
                if (from->listening_edges == NULL) {
                    // realloc failed, TODO free + exit
#ifdef HP4_DEBUG
                    fprintf(stderr, "realloc failed\n");
#endif /* HP4_DEBUG */
                }
                from->listening_edges[from->n_listening_edges - 1] = pe;
            }
        }
        else {
            // TODO support non-EXEC nodes
            free_p4_file(pf);
#ifdef HP4_DEBUG
            fprintf(stderr, "Non-EXEC nodes are not yet supported\n");
#endif /* HP4_DEBUG */
            return -1;
        }

        //printf("%d: %s, %s, %s\n", i, pe->id, pe->from, pe->to);
    }
    return 0;
}

int build_nodes(struct p4_file *pf, struct event_base *eb) {
    for (int i=0; i < pf->n_nodes; i++) {
        struct p4_node *pn = &pf->nodes[i];
        if (strncmp(pn->type, "EXEC\0", 5) == 0) {
            if (pn->in_pipe == NULL && pn->out_pipe == NULL) {
                // node is not joined to the graph, skip
#ifdef HP4_DEBUG
                fprintf(stderr, "disconnected node\n");
#endif /* HP4_DEBUG */
                continue;
            }
            if (pn->out_pipe != NULL) {
                struct event_args *ea = malloc(sizeof(*ea));
                ea->out_pipe = pn->out_pipe;
                ea->n_in_pipes = (int)pn->n_listening_edges;
                ea->bytes_spliced = calloc(pn->n_listening_edges,
                                           sizeof(*ea->bytes_spliced));
                ea->in_pipes = calloc(pn->n_listening_edges,
                                      sizeof(*ea->in_pipes));
                for (int j=0; j < (int)pn->n_listening_edges; j++) {
                    ea->bytes_spliced[j] = &pn->listening_edges[j]->bytes_spliced;
                    ea->in_pipes[j] = find_node_by_id(pf, pn->listening_edges[j]->to)->in_pipe;
                }
                struct event *readable = event_new(eb, pn->out_pipe->read_fd,
                                                   EV_READ|EV_PERSIST, pipeCb,
                                                   ea);
                event_add(readable, NULL);
            }
            pid_t pid = fork();
            if (pid == 0) { // child
                if (pn->out_pipe != NULL) {
                    dup2(pn->out_pipe->write_fd, STDOUT_FILENO);
                }
                if (pn->in_pipe != NULL) {
                    dup2(pn->in_pipe->read_fd, STDIN_FILENO);
                }
                for (int j = 0; j < pf->n_nodes; j++) {
                    struct p4_node *po = &pf->nodes[j];
                    if (po->in_pipe && close_pipe(po->in_pipe) < 0) {
                        perror("close in pipe\n");
                    }
                    if (po->out_pipe && close_pipe(po->out_pipe) < 0) {
                        perror("close out pipe\n");
                    }
                }
#ifdef HP4_DEBUG
                fprintf(stderr, "Node %s about to exec\n", pn->id);
#endif /* HP4_DEBUG */
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
#ifdef HP4_DEBUG
            fprintf(stderr, "Non-EXEC nodes not yet supported\n");
#endif /* HP4_DEBUG */
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
#ifdef HP4_DEBUG
        fprintf(stderr, "specify json filename\n");
#endif /* HP4_DEBUG */
        return 1;
    }

    struct p4_file *pf = p4_file_new(argv[1]);

    struct event_base *eb = event_base_new();

    struct event *sigintev = evsignal_new(eb, SIGINT, sigint_handler, eb);
    event_add(sigintev, NULL);

    struct sigchld_args *sa = malloc(sizeof(*sa));
    sa->pf = pf;
    sa->eb = eb;

    struct event *sigchldev = evsignal_new(eb, SIGCHLD, sigchld_handler, sa);
    event_add(sigchldev, NULL);

    if (pf == NULL) {
        return 1;
    }

    if (build_edges(pf) == -1) {
        return 1;
    }

    if (build_nodes(pf, eb) == -1) {
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
