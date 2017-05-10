#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/epoll.h>

volatile sig_atomic_t stop;

typedef struct Pipe {
    int read_fd;
    int write_fd;
} pipe_t;

void handle_sigint(int signum) {
    stop = 1;
}

pipe_t* pipe_new(void) {
    pipe_t* new_pipe = malloc(sizeof(*new_pipe));

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

int close_pipe(pipe_t* pipe_to_close) {
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

/* Open a pipe; print name of write end to stdout, then epoll read end until
 * sigint, echoing data to stdout.
 */
int main(int argc, char** argv) {
    printf("Hi\n");
    pipe_t* my_pipe = pipe_new();
    if (my_pipe == NULL) {
        perror("pipe_new");
        return 1;
    }

    signal(SIGINT, handle_sigint);

    printf("Write to fd %d now!\n", my_pipe->write_fd);

    // epolling written with inspiration from
    // https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
    struct epoll_event event;
    struct epoll_event events[3];

    int efd = epoll_create1(0);
    if (efd < 0) {
        perror("epoll_create1");
        return 1;
    }

    event.data.fd = my_pipe->read_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, my_pipe->read_fd, &event) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    while(!stop) {
        int n = epoll_wait(efd, events, 3, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }
            else if (my_pipe->read_fd == events[i].data.fd) {
                char* read_buf = calloc(1024, sizeof(char));
                if (read(my_pipe->read_fd, read_buf, 1024) < 0) {
                    if (errno & EAGAIN) {
                        errno = 0;
                        continue;
                    }
                    else {
                        perror("read");
                        return 1;
                    }
                }
                printf("%s", read_buf);
            }
        }
    }

    if (close_pipe(my_pipe)) {
        perror("close_pipe");
        return 1;
    }
    free(my_pipe);
    my_pipe = NULL;
    printf("Bye\n");
    return 0;
}
