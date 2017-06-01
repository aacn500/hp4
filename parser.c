#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <jansson.h>

#include "parser.h"
#include "pipe.h"

#define PORT_DELIMITER ':'

/*
 * Splits a cmd such as `grep -v word` into an array of strings,
 * where the last element is NULL, as is expected by exec() functions.
 */
struct p4_args *args_list_new(const char *args) {
    char *tmp_args = malloc((strlen(args) + 1) * sizeof(*tmp_args));
    strcpy(tmp_args, args);
    char **argv = NULL;
    char *p = strtok(tmp_args, " ");
    int argc = 0;

    while (p) {
        argv = realloc(argv, sizeof(*argv) * ++argc);

        if (argv == NULL)
            // realloc failed
            return NULL;

        argv[argc-1] = p;

        p = strtok(NULL, " ");
    }
    argv = realloc(argv, sizeof(*argv) * (argc + 1));
    argv[argc] = NULL;

    struct p4_args *pa = malloc(sizeof(*pa));
    pa->argc = argc;
    pa->argv = argv;

    return pa;
}

char **parse_edge_string(const char *edge_ro) {
    char **edge_strings = malloc(2 * sizeof(*edge_strings));
    char *port_ro = strchr(edge_ro, PORT_DELIMITER);
    if (port_ro == NULL) {
        // Did not find any instance of PORT_DELIMITER;
        // use default of "-", to represent std(in|out).
        edge_strings[0] = malloc((strlen(edge_ro) + 1) * sizeof(**edge_strings));
        strcpy(edge_strings[0], edge_ro);
        edge_strings[1] = malloc((sizeof("-")));
        strcpy(edge_strings[1], "-");
        return edge_strings;
    }
    port_ro++;
    // there should only be one instance of PORT_DELIMITER in the string
    if (strchr(port_ro, PORT_DELIMITER) != NULL) {
        free(edge_strings);
        return NULL;
    }
    size_t id_len = port_ro - edge_ro;
    size_t port_len = strlen(port_ro) + 1;
    edge_strings[0] = malloc(id_len * sizeof(**edge_strings));
    edge_strings[1] = malloc(port_len * sizeof(**edge_strings));
    strncpy(edge_strings[0], edge_ro, id_len);
    edge_strings[0][id_len-1] = 0;
    strcpy(edge_strings[1], port_ro);
    return edge_strings;
}

int parse_p4_edge(json_t *edge, struct p4_edge *parsed_edge) {
    json_incref(edge);
    if (!json_is_object(edge)) {
        json_decref(edge);
        return -1;
    }

    json_t *json_id;
    json_t *json_from;
    json_t *json_to;

    parsed_edge->bytes_spliced = 0l;

    // TODO all fields are compulsory

    if ((json_id = json_object_get(edge, "id"))) {
        parsed_edge->id = malloc((json_string_length(json_id) + 1) * sizeof(char));
        strcpy(parsed_edge->id, json_string_value(json_id));
    }
    else {
        parsed_edge->id = NULL;
    }
    if ((json_from = json_object_get(edge, "from"))) {
        const char *from_ro = json_string_value(json_from);
        char **from_parsed = parse_edge_string(from_ro);
        if (from_parsed != NULL) {
            parsed_edge->from = from_parsed[0];
            parsed_edge->from_port = from_parsed[1];
            free(from_parsed);
        }
        else {
            // TODO error when parsing the edge; found multiple ports
            parsed_edge->from = NULL;
            parsed_edge->from_port = NULL;
        }
    }
    else {
        parsed_edge->from = NULL;
        parsed_edge->from_port = NULL;
    }
    if ((json_to = json_object_get(edge, "to"))) {
        const char *to_ro = json_string_value(json_to);
        char **to_parsed = parse_edge_string(to_ro);
        if (to_parsed != NULL) {
            parsed_edge->to = to_parsed[0];
            parsed_edge->to_port = to_parsed[1];
            free(to_parsed);
        }
        else {
            // TODO error when parsing the edge; found multiple ports
            parsed_edge->to = NULL;
            parsed_edge->to_port = NULL;
        }
    }
    else {
        parsed_edge->to = NULL;
        parsed_edge->to_port = NULL;
    }
    json_decref(edge);
    return 0;
}

