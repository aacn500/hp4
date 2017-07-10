#include <stdlib.h>

#include <check.h>

Suite *parser_suite(void);
Suite *pipe_suite(void);
Suite *stats_suite(void);
Suite *strutil_suite(void);
Suite *validate_suite(void);

int main(void) {
    int n_failed;

    Suite *s_parser = parser_suite();
    SRunner *sr = srunner_create(s_parser);

    Suite *s_pipe = pipe_suite();
    srunner_add_suite(sr, s_pipe);

    Suite *s_stats = stats_suite();
    srunner_add_suite(sr, s_stats);

    Suite *s_strutil = strutil_suite();
    srunner_add_suite(sr, s_strutil);

    Suite *s_validate = validate_suite();
    srunner_add_suite(sr, s_validate);

    srunner_run_all(sr, CK_NORMAL);
    n_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
