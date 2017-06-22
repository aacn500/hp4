#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <check.h>

#include "../src/parser.h"
#include "../src/stats.h"

START_TEST(test_create_stats_file) {
    /* Create_stats_file sends output to stdout. In order to read
     * the output, swap stdout for a pipe() using dup(), read the
     * stats output, and then swap stdout back. */

    int success;

    int pipe_fds[2] = {0, 0};
    success = pipe(pipe_fds);
    ck_assert_int_eq(success, 0);

    int orig_stdout = dup(STDOUT_FILENO);

    success = dup2(pipe_fds[1], STDOUT_FILENO);
    ck_assert_int_ge(success, 0);

    struct p4_file *pf = p4_file_new("data/basic.json");
    ck_assert(pf != NULL);

    pf->edges->edges[0]->bytes_spliced = 172l;

    int current_flags;
    current_flags = fcntl(pipe_fds[1], F_GETFL, NULL);
    success = fcntl(pipe_fds[1], F_SETFL, current_flags | O_NONBLOCK);
    ck_assert_int_eq(success, 0);
    current_flags = fcntl(pipe_fds[0], F_GETFL, NULL);
    success = fcntl(pipe_fds[0], F_SETFL, current_flags | O_NONBLOCK);
    ck_assert_int_eq(success, 0);

    success = create_stats_file(pf);
    ck_assert_int_eq(success, 0);

    success = fflush(stdout);
    ck_assert_int_eq(success, 0);

    success = close(pipe_fds[1]);
    ck_assert_int_eq(success, 0);

    char buf[1024] = {'\0'};
    ssize_t bytes_read = read(pipe_fds[0], buf, 1024);
    ck_assert_int_gt(bytes_read, 0);
    buf[bytes_read] = '\0';

    ck_assert_str_eq(buf, "{\"cat-to-save\": 172}\n");

    free_p4_file(pf);

    success = dup2(STDOUT_FILENO, orig_stdout);
    ck_assert_int_ge(success, 0);

    success = close(pipe_fds[0]);
    ck_assert_int_eq(success, 0);
    success = close(orig_stdout);
    ck_assert_int_eq(success, 0);
}
END_TEST

Suite *stats_suite(void) {
    Suite *s = suite_create("stats");

    TCase *tc_stats = tcase_create("create stats file");
    tcase_add_test(tc_stats, test_create_stats_file);
    suite_add_tcase(s, tc_stats);

    return s;
}
