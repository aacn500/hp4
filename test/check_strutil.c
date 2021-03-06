#include "config.h"

#include <stdlib.h>

#include <check.h>

#include "../src/strutil.h"

START_TEST(test_strrep) {
    /* Fails when any input is NULL */
    ck_assert(strrep(NULL, NULL, NULL) == NULL);
    ck_assert(strrep("original string", "original", NULL) == NULL);
    ck_assert(strrep("original string", NULL, "new") == NULL);
    ck_assert(strrep(NULL, "original", "new") == NULL);

    /* Fails when `original` or `replace` are 0 length strings */
    ck_assert(strrep("original string", "", "new") == NULL);
    ck_assert(strrep("", "original", "new") == NULL);

    /* Succeeds when `with` is 0 length string */
    ck_assert_str_eq(
            strrep("original string", "original", ""),
            " string"
    );

    /* Succeeds when `with` is shorter than `replace` */
    ck_assert_str_eq(
            strrep("original string", "original", "new"),
            "new string"
    );

    /* Succeeds when `with` is longer than `replace` */
    ck_assert_str_eq(
            strrep("original string", "original", "longer than before"),
            "longer than before string"
    );
}
END_TEST

START_TEST(test_parse_edge_string) {

    char **result = parse_edge_string("cat");
    ck_assert(result != NULL);
    ck_assert_str_eq(result[0], "cat");
    ck_assert_str_eq(result[1], STDIO_PORT);

    free(result[0]);
    free(result[1]);
    free(result);

    result = parse_edge_string("cat" PORT_DELIMITER "DOG");
    ck_assert(result != NULL);
    ck_assert_str_eq(result[0], "cat");
    ck_assert_str_eq(result[1], "DOG");

    free(result[0]);
    free(result[1]);
    free(result);

    result = parse_edge_string("");
    ck_assert(result != NULL);
    ck_assert_str_eq(result[0], "");
    ck_assert_str_eq(result[1], STDIO_PORT);

    free(result[0]);
    free(result[1]);
    free(result);

    result = parse_edge_string(NULL);
    ck_assert(result == NULL);

    result = parse_edge_string("cat" PORT_DELIMITER "DOG" PORT_DELIMITER "TOO_MANY_PORTS");
    ck_assert(result == NULL);
}
END_TEST

START_TEST(test_parse_argstring) {
    struct argstruct *arg = malloc(sizeof(*arg));
    int success;

    success = parse_argstring(NULL, "cat");
    ck_assert_int_eq(success, -1);

    success = parse_argstring(arg, NULL);
    ck_assert_int_eq(success, -1);

    success = parse_argstring(arg, "bash -c \"unclosed block");
    ck_assert_int_eq(success, -1);

    success = parse_argstring(arg, "cat");
    ck_assert_int_eq(success, 0);
    ck_assert_int_eq(arg->argc, 1);
    ck_assert_str_eq(arg->argv[0], "cat");

    free(*arg->argv);

    success = parse_argstring(arg, "cat file");
    ck_assert_int_eq(success, 0);
    ck_assert_int_eq(arg->argc, 2);
    ck_assert_str_eq(arg->argv[0], "cat");
    ck_assert_str_eq(arg->argv[1], "file");

    free(*arg->argv);

    success = parse_argstring(arg, "bash -c \"block of args\" another");
    ck_assert_int_eq(success, 0);
    ck_assert_int_eq(arg->argc, 4);
    ck_assert_str_eq(arg->argv[0], "bash");
    ck_assert_str_eq(arg->argv[1], "-c");
    ck_assert_str_eq(arg->argv[2], "block of args");
    ck_assert_str_eq(arg->argv[3], "another");
    ck_assert(arg->argv[4] == NULL);

    free(*arg->argv);

    success = parse_argstring(arg, "bash -c \"block of args\"");
    ck_assert_int_eq(success, 0);
    ck_assert_int_eq(arg->argc, 3);
    ck_assert_str_eq(arg->argv[0], "bash");
    ck_assert_str_eq(arg->argv[1], "-c");
    ck_assert_str_eq(arg->argv[2], "block of args");
    ck_assert(arg->argv[3] == NULL);

    free(*arg->argv);

    free(arg);
}
END_TEST

Suite *strutil_suite(void) {
    Suite *s;
    s = suite_create("strutil");

    TCase *tc_strrep;

    tc_strrep = tcase_create("strrep");
    tcase_add_test(tc_strrep, test_strrep);
    suite_add_tcase(s, tc_strrep);

    TCase *tc_parse_edge_string = tcase_create("parse edge string");
    tcase_add_test(tc_parse_edge_string, test_parse_edge_string);
    suite_add_tcase(s, tc_parse_edge_string);

    TCase *tc_parse_argstring = tcase_create("parse argstring");
    tcase_add_test(tc_parse_argstring, test_parse_argstring);
    suite_add_tcase(s, tc_parse_argstring);

    return s;
}
