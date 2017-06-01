#ifndef HP4_PIPE_H
#define HP4_PIPE_H

struct pipe {
    int read_fd;
    int write_fd;
    char *port;
    char *edge_id;
    size_t bytes_written;
};

struct pipe_array {
    struct pipe **pipes;
    size_t length;
};

struct pipe_array *pipe_array_new(void);

int pipe_array_append_new(struct pipe_array *pa, char *port, char *edge_id);

int pipe_array_append(struct pipe_array *pa, struct pipe *pipe);

int pipe_array_has_pipe_with_port(struct pipe_array *pa, char *port);

int pipe_array_close(struct pipe_array *pa);

int pipe_array_free(struct pipe_array *pa);

int close_pipe(struct pipe *pipe_to_close);

#endif /* HP4_PIPE_H */
