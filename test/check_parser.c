#include <stdlib.h>

#include <check.h>

#include "../src/parser.h"

START_TEST(fail_parse_broken_json) {
    struct p4_file *pf;
    pf = p4_file_new("data/broken.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/array.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/no_nodes.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/no_edges.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/multiple_ports.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/edge_no_from.json");
    ck_assert(pf == NULL);

    pf = p4_file_new("data/edge_no_to.json");
    ck_assert(pf == NULL);
}
END_TEST

START_TEST(parse_basic_file) {
    /* Two EXEC nodes, one connecting edge with no ports. */

    struct p4_file *pf = p4_file_new("data/basic.json");
    ck_assert(pf != NULL);
    ck_assert_uint_eq(pf->nodes->length, 2);
    ck_assert_uint_eq(pf->edges->length, 1);

    /* nodes */
    struct p4_node *pn_cat = pf->nodes->nodes[0];
    ck_assert_str_eq(pn_cat->id, "cat");
    ck_assert_str_eq(pn_cat->type, "EXEC");
    ck_assert_str_eq(pn_cat->cmd, "cat");

    struct p4_node *pn_save = pf->nodes->nodes[1];
    ck_assert_str_eq(pn_save->id, "save");
    ck_assert_str_eq(pn_save->type, "EXEC");
    ck_assert_str_eq(pn_save->cmd, "save");

    /* edge */
    struct p4_edge *pe_cattosave = pf->edges->edges[0];
    ck_assert_str_eq(pe_cattosave->id, "cat-to-save");
    ck_assert_str_eq(pe_cattosave->from, "cat");
    ck_assert_str_eq(pe_cattosave->from_port, "-");
    ck_assert_str_eq(pe_cattosave->to, "save");
    ck_assert_str_eq(pe_cattosave->to_port, "-");
    ck_assert(pe_cattosave->bytes_spliced == 0l);

    free_p4_file(pf);
}
END_TEST

