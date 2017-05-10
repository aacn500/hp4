#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/types.h>

volatile sig_atomic_t stop;

typedef struct Pipe {
    int read_fd;
    int write_fd;
} Pipe;

void handle_sigint(int signum) {
    stop = 1;
}

Pipe* pipe_new(void) {
    Pipe* new_pipe = malloc(sizeof(*new_pipe));

    int fds[2] = {0, 0};
    if (pipe(fds) < 0) {
        return NULL;
    }

    // set pipe fds non-blocking
    int i;
    for (i = 0; i < 2; i++) {
        int flags = fcntl(fds[i], F_GETFL);
        if (flags < 0) {
            return NULL;
        }
        if (fcntl(fds[i], F_SETFL, flags|O_NONBLOCK) < 0) {
            return NULL;
        }
    }

    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

int close_pipe(Pipe* pipe_to_close) {
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

/*
 * Open a pipe; print name of write end to stdout, then epoll read end until
 * sigint, echoing data to stdout.
 */
int main(int argc, char** argv) {
    fprintf(stderr, "Hi\n");
    Pipe* in_pipe = pipe_new();
    Pipe* out_pipe = pipe_new();
    if (in_pipe == NULL || out_pipe == NULL) {
        perror("pipe_new");
        return 1;
    }

    int my_pid = getpid();

    signal(SIGINT, handle_sigint);

    fprintf(stderr, "Write to fd /proc/%d/fd/%d and", my_pid, in_pipe->write_fd);
    fprintf(stderr, " read from fd /proc/%d/fd/%d\n", my_pid, out_pipe->read_fd);

    // epolling written with inspiration from
    // https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
    struct epoll_event event;
    struct epoll_event events[3];

    int efd = epoll_create1(0);
    if (efd < 0) {
        perror("epoll_create1");
        return 1;
    }

    event.data.fd = in_pipe->read_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, in_pipe->read_fd, &event) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    long bytes_passed = 0l;

    while(!stop) {
        int n = epoll_wait(efd, events, 3, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }
            else if (in_pipe->read_fd == events[i].data.fd) {
                ssize_t bytes_spliced = splice(in_pipe->read_fd, NULL,
                       out_pipe->write_fd, NULL, 1024, SPLICE_F_NONBLOCK);
                if (bytes_spliced < 0) {
                    if (errno & EAGAIN) {
                        errno = 0;
                        continue;
                    }
                    else {
                        perror("splice");
                        return 1;
                    }
                }
                bytes_passed += (long)bytes_spliced;
            }
        }
    }

    if (close_pipe(in_pipe) || close_pipe(out_pipe)) {
        perror("close_pipe");
        return 1;
    }
    fprintf(stderr, "Read and wrote %ld bytes\n", bytes_passed);
    free(in_pipe);
    in_pipe = NULL;
    free(out_pipe);
    out_pipe = NULL;
    fprintf(stderr, "Bye\n");
    return 0;
}
