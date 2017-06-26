#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "pipe.h"

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
    new_pipe->read_fd = fds[0];
    new_pipe->read_fd_is_open = 1;
    new_pipe->write_fd = fds[1];
    new_pipe->write_fd_is_open = 1;
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
    PRINT_DEBUG("close_pipe {%d %d}\n", pipe_to_close->read_fd,
                                        pipe_to_close->write_fd);
    int result = 0;
    if (pipe_to_close->read_fd_is_open == 1) {
        int close_read = close(pipe_to_close->read_fd);
        if (close_read == 0)
            pipe_to_close->read_fd_is_open = 0;
        else if (close_read < 0)
            PRINT_DEBUG("Closing pipe read_fd failed: %s\n",
                          strerror(errno));
        result |= close_read;
    }
    if (pipe_to_close->write_fd_is_open == 1) {
        int close_write = close(pipe_to_close->write_fd);
        if (close_write == 0)
            pipe_to_close->write_fd_is_open = 0;
        else if (close_write < 0)
            PRINT_DEBUG("Closing pipe write_fd failed: %s\n",
                          strerror(errno));
        result |= close_write;
    }

    return result;
}
