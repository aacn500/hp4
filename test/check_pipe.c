#include <stdlib.h>

#include <check.h>

#include "../src/pipe.h"

START_TEST(test_pipe_array) {
    int success;

    struct pipe_array *pa = pipe_array_new();
    ck_assert(pa != NULL);

    success = pipe_array_append_new(pa, "-", "edge");
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 1u);

    success = pipe_array_append_new(pa, "PORT", "edge2");
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 2u);

    success = pipe_array_has_pipe_with_port(pa, "PORT");
    ck_assert_int_eq(success, 1);

    success = pipe_array_has_pipe_with_port(pa, "PORT2");
    ck_assert_int_eq(success, 0);

    struct pipe *p;
    p = find_pipe_by_edge_id(pa, "edge");
    ck_assert(p != NULL);
    ck_assert_str_eq(p->edge_ids[0], "edge");
    ck_assert_str_eq(p->port, "-");

    p = find_pipe_by_edge_id(pa, "edge2");
    ck_assert(p != NULL);
    ck_assert_str_eq(p->edge_ids[0], "edge2");
    ck_assert_str_eq(p->port, "PORT");

    p = find_pipe_by_edge_id(pa, "no-edge");
    ck_assert(p == NULL);

    pipe_array_free(pa);
}
END_TEST

START_TEST(test_pipe_open_and_close) {
    int success;

    struct pipe_array *pa = pipe_array_new();
    ck_assert(pa != NULL);

    success = pipe_array_append_new(pa, "-", "edge");
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 1u);
    ck_assert(pa->pipes[0]->read_fd_is_open);
    ck_assert(pa->pipes[0]->write_fd_is_open);

    success = close_pipe(pa->pipes[0]);
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 1u);
    ck_assert(!pa->pipes[0]->read_fd_is_open);
    ck_assert(!pa->pipes[0]->write_fd_is_open);

    success = pipe_array_append_new(pa, "-2", "edge2");
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 2u);
    ck_assert(pa->pipes[1]->read_fd_is_open);
    ck_assert(pa->pipes[1]->write_fd_is_open);

    success = pipe_array_close(pa);
    ck_assert_int_eq(success, 0);
    ck_assert_uint_eq(pa->length, 2u);
    ck_assert(!pa->pipes[1]->read_fd_is_open);
    ck_assert(!pa->pipes[1]->write_fd_is_open);
}
END_TEST

Suite *pipe_suite(void) {
    Suite *s = suite_create("pipe");

    TCase *tc_pipe_array = tcase_create("pipe array");
    tcase_add_test(tc_pipe_array, test_pipe_array);
    suite_add_tcase(s, tc_pipe_array);

    TCase *tc_open_and_close = tcase_create("pipe open and close");
    tcase_add_test(tc_open_and_close, test_pipe_open_and_close);
    suite_add_tcase(s, tc_open_and_close);

    return s;
}
