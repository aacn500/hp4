#ifndef HP4_PIPE_H
#define HP4_PIPE_H

#include <stdbool.h>

struct pipe {
    int read_fd;
    bool read_fd_is_open;
    int write_fd;
    bool write_fd_is_open;
    char *port;
    char **edge_ids;
    int n_edge_ids;
    size_t bytes_written;
    /* Flag whether writable callback has fired for this pipe */
    bool visited;
};

struct pipe_array {
    struct pipe **pipes;
    size_t length;
};

struct pipe *get_pipe(struct pipe_array *pa, int idx);

int pipe_append_edge_id(struct pipe *p, const char *edge_id);

bool pipe_has_edge_id(struct pipe *p, const char *edge_id);

struct pipe *find_pipe_by_edge_id(struct pipe_array *pa, char *edge_id);

struct pipe_array *pipe_array_new(void);

int pipe_array_append_new(struct pipe_array *pa, char *port, char *edge_id);

int pipe_array_append(struct pipe_array *pa, struct pipe *pipe);

bool pipe_array_has_pipe_with_port(struct pipe_array *pa, char *port);

struct pipe *pipe_array_find_pipe_with_port(struct pipe_array *pa, char *port);

int pipe_array_close(struct pipe_array *pa);

int pipe_array_free(struct pipe_array *pa);

int close_pipe(struct pipe *pipe_to_close);

#endif /* HP4_PIPE_H */
