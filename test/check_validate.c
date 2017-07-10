#include <stdbool.h>

#include <check.h>

#include "../src/parser.h"
#include "../src/validate.h"

START_TEST(test_validate) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    ck_assert(pf != NULL);

    bool valid;
    valid = validate_p4_file(pf);
    ck_assert(valid);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_no_edges) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/no_edges.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_no_nodes) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/no_nodes.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_no_spaces) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/spaces_node_id.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_node_type.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_edge_id.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_edge_from.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_edge_from_port.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_edge_to.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/spaces_edge_to_port.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_edge_missing_attributes) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/edge_no_id.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/edge_no_from.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/edge_no_to.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    /* Ports will default to "-" by p4_file_new, deliberately break them */

    pf = p4_file_new("data/basic.json");
    ck_assert(pf != NULL);
    free(pf->edges->edges[0]->from_port);
    pf->edges->edges[0]->from_port = NULL;
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/basic.json");
    ck_assert(pf != NULL);
    free(pf->edges->edges[0]->to_port);
    pf->edges->edges[0]->to_port = NULL;
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_node_missing_attributes) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/node_missing_id.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/node_missing_type.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/node_exec_missing_cmd.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_missing_node) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/node_from_missing.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/node_to_missing.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_unconnected_node) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/node_unconnected.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_port_not_in_cmd) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/port_missing_from.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/port_missing_to.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

START_TEST(test_multiple_ports) {
    struct p4_file *pf;
    bool valid;

    pf = p4_file_new("data/validate/edge_from_multi_ports.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);

    pf = p4_file_new("data/validate/edge_to_multi_ports.json");
    ck_assert(pf != NULL);
    valid = validate_p4_file(pf);
    ck_assert(!valid);
    free_p4_file(pf);
}
END_TEST

Suite *validate_suite(void) {
    Suite *s = suite_create("validate");

    TCase *tc_validate = tcase_create("validate");
    tcase_add_test(tc_validate, test_validate);
    tcase_add_test(tc_validate, test_no_edges);
    tcase_add_test(tc_validate, test_no_nodes);
    tcase_add_test(tc_validate, test_no_spaces);
    tcase_add_test(tc_validate, test_edge_missing_attributes);
    tcase_add_test(tc_validate, test_node_missing_attributes);
    tcase_add_test(tc_validate, test_missing_node);
    tcase_add_test(tc_validate, test_unconnected_node);
    tcase_add_test(tc_validate, test_port_not_in_cmd);
    tcase_add_test(tc_validate, test_multiple_ports);

    suite_add_tcase(s, tc_validate);

    return s;
}
