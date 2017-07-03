#include "config.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "pipe.h"

int pipe_append_edge_id(struct pipe *p, const char *edge_id) {
    char **new_edge_ids = realloc(p->edge_ids, p->n_edge_ids + 1);
    if (new_edge_ids == NULL)
        return -1;
    p->edge_ids = new_edge_ids;
    size_t id_len = strlen(edge_id) + 1;
    p->edge_ids[p->n_edge_ids] = malloc(id_len);
    if (p->edge_ids[p->n_edge_ids] == NULL) {
        return -1;
    }
    strcpy(p->edge_ids[p->n_edge_ids], edge_id);
    ++p->n_edge_ids;
    return 0;
}

struct pipe *pipe_new(char *port, char *edge_id) {
    struct pipe *new_pipe = malloc(sizeof(*new_pipe));
    if (new_pipe == NULL) {
        REPORT_ERROR(strerror(errno));
        return NULL;
    }

    int fds[2] = {0, 0};
    if (pipe(fds) < 0) {
        REPORT_ERROR(strerror(errno));
        free(new_pipe);
        return NULL;
    }

    PRINT_DEBUG("pipe_new {%d %d}\n", fds[0], fds[1]);

    new_pipe->n_edge_ids = 0;
    new_pipe->edge_ids = NULL;

    if (pipe_append_edge_id(new_pipe, edge_id) < 0) {
        free(new_pipe);
        return NULL;
    }

    new_pipe->read_fd = fds[0];
    new_pipe->read_fd_is_open = true;
    new_pipe->write_fd = fds[1];
    new_pipe->write_fd_is_open = true;
    new_pipe->port = port;
    new_pipe->bytes_written = 0u;
    new_pipe->visited = 0;
    return new_pipe;
}

bool pipe_has_edge_id(struct pipe *p, const char *edge_id) {
    for (int i = 0; i < p->n_edge_ids; i++) {
        if (strcmp(edge_id, p->edge_ids[i]) == 0) {
            return true;
        }
    }
    return false;
}

struct pipe *find_pipe_by_edge_id(struct pipe_array *pa, char *edge_id) {
    for (int i = 0; i < (int)pa->length; i++) {
        if (pipe_has_edge_id(pa->pipes[i], edge_id) == 1) {
            return pa->pipes[i];
        }
    }
    return NULL;
}

struct pipe_array *pipe_array_new(void) {
    struct pipe_array *pa = malloc(sizeof(*pa));
    if (pa == NULL) {
        REPORT_ERROR(strerror(errno));
        return NULL;
    }
    pa->length = 0u;
    pa->pipes = NULL;
    return pa;
}

int pipe_array_append_new(struct pipe_array *pa, char *port, char *edge_id) {
    return pipe_array_append(pa, pipe_new(port, edge_id));
}

int pipe_array_append(struct pipe_array *pa, struct pipe *pipe) {
    if (pipe == NULL) {
        REPORT_ERROR("Cannot append NULL pipe");
        return -1;
    }
    if (++pa->length == 1u) {
        pa->pipes = malloc(sizeof(*pa->pipes));
        if (pa->pipes == NULL) {
            REPORT_ERROR(strerror(errno));
            return -1;
        }
    }
    else {
        struct pipe **realloced_pipes = realloc(pa->pipes, pa->length * sizeof(*pa->pipes));
        if (realloced_pipes == NULL) {
            REPORT_ERROR(strerror(errno));
            return -1;
        }
        pa->pipes = realloced_pipes;
    }
    pa->pipes[pa->length - 1] = pipe;
    return 0;
}

bool pipe_array_has_pipe_with_port(struct pipe_array *pa, char *port) {
    if (pa->pipes == NULL) {
        return false;
    }
    for (int i = 0; i < (int)pa->length; i++) {
        if (strcmp(pa->pipes[i]->port, port) == 0) {
            return true;
        }
    }
    return false;
}

struct pipe *pipe_array_find_pipe_with_port(struct pipe_array *pa, char *port) {
    if (pa->pipes == NULL) {
        return NULL;
    }
    for (int i = 0; i < (int)pa->length; i++) {
        if (strcmp(pa->pipes[i]->port, port) == 0) {
            return pa->pipes[i];
        }
    }
    return NULL;
}

int pipe_array_close(struct pipe_array *pa) {
    int res = 0;
    for (size_t i = 0u; i < pa->length; i++) {
        res |= close_pipe(pa->pipes[i]);
    }
    return res;
}

int pipe_array_free(struct pipe_array *pa) {
    int res = pipe_array_close(pa);
    if (res != 0) {
        return res;
    }

    free(pa->pipes);
    free(pa);
    return res;
}

int close_pipe(struct pipe *pipe_to_close) {
    PRINT_DEBUG("close_pipe {%d %d}\n", pipe_to_close->read_fd,
                                        pipe_to_close->write_fd);
    int result = 0;
    if (pipe_to_close->read_fd_is_open) {
        int close_read = close(pipe_to_close->read_fd);
        if (close_read == 0)
            pipe_to_close->read_fd_is_open = false;
        else if (close_read < 0)
            PRINT_DEBUG("Closing pipe read_fd failed: %s\n",
                          strerror(errno));
        result |= close_read;
    }
    if (pipe_to_close->write_fd_is_open) {
        int close_write = close(pipe_to_close->write_fd);
        if (close_write == 0)
            pipe_to_close->write_fd_is_open = false;
        else if (close_write < 0)
            PRINT_DEBUG("Closing pipe write_fd failed: %s\n",
                          strerror(errno));
        result |= close_write;
    }

    return result;
}
