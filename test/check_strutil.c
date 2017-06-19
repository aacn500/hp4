#include <stdlib.h>

#include <check.h>

#include "../src/strutil.h"

START_TEST(test_alive) {
    ck_assert_int_eq(1, 1);
}
END_TEST

Suite *strutil_suite(void) {
    Suite *s;
    TCase *tc_alive;

    s = suite_create("strutil");

    tc_alive = tcase_create("Alive");

    tcase_add_test(tc_alive, test_alive);
    suite_add_tcase(s, tc_alive);

    return s;
}

int main(void) {
    int n_failed;
    Suite *s;
    SRunner *sr;

    s = strutil_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
