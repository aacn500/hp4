#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <jansson.h>

#include "parser.h"

int create_stats_file(struct p4_file *pf) {
    json_t *json_byte_counters = json_object();
    if (json_byte_counters == NULL) {
        return -1;
    }

    for (int i = 0; i < (int)pf->edges->length; i++) {
        json_t *json_bytes = json_integer((json_int_t)pf->edges->edges[i]->bytes_spliced);
        if (json_bytes == NULL) {
            json_decref(json_byte_counters);
            return -1;
        }

        if (json_object_set_new(json_byte_counters, pf->edges->edges[i]->id, json_bytes) < 0) {
            json_decref(json_bytes);
            json_decref(json_byte_counters);
            return -1;
        }
    }

    if (json_dumpfd(json_byte_counters, STDOUT_FILENO, 0) < 0) {
        json_decref(json_byte_counters);
        return -1;
    }
    if (write(STDOUT_FILENO, "\n", 1) < 0) {
        json_decref(json_byte_counters);
        return -1;
    }

    json_decref(json_byte_counters);
    return 0;
}
