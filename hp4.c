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

    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

int close_pipe(Pipe* pipe_to_close) {
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

int run(Pipe **pipes) {
    signal(SIGINT, handle_sigint);
    fprintf(stderr, "Hi\n");

    struct epoll_event event;
    struct epoll_event events[3];

    int efd = epoll_create1(0);
    if (efd < 0) {
        perror("epoll_create1");
        return 1;
    }

    event.data.fd = pipes[0]->read_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, pipes[0]->read_fd, &event) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    int bytes_passed = 0;

    while(!stop) {
        int n = epoll_wait(efd, events, 3, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }
            else if (pipes[0]->read_fd == events[i].data.fd) {
                ssize_t bytes_spliced = splice(pipes[0]->read_fd, NULL,
                       pipes[1]->write_fd, NULL, 4096, SPLICE_F_NONBLOCK);
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
                bytes_passed += bytes_spliced;
            }
        }
    }
    return bytes_passed;
}

int main(int argc, char** argv) {
    Pipe *pipes[2] = {pipe_new(), pipe_new()};
    pid_t pids[2];
    pids[0] = fork();
    if (pids[0] == 0) { // child
        // set stdin to pipes[1]->read_fd, exec cat
        if (dup2(pipes[1]->read_fd, STDIN_FILENO) < 0) {
            perror("dup2");
            return 1;
        }
        char* args[2] = {"cat", NULL};
        execvp(args[0], args);
    } else if (pids[0] < 0) { // err
        perror("fork");
        return 1;
    } else {
        pids[1] = fork();
        if (pids[1] < 0) {
            perror("fork v2");
            return 1;
        } else if (pids[1] == 0) {
            if (dup2(pipes[0]->write_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                return 1;
            }
            char* args[3] = {"echo", "Hi! I'm a string!", NULL};
            execvp(args[0], args);
        } else {
            int bytes_spliced = run(pipes);
            if (bytes_spliced < 0) {
                perror("run");
                return 1;
            }
            for (int i = 0; i < sizeof(pids)/sizeof(pids[0]); i++) {
                if (kill(pids[i], SIGINT) < 0) {
                    perror("kill");
                    return 1;
                }
            }
            if (close_pipe(pipes[0]) || close_pipe(pipes[1])) {
                perror("close_pipe");
                return 1;
            }
            fprintf(stderr, "Read and wrote %d bytes\n", bytes_spliced);
            free(pipes[0]);
            pipes[0] = NULL;
            free(pipes[1]);
            pipes[1] = NULL;
            fprintf(stderr, "\b\bBye\n");
            return 0;
        }
    }
}
