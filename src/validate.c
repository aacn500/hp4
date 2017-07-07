#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "parser.h"

bool validate_p4_file(struct p4_file *pf) {
    for (int i = 0; i < (int)pf->nodes->length; i++) {
        struct p4_node *node = p4_file_get_node(pf, i);

        /* Node has an id */
        if (node->id == NULL) {
            REPORT_ERROR("One or more nodes do not have an id.");
            return false;
        }

        /* Node has a type */
        if (node->type == NULL) {
            REPORT_ERRORF("Node %s does not have a type.\n", node->id);
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

        /* Edge has from node */
        if (edge->from == NULL) {
            REPORT_ERRORF("Edge %s does not have `from` node", edge->id);
            return false;
        }

        /* Edge has from_port */
        if (edge->from_port == NULL) {
            REPORT_ERRORF("edge %s does not have port for `from` node", edge->id);
            return false;
        }

        /* Edge has to node */
        if (edge->to == NULL) {
            REPORT_ERRORF("Edge %s does not have `to` node", edge->id);
            return false;
        }

        /* Edge has to_port */
        if (edge->to_port == NULL) {
            REPORT_ERRORF("edge %s does not have port for `to` node", edge->id);
            return false;
        }

        bool found_from = false;
        bool found_to = false;

        for (int k = 0; k < (int)pf->nodes->length; k++) {
            struct p4_node *node = p4_file_get_node(pf, k);
            if (strcmp(edge->from, node->id) == 0)
                found_from = true;

            if (strcmp(edge->to, node->id) == 0)
                found_to = true;
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

    return true;
}
