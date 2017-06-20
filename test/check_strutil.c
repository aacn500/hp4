#include <stdlib.h>

#include <check.h>

#include "../src/strutil.h"

START_TEST(test_alive) {
    ck_assert_int_eq(1, 1);
}
END_TEST

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

Suite *strutil_suite(void) {
    Suite *s;
    s = suite_create("strutil");

    TCase *tc_alive;

    tc_alive = tcase_create("Alive");
    tcase_add_test(tc_alive, test_alive);
    suite_add_tcase(s, tc_alive);

    TCase *tc_strrep;

    tc_strrep = tcase_create("strrep");
    tcase_add_test(tc_strrep, test_strrep);
    suite_add_tcase(s, tc_strrep);

    return s;
}
