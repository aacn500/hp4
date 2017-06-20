#include <stdlib.h>

#include <check.h>

#include "../src/parser.h"

START_TEST(test_alive) {
    ck_assert_int_eq(1, 1);
}
END_TEST

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

START_TEST(test_find_node_by_id) {
    struct p4_file *pf = p4_file_new("data/basic.json");
    struct p4_node *pn_cat = find_node_by_id(pf, "cat");
    ck_assert_str_eq(pn_cat->id, "cat");
    ck_assert_str_eq(pn_cat->type, "EXEC");
    ck_assert_str_eq(pn_cat->cmd, "cat");


    struct p4_node *pn_na = find_node_by_id(pf, "NONE");
    ck_assert(pn_na == NULL);
}
END_TEST

Suite *parser_suite(void) {
    Suite *s = suite_create("parser");

    TCase *tc_alive = tcase_create("Alive");
    tcase_add_test(tc_alive, test_alive);
    suite_add_tcase(s, tc_alive);

    TCase *tc_parse = tcase_create("parse");
    tcase_add_test(tc_parse, fail_parse_broken_json);
    tcase_add_test(tc_parse, parse_basic_file);
    tcase_add_test(tc_parse, parse_ports_file);
    suite_add_tcase(s, tc_parse);

    TCase *tc_find_node = tcase_create("find node");
    tcase_add_test(tc_find_node, test_find_node_by_id);
    suite_add_tcase(s, tc_find_node);

    return s;
}
