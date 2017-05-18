#ifndef HP4_PARSER_H
#define HP4_PARSER_H

#include <jansson.h>

struct p4_node {
    char *id;
    char *cmd;
    char *type;
    char *subtype;
    char *name;
};

struct p4_edge {
    char *id;
    char *from;
    char *to;
};

struct p4_file {
    int n_nodes;
    int n_edges;
    struct p4_node *nodes;
    struct p4_edge *edges;
};

struct p4_file *p4_file_new(const char *filename);

void free_p4_file(struct p4_file *pf);

#endif // HP4_PARSER_H
