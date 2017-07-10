#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "parser.h"

bool validate_p4_file(struct p4_file *pf) {
    /* There is at least 1 node and edge. */
    if (pf->nodes->length == 0u) {
        REPORT_ERROR("Graph has no nodes.");
        return false;
    }
    if (pf->edges->length == 0u) {
        REPORT_ERROR("Graph has no edges.");
        return false;
    }

    for (int i = 0; i < (int)pf->nodes->length; i++) {
        struct p4_node *node = p4_file_get_node(pf, i);

        /* Node has an id */
        if (node->id == NULL) {
            REPORT_ERROR("One or more nodes do not have an id.");
            return false;
        }
        else if (strchr(node->id, ' ')) {
            REPORT_ERRORF("Node '%s' has a space in its id.", node->id);
            return false;
        }

        /* Node has a type */
        if (node->type == NULL) {
            REPORT_ERRORF("Node %s does not have a type.\n", node->id);
            return false;
        }
        else if (strchr(node->type, ' ')) {
            REPORT_ERRORF("Node %s has a space in its type.", node->id);
            return false;
        }

        /* Node with type EXEC has a cmd */
        if (strcmp(node->type, "EXEC") == 0 && node->cmd == NULL) {
            REPORT_ERRORF("Node %s is type EXEC but does not have a cmd.\n", node->id);
            return false;
        }

    }

    for (int j = 0; j < (int)pf->edges->length; j++) {
        struct p4_edge *edge = p4_file_get_edge(pf, j);

        /* Edge has an id */
        if (edge->id == NULL) {
            REPORT_ERROR("Edge does not have an id.");
            return false;
        }
        else if (strchr(edge->id, ' ')) {
            REPORT_ERRORF("Edge '%s' has a space in its id.", edge->id);
            return false;
        }

        /* Edge has from node */
        if (edge->from == NULL) {
            REPORT_ERRORF("Edge %s does not have `from` node", edge->id);
            return false;
        }
        else if (strchr(edge->from, ' ')) {
            REPORT_ERRORF("Edge %s has a space in its from node specifier.", edge->id);
            return false;
        }

        /* Edge has from_port */
        if (edge->from_port == NULL) {
            REPORT_ERRORF("edge %s does not have port for `from` node", edge->id);
            return false;
        }
        else if (strchr(edge->from_port, ' ')) {
            REPORT_ERRORF("Edge %s has a space in its from node port specifier.", edge->id);
            return false;
        }

        /* Edge has to node */
        if (edge->to == NULL) {
            REPORT_ERRORF("Edge %s does not have `to` node", edge->id);
            return false;
        }
        else if (strchr(edge->to, ' ')) {
            REPORT_ERRORF("Edge %s has a space in its to node specifier.", edge->id);
            return false;
        }

        /* Edge has to_port */
        if (edge->to_port == NULL) {
            REPORT_ERRORF("edge %s does not have port for `to` node", edge->id);
            return false;
        }
        else if (strchr(edge->to_port, ' ')) {
            REPORT_ERRORF("Edge %s has a space in its to node port specifier.", edge->id);
            return false;
        }

        bool found_from = false;
        bool found_to = false;
        /* Both `from` and `to` nodes exist. */
        for (int k = 0; k < (int)pf->nodes->length; k++) {
            struct p4_node *node = p4_file_get_node(pf, k);
            if (strcmp(edge->from, node->id) == 0) {
                found_from = true;
                /* if port is not "-", port exists at least once in node->cmd */
                if (strcmp(edge->from_port, STDIO_PORT) != 0) {
                    char *port_loc = strstr(node->cmd, edge->from_port);
                    if (port_loc == NULL) {
                        REPORT_ERRORF("Edge %s has port named %s from node %s,"
                               " but this was not found in node's cmd, %s.",
                               edge->id, edge->from_port, edge->from, node->cmd);
                        return false;
                    }
                    /* port exists at most once in node->cmd */
                    else if (strstr(port_loc+1, edge->from_port) != NULL) {
                        REPORT_ERRORF("Port %s occurs multiple times in cmd for "
                                "node %s.", edge->from_port, node->cmd);
                        return false;
                    }
                }
            }

            if (strcmp(edge->to, node->id) == 0) {
                found_to = true;
                /* if port is not "-", port exists at least once in node->cmd */
                if (strcmp(edge->to_port, STDIO_PORT) != 0) {
                    char *port_loc = strstr(node->cmd, edge->to_port);
                    if (port_loc == NULL) {
                        REPORT_ERRORF("Edge %s has port named %s to node %s,"
                               " but this was not found in node's cmd, %s.",
                               edge->id, edge->to_port, edge->to, node->cmd);
                        return false;
                    }
                    /* port exists at most once in node->cmd */
                    else if (strstr(port_loc+1, edge->to_port) != NULL) {
                        REPORT_ERRORF("Port %s occurs multiple times in cmd for "
                                "node %s.", edge->to_port, node->cmd);
                        return false;
                    }
                }
            }
        }

        if (!found_from) {
            REPORT_ERRORF("Edge %s has from node %s, but that does not exist!",
                    edge->id, edge->from);
            return false;
        }
        if (!found_to) {
            REPORT_ERRORF("Edge %s has to node %s, but that does not exist!",
                    edge->id, edge->to);
            return false;
        }
    }

    /* Node is connected to the graph. */
    for (int l = 0; l < (int)pf->nodes->length; l++) {
        struct p4_node *node = p4_file_get_node(pf, l);

        bool found = false;
        for (int m = 0; m < (int)pf->edges->length; m++) {
            struct p4_edge *edge = p4_file_get_edge(pf, m);

            if (strcmp(node->id, edge->from) == 0 || strcmp(node->id, edge->to) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            REPORT_ERRORF("Could not find an edge which connects to node %s.", node->id);
            return false;
        }
    }

    return true;
}
