#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "pipe.h"

struct pipe *pipe_new(char *port, char *edge_id) {
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
    new_pipe->port = port;
    new_pipe->edge_id = edge_id;
    new_pipe->bytes_written = 0u;
    new_pipe->visited = 0;
    return new_pipe;
}

struct pipe *find_pipe_by_edge_id(struct pipe_array *pa, char *edge_id) {
    for (int i = 0; i < (int)pa->length; i++) {
        if (strcmp(pa->pipes[i]->edge_id, edge_id) == 0) {
            return pa->pipes[i];
        }
    }
    return NULL;
}

struct pipe_array *pipe_array_new(void) {
    struct pipe_array *pa = malloc(sizeof(*pa));
    if (pa == NULL) {
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
        return -1;
    }
    if (++pa->length == 1u) {
        pa->pipes = malloc(sizeof(*pa->pipes));
        if (pa->pipes == NULL) {
            return -1;
        }
    }
    else {
        struct pipe **realloced_pipes = realloc(pa->pipes, pa->length * sizeof(*pa->pipes));
        if (realloced_pipes == NULL) {
            return -1;
        }
        pa->pipes = realloced_pipes;
    }
    pa->pipes[pa->length - 1] = pipe;
    return 0;
}

int pipe_array_has_pipe_with_port(struct pipe_array *pa, char *port) {
    if (pa->pipes == NULL) {
        return 0;
    }
    for (int i = 0; i < (int)pa->length; i++) {
        if (strcmp(pa->pipes[i]->port, port) == 0) {
            return 1;
        }
    }
    return 0;
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
    PRINT_DEBUG("close_pipe {%d %d}\n", pipe_to_close->read_fd, pipe_to_close->write_fd);
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