void free_p4_edge(struct p4_edge *pe) {
    free(pe->id);
    free(pe->from);
    free(pe->from_port);
    free(pe->to);
    free(pe->to_port);
    free(pe);
}

void free_p4_edge_array(struct p4_edge_array *edges) {
    for (size_t i = 0u; i < edges->length; i++) {
        free_p4_edge(edges->edges[i]);
    }
    free(edges->edges);
    free(edges);
}

struct p4_edge_array *p4_edge_array_new(json_t *edges, size_t length) {
    json_incref(edges);
    if (!json_is_array(edges)) {
        json_decref(edges);
        return NULL;
    }

    struct p4_edge_array *edge_arr = malloc(sizeof(*edge_arr));
    edge_arr->length = length;
    edge_arr->edges = calloc(edge_arr->length, sizeof(*edge_arr->edges));

    for (int i = 0; i < (int)edge_arr->length; i++) {
        edge_arr->edges[i] = calloc(1u, sizeof(**edge_arr->edges));

        if (parse_p4_edge(json_array_get(edges, i), edge_arr->edges[i]) < 0) {
            fprintf(stderr, "Failed to parse edge\n");
            free_p4_edge_array(edge_arr);
            json_decref(edges);
            return NULL;
        }
    }

    json_decref(edges);
    return edge_arr;
}

void free_p4_node(struct p4_node *pn) {
    free(pn->id);
    free(pn->type);
    free(pn->subtype);
    free(pn->cmd);
    free(pn->name);

    pipe_array_free(pn->in_pipes);
    pipe_array_free(pn->out_pipes);

    if (pn->listening_edges != NULL)
        free(pn->listening_edges->edges);
    free(pn->listening_edges);
    free(pn);
}

void free_p4_node_array(struct p4_node_array *nodes) {
    for (size_t i = 0u; i < nodes->length; i++) {
        free_p4_node(nodes->nodes[i]);
    }
    free(nodes->nodes);
    free(nodes);
}

int parse_p4_node(json_t *node, struct p4_node *parsed_node) {
    json_incref(node);
    if (!json_is_object(node)) {
        json_decref(node);
        return -1;
    }

    json_t *json_id;
    json_t *json_type;
    json_t *json_subtype;
    json_t *json_cmd;
    json_t *json_name;

    // TODO id and type are compulsory

    if ((json_id = json_object_get(node, "id"))) {
        parsed_node->id = malloc((json_string_length(json_id)+1) * sizeof(char));
        strcpy(parsed_node->id, json_string_value(json_id));
    }
    else {
        parsed_node->id = NULL;
    }
    if ((json_type = json_object_get(node, "type"))) {
        parsed_node->type = malloc((json_string_length(json_type)+1) * sizeof(char));
        strcpy(parsed_node->type, json_string_value(json_type));
    }
    else {
        parsed_node->type = NULL;
    }
    if ((json_subtype = json_object_get(node, "subtype"))) {
        parsed_node->subtype = malloc((json_string_length(json_subtype)+1) * sizeof(char));
        strcpy(parsed_node->subtype, json_string_value(json_subtype));
    }
    else {
        parsed_node->subtype = NULL;
    }
    if ((json_cmd = json_object_get(node, "cmd"))) {
        parsed_node->cmd = malloc((json_string_length(json_cmd)+1) * sizeof(char));
        strcpy(parsed_node->cmd, json_string_value(json_cmd));
    }
    else {
        parsed_node->cmd = NULL;
    }
    if ((json_name = json_object_get(node, "name"))) {
        parsed_node->name = malloc((json_string_length(json_name)+1) * sizeof(char));
        strcpy(parsed_node->name, json_string_value(json_name));
    }
    else {
        parsed_node->name = NULL;
    }

    parsed_node->in_pipes = pipe_array_new();
    parsed_node->out_pipes = pipe_array_new();

    /* Validate unpacked object */
    /* Subtype can only be DUMMY, and only exist when type == RAFILE */
    int valid_subtype = (parsed_node->subtype == NULL ||
                         (strncmp(parsed_node->subtype, "DUMMY\0", 6) == 0 &&
                          strncmp(parsed_node->type, "RAFILE\0", 7) == 0)) ? 0 : -1;
    /* cmd must be defined iff type == EXEC */
    int valid_cmd =
        ((parsed_node->cmd != NULL) == (strncmp(parsed_node->type, "EXEC\0", 5) == 0)) ? 0 : -1;
    /* name must be defined iff type == *FILE */
    const char *type_suffix = strrchr(parsed_node->type, 'F');
    int valid_name =
        (parsed_node->name != NULL) == (type_suffix != NULL && strncmp(type_suffix, "FILE\0", 5) == 0) ? 0 : -1;

    if (!(valid_subtype == 0 && valid_cmd == 0 && valid_name == 0)) {
        fprintf(stderr, "%s\n", parsed_node->cmd);
        fprintf(stderr, "subtype: %d, cmd: %d, name: %d\n", valid_subtype, valid_cmd, valid_name);
        fprintf(stderr, "node was invalid\n");
        free_p4_node(parsed_node);
        parsed_node = NULL;
        json_decref(node);
        return -1;
    }

    json_decref(node);
    return 0;
}

