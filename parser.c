#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <jansson.h>

#include "parser.h"

/*
 * Splits a cmd such as `grep -v word` into an array of strings,
 * where the last element is NULL, as is expected by exec() functions.
 */
p4_args_list_t args_list_new(char *args) {
    char **res = NULL;
    char *p = strtok(args, " ");
    int n_spaces = 0;

    while (p) {
        res = realloc(res, sizeof(*res) * ++n_spaces);

        if (res == NULL)
            // realloc failed
            return NULL;

        res[n_spaces-1] = p;

        p = strtok(NULL, " ");
    }
    res = realloc(res, sizeof(*res) * (n_spaces + 1));
    res[n_spaces] = NULL;

    return res;
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

    // TODO all fields are compulsory

    if ((json_id = json_object_get(edge, "id"))) {
        parsed_edge->id = malloc((json_string_length(json_id) + 1) * sizeof(char));
        strcpy(parsed_edge->id, json_string_value(json_id));
    }
    else {
        parsed_edge->id = NULL;
    }
    if ((json_from = json_object_get(edge, "from"))) {
        parsed_edge->from = malloc((json_string_length(json_from) + 1) * sizeof(char));
        strcpy(parsed_edge->from, json_string_value(json_from));
    }
    else {
        parsed_edge->from = NULL;
    }
    if ((json_to = json_object_get(edge, "to"))) {
        parsed_edge->to = malloc((json_string_length(json_to) + 1) * sizeof(char));
        strcpy(parsed_edge->to, json_string_value(json_to));
    }
    else {
        parsed_edge->to = NULL;
    }
    json_decref(edge);
    return 0;
}

void free_p4_edge(struct p4_edge *pe) {
    free(pe->id);
    free(pe->from);
    free(pe->to);
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

    free(pn->in_pipe);
    free(pn->out_pipe);

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

void free_p4_file(struct p4_file *pf) {
    free_p4_node_array(pf->nodes);
    free_p4_edge_array(pf->edges);
    free(pf);
}
