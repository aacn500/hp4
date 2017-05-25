#ifndef HP4_PARSER_H
#define HP4_PARSER_H

#include <jansson.h>

struct p4_node {
    char *id;
    char *cmd;
    char *type;
    char *subtype;
    char *name;

    struct pipe *in_pipe;
    struct pipe *out_pipe;

    struct p4_edge_array *listening_edges;

    pid_t pid;
};

struct p4_node_array {
    size_t length;
    struct p4_node **nodes;
};

struct p4_edge {
    char *id;
    char *from;
    char *to;

    ssize_t bytes_spliced;
};

struct p4_edge_array {
    size_t length;
    struct p4_edge **edges;
};

struct p4_file {
    struct p4_edge_array *edges;
    struct p4_node_array *nodes;
};

typedef char** p4_args_list_t;

p4_args_list_t args_list_new(char *args);

struct p4_file *p4_file_new(const char *filename);

void free_p4_file(struct p4_file *pf);

#endif // HP4_PARSER_H
