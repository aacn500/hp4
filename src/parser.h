#ifndef HP4_PARSER_H
#define HP4_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include <jansson.h>

#include "event_handlers.h"
#include "pipe.h"

struct p4_node {
    char *id;
    char *cmd;
    char *type;
    char *subtype;
    char *name;

    struct pipe_array *in_pipes;
    struct pipe_array *out_pipes;

    struct event_array *writable_events;

    struct p4_edge_array *listening_edges;

    pid_t pid;
    bool ended;
};

struct p4_node_array {
    size_t length;
    struct p4_node **nodes;
};

struct p4_edge {
    char *id;
    char *from;
    char *from_port;
    char *to;
    char *to_port;

    // Potentially splicing multiple GBs; ensure 64-bit counter
    int64_t bytes_spliced;
};

struct p4_edge_array {
    size_t length;
    struct p4_edge **edges;
};

struct p4_file {
    struct p4_edge_array *edges;
    struct p4_node_array *nodes;
};

struct p4_node *find_node_by_id(struct p4_file *pf, const char *id);

struct p4_node *find_node_by_pid(struct p4_file *pf, pid_t pid);

struct p4_edge *find_edge_by_id(struct p4_file *pf, const char *edge_id);

struct p4_node *find_from_node_by_edge_id(struct p4_file *pf, const char *edge_id);

struct p4_node *find_to_node_by_edge_id(struct p4_file *pf, const char *edge_id);

struct p4_node *get_node(struct p4_node_array *nodes, int idx);

struct p4_node *p4_file_get_node(struct p4_file *pf, int idx);

struct p4_edge *get_edge(struct p4_edge_array *edges, int idx);

struct p4_edge *p4_file_get_edge(struct p4_file *pf, int idx);

struct p4_file *p4_file_new(const char *filename);

void free_p4_file(struct p4_file *pf);

#endif /* HP4_PARSER_H */