START_TEST(parse_ports_file) {

    struct p4_file *pf = p4_file_new("data/ports.json");
    ck_assert(pf != NULL);
    ck_assert_uint_eq(pf->nodes->length, 2);
    ck_assert_uint_eq(pf->edges->length, 1);

    /* nodes */
    struct p4_node *pn_view = pf->nodes->nodes[0];
    ck_assert_str_eq(pn_view->id, "view");
    ck_assert_str_eq(pn_view->type, "EXEC");
    ck_assert_str_eq(pn_view->cmd, "samtools view example.bam -O SAM -o _SAM_OUT_");

    struct p4_node *pn_save = pf->nodes->nodes[1];
    ck_assert_str_eq(pn_save->id, "save");
    ck_assert_str_eq(pn_save->type, "EXEC");
    ck_assert_str_eq(pn_save->cmd, "save _SAVE_IN_ example.sam");

    /* edge */
    struct p4_edge *pe_cattosave = pf->edges->edges[0];
    ck_assert_str_eq(pe_cattosave->id, "view-to-save");
    ck_assert_str_eq(pe_cattosave->from, "view");
    ck_assert_str_eq(pe_cattosave->from_port, "_SAM_OUT_");
    ck_assert_str_eq(pe_cattosave->to, "save");
    ck_assert_str_eq(pe_cattosave->to_port, "_SAVE_IN_");
    ck_assert(pe_cattosave->bytes_spliced == 0l);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_get_node) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_node_array *pna = pf->nodes;

    size_t n_nodes = pna->length;
    ck_assert_uint_eq(n_nodes, 2);

    struct p4_node *pn;

    pn = get_node(pna, 0);
    ck_assert(pn != NULL);
    ck_assert_str_eq(pn->id, "cat");
    ck_assert_str_eq(pn->cmd, "cat");
    ck_assert_str_eq(pn->type, "EXEC");

    pn = get_node(pna, -1);
    ck_assert(pn == NULL);

    pn = get_node(pna, n_nodes);
    ck_assert(pn == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_p4_file_get_node) {
    struct p4_file *pf = p4_file_new("data/basic.json");

    size_t n_nodes = pf->nodes->length;
    ck_assert_uint_eq(n_nodes, 2);

    struct p4_node *pn;

    pn = p4_file_get_node(pf, 0);
    ck_assert(pn != NULL);
    ck_assert_str_eq(pn->id, "cat");
    ck_assert_str_eq(pn->cmd, "cat");
    ck_assert_str_eq(pn->type, "EXEC");

    pn = p4_file_get_node(pf, -1);
    ck_assert(pn == NULL);

    pn = p4_file_get_node(pf, n_nodes);
    ck_assert(pn == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_get_edge) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_edge_array *pea = pf->edges;

    size_t n_edges = pea->length;
    ck_assert_uint_eq(n_edges, 1);

    struct p4_edge *pe;

    pe = get_edge(pea, 0);
    ck_assert(pe != NULL);
    ck_assert_str_eq(pe->id, "cat-to-save");
    ck_assert_str_eq(pe->from, "cat");
    ck_assert_str_eq(pe->to, "save");

    pe = get_edge(pea, -1);
    ck_assert(pe == NULL);

    pe = get_edge(pea, n_edges);
    ck_assert(pe == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_p4_file_get_edge) {
    struct p4_file *pf = p4_file_new("data/basic.json");

    size_t n_edges = pf->edges->length;
    ck_assert_uint_eq(n_edges, 1);

    struct p4_edge *pe;

    pe = p4_file_get_edge(pf, 0);
    ck_assert(pe != NULL);
    ck_assert_str_eq(pe->id, "cat-to-save");
    ck_assert_str_eq(pe->from, "cat");
    ck_assert_str_eq(pe->to, "save");

    pe = p4_file_get_edge(pf, -1);
    ck_assert(pe == NULL);

    pe = p4_file_get_edge(pf, n_edges);
    ck_assert(pe == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_find_node_by_id) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_node *pn_cat = find_node_by_id(pf, "cat");
    ck_assert_str_eq(pn_cat->id, "cat");
    ck_assert_str_eq(pn_cat->type, "EXEC");
    ck_assert_str_eq(pn_cat->cmd, "cat");

    struct p4_node *pn_na = find_node_by_id(pf, "NONE");
    ck_assert(pn_na == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_find_edge_by_id) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_edge *pe = find_edge_by_id(pf, "cat-to-save");
    ck_assert_str_eq(pe->id, "cat-to-save");
    ck_assert_str_eq(pe->from, "cat");
    ck_assert_str_eq(pe->to, "save");

    struct p4_edge *pe_na = find_edge_by_id(pf, "NONE");
    ck_assert(pe_na == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_find_from_node_by_edge_id) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_node *pn = find_from_node_by_edge_id(pf, "cat-to-save");
    ck_assert_str_eq(pn->id, "cat");
    ck_assert_str_eq(pn->type, "EXEC");
    ck_assert_str_eq(pn->cmd, "cat");

    struct p4_node *pn_na = find_from_node_by_edge_id(pf, "NONE");
    ck_assert(pn_na == NULL);

    free_p4_file(pf);
}
END_TEST

START_TEST(test_find_to_node_by_edge_id) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_node *pn = find_to_node_by_edge_id(pf, "cat-to-save");
    ck_assert_str_eq(pn->id, "save");
    ck_assert_str_eq(pn->type, "EXEC");
    ck_assert_str_eq(pn->cmd, "save");

    struct p4_node *pn_na = find_to_node_by_edge_id(pf, "NONE");
    ck_assert(pn_na == NULL);

    free_p4_file(pf);
}
END_TEST

Suite *parser_suite(void) {
    Suite *s = suite_create("parser");

    TCase *tc_parse = tcase_create("parse");
    tcase_add_test(tc_parse, fail_parse_broken_json);
    tcase_add_test(tc_parse, parse_basic_file);
    tcase_add_test(tc_parse, parse_ports_file);
    suite_add_tcase(s, tc_parse);

    TCase *tc_find_node = tcase_create("find nodes");
    tcase_add_test(tc_find_node, test_get_node);
    tcase_add_test(tc_find_node, test_p4_file_get_node);
    tcase_add_test(tc_find_node, test_find_node_by_id);
    tcase_add_test(tc_find_node, test_find_from_node_by_edge_id);
    tcase_add_test(tc_find_node, test_find_to_node_by_edge_id);
    suite_add_tcase(s, tc_find_node);

    TCase *tc_find_edge = tcase_create("find edges");
    tcase_add_test(tc_find_edge, test_get_edge);
    tcase_add_test(tc_find_edge, test_p4_file_get_edge);
    tcase_add_test(tc_find_edge, test_find_edge_by_id);
    suite_add_tcase(s, tc_find_edge);

    return s;
}
