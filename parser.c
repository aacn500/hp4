#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <jansson.h>

#include "parser.h"

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
}

void free_p4_edge_array(struct p4_edge *edges_arr, int n_edges) {
    for (int i = 0; i < n_edges; i++) {
        free_p4_edge(&edges_arr[i]);
    }
    free(edges_arr);
}

struct p4_edge *p4_edges_array_new(json_t *edges) {
    json_incref(edges);
    if (!json_is_array(edges)) {
        json_decref(edges);
        return NULL;
    }
    size_t n_edges = json_array_size(edges);
    struct p4_edge *edges_array = malloc(n_edges * sizeof(*edges_array));

    for (int i = 0; i < n_edges; i++) {
        if (parse_p4_edge(json_array_get(edges, i), &edges_array[i]) < 0) {
            fprintf(stderr, "Failed to parse edge\n");
            free(edges_array);
            json_decref(edges);
            return NULL;
        }
    }

    json_decref(edges);
    return edges_array;
}

void free_p4_node(struct p4_node *pn) {
    free(pn->id);
    free(pn->type);
    free(pn->subtype);
    free(pn->cmd);
    free(pn->name);
}

void free_p4_node_array(struct p4_node *nodes_array, int n_nodes) {
    for (int i = 0; i < n_nodes; i++) {
        free_p4_node(&nodes_array[i]);
    }
    free(nodes_array);
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
                         (strcmp(parsed_node->subtype, "DUMMY") == 0 &&
                          strcmp(parsed_node->type, "RAFILE") == 0)) ? 0 : -1;
    /* cmd must be defined iff type == EXEC */
    int valid_cmd =
        ((parsed_node->cmd != NULL) == (strcmp(parsed_node->type, "EXEC") == 0)) ? 0 : -1;
    /* name must be defined iff type == *FILE */
    const char *type_suffix = strrchr(parsed_node->type, 'F');
    int valid_name = (parsed_node->name != NULL) == (type_suffix != NULL && strcmp(type_suffix, "FILE") == 0) ? 0 : -1;

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

struct p4_node *p4_nodes_array_new(json_t *nodes) {
    json_incref(nodes);
    if (!json_is_array(nodes)) {
        json_decref(nodes);
        return NULL;
    }
    size_t n_nodes = json_array_size(nodes);

    struct p4_node *nodes_array = malloc(n_nodes * sizeof(*nodes_array));

    for (int i = 0; i < n_nodes; i++) {
        if (parse_p4_node(json_array_get(nodes, i), &nodes_array[i]) < 0) {
            fprintf(stderr, "Failed to parse node\n");
            free(nodes_array);
            json_decref(nodes);
            return NULL;
        }
    }
    json_decref(nodes);
    return nodes_array;
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

    pf->n_nodes = (int)json_array_size(nodes);
    pf->n_edges = (int)json_array_size(edges);

    pf->nodes = p4_nodes_array_new(nodes);
    if (pf->nodes == NULL) {
        free(pf);
        json_decref(root);
        return NULL;
    }
    pf->edges = p4_edges_array_new(edges);
    if (pf->edges == NULL) {
        free(pf);
        json_decref(root);
        return NULL;
    }
    json_decref(root);
    return pf;
}

void free_p4_file(struct p4_file *pf) {
    free_p4_node_array(pf->nodes, pf->n_nodes);
    free_p4_edge_array(pf->edges, pf->n_edges);
    free(pf);
}
/* Uncomment this to get a running example of parser.c
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "specify json filename\n");
        return 1;
    }

    struct p4_file *pf = p4_file_new(argv[1]);
    if (pf == NULL) {
        return 1;
    }

    for (int i = 0; i < pf->n_nodes; i++) {
        struct p4_node *pn = &pf->nodes[i];
        printf("%d: %s", i, pn->id);
        printf(", type: %s", pn->type);
        if (pn->subtype != NULL)
            printf(", subtype: %s", pn->subtype);
        if (pn->cmd != NULL)
            printf(", cmd: %s", pn->cmd);
        if (pn->name != NULL)
            printf(", name: %s", pn->name);
        printf("\n");
    }

    for (int i = 0; i < pf->n_edges; i++) {
        struct p4_edge *pe = &pf->edges[i];
        if (pe == NULL) {
            fprintf(stderr, "error: pe was not parsed :(\n");
            return 1;
        }
        printf("%d: %s, from %s to %s\n", i, pe->id, pe->from, pe->to);
    }

    free_p4_file(pf);

    return 0;
}
*/