struct p4_node_array *p4_node_array_new(json_t *nodes, size_t length) {
    json_incref(nodes);
    if (!json_is_array(nodes)) {
        json_decref(nodes);
        return NULL;
    }

    struct p4_node_array *node_arr = malloc(sizeof(*node_arr));
    node_arr->length = length;
    node_arr->nodes = calloc(node_arr->length, sizeof(*node_arr->nodes));

    for (int i = 0; i < (int)node_arr->length; i++) {
        node_arr->nodes[i] = calloc(1u, sizeof(**node_arr->nodes));

        if (parse_p4_node(json_array_get(nodes, i), node_arr->nodes[i]) < 0) {
            fprintf(stderr, "Failed to parse node\n");

            free(node_arr);
            json_decref(nodes);
            return NULL;
        }
    }
    json_decref(nodes);
    return node_arr;
}

struct p4_file *p4_file_new(const char *filename) {
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (root == NULL) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        json_decref(root);
        return NULL;
    }
    if (!json_is_object(root)) {
        fprintf(stderr, "Error: Root is not an object\n");
        json_decref(root);
        return NULL;
    }

    json_t *nodes = json_object_get(root, "nodes");
    if (!json_is_array(nodes)) {
        fprintf(stderr, "error: nodes is not an array\n");
        json_decref(root);
        return NULL;
    }
    json_t *edges = json_object_get(root, "edges");
    if (!json_is_array(edges)) {
        fprintf(stderr, "error: edges is not an array\n");
        json_decref(root);
        return NULL;
    }

    struct p4_file *pf = malloc(sizeof(*pf));

    pf->edges = p4_edge_array_new(edges, json_array_size(edges));
    if (pf->edges == NULL) {
        free_p4_file(pf);
        json_decref(root);
        return NULL;
    }
    pf->nodes = p4_node_array_new(nodes, json_array_size(nodes));
    if (pf->nodes == NULL) {
        free_p4_file(pf);
        json_decref(root);
        return NULL;
    }
    json_decref(root);
    return pf;
}

int validate_p4_file(struct p4_file *pf) {
    // FIXME need to expand validations
    for (int i = 0; i < (int)pf->nodes->length; i++) {
        struct p4_node *node = pf->nodes->nodes[i];
        if (node->id == NULL) {
            fprintf(stderr, "One or more nodes do not have an id.\n");
            return 0;
        }
        if (node->type == NULL) {
            fprintf(stderr, "Node %s does not have a type.\n", node->id);
            return 0;
        }
        if (strcmp(node->type, "EXEC") == 0 && node->cmd == NULL) {
            fprintf(stderr, "Node %s is type EXEC but does not have a cmd.\n", node->id);
            return 0;
        }
    }
    return 1;
}

void free_p4_file(struct p4_file *pf) {
    free_p4_node_array(pf->nodes);
    free_p4_edge_array(pf->edges);
    free(pf);
}
