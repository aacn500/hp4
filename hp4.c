#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <event2/event.h>

typedef struct Pipe {
    int read_fd;
    int write_fd;
} Pipe;

struct event_args {
    Pipe **pipes;
    int *bytes_passed;
};

struct sigchld_args {
    Pipe **pipes;
    pid_t *pids;
    struct event_base *eb;
};

int children_gone = 0;

int close_pipe(Pipe *pipe_to_close) {
    return close(pipe_to_close->read_fd) | close(pipe_to_close->write_fd);
}

void sigint_handler(evutil_socket_t fd, short what, void *arg) {
    fprintf(stderr, "\b\bHandling sigint...\n");
    struct event_base *eb = arg;
    event_base_loopbreak(eb);
}

void sigchld_handler(evutil_socket_t fd, short what, void *arg) {
    struct sigchld_args *sa = arg;
    int *status = malloc(sizeof(*status));
    while (1) {
        pid_t p = waitpid(-1, status, WNOHANG);
        if (p == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        else if (p == 0) {
            break;
        }
        else if (WIFEXITED(*status)) {
            children_gone++;
            fprintf(stderr, "%d child process ended\n", children_gone);

            if (close_pipe(sa->pipes[0]) < 0) {
                perror("close pipes 0");
            }

            if (close_pipe(sa->pipes[1]) < 0) {
                perror("close pipes 1");
            }
            if (kill(sa->pids[0], SIGINT) < 0) {
                perror("kill pid 1");
            }
            event_base_loopexit(sa->eb, NULL);
        }
    }
    free(status);
}

Pipe *pipe_new(void) {
    Pipe *new_pipe = malloc(sizeof(*new_pipe));

    int fds[2] = {0, 0};
    if (pipe(fds) < 0) {
        return NULL;
    }

    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

void readPipeCb(evutil_socket_t fd, short what, void *arg) {
    struct event_args* ea = arg;
    if ((what & EV_READ) == 0) {
        return;
    }
    ssize_t bytes_spliced = splice(ea->pipes[0]->read_fd,
                                   NULL,
                                   ea->pipes[1]->write_fd,
                                   NULL,
                                   4096,
                                   SPLICE_F_NONBLOCK);
    if (bytes_spliced < 0) {
        return;
    }
    int *bytes_passed = ea->bytes_passed;
    *bytes_passed += bytes_spliced;
}

int run(Pipe **pipes, pid_t* pids) {
    fprintf(stderr, "Hi\n");
    int bytes_passed = 0;

    struct event_base *eb = event_base_new();

    struct event_args *ea = malloc(sizeof(*ea));
    ea->pipes = pipes;
    ea->bytes_passed = &bytes_passed;

    struct event *ev =
        event_new(eb, pipes[0]->read_fd, EV_READ|EV_PERSIST, readPipeCb, ea);
    event_add(ev, NULL);

    struct event *sigintev = evsignal_new(eb, SIGINT, sigint_handler, eb);
    event_add(sigintev, NULL);

    struct sigchld_args *sa = malloc(sizeof(*sa));
    sa->pipes = pipes;
    sa->pids = pids;
    sa->eb = eb;
    struct event *sigchldev = evsignal_new(eb, SIGCHLD, sigchld_handler, sa);
    event_add(sigchldev, NULL);

    if (event_base_dispatch(eb) < 0) {
        perror("event base loop");
        return 1;
    }

    event_free(ev);
    event_free(sigintev);
    event_free(sigchldev);

    free(ea);
    free(sa);

    return bytes_passed;
}

int main(int argc, char **argv) {
    Pipe *pipes[2] = {pipe_new(), pipe_new()};
    pid_t pids[2];
    pids[0] = fork();
    if (pids[0] == 0) { // child
        // set stdin to pipes[1]->read_fd, exec cat
        if (dup2(pipes[1]->read_fd, STDIN_FILENO) < 0) {
            perror("dup2");
            return 1;
        }
        char *args[2] = {"cat", NULL};
        execvp(args[0], args);
    } else if (pids[0] < 0) { // err
        perror("fork");
        return 1;
    } else { // parent
        pids[1] = fork();
        if (pids[1] < 0) { // err
            perror("2nd fork");
            return 1;
        } else if (pids[1] == 0) { // child
            if (dup2(pipes[0]->write_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                return 1;
            }
            char *args[3] = {"echo", "Hi! I'm a string!", NULL};
            execvp(args[0], args);
        } else { // parent
            int bytes_spliced = run(pipes, pids);
            if (bytes_spliced < 0) {
                perror("run");
                return 1;
            }
            //for (int i = 0; i < sizeof(pids)/sizeof(pids[0]); i++) {
            //    if (kill(pids[i], SIGINT) < 0) {
            //        perror("kill");
            //        return 1;
            //    }
            //}
            //if (close_pipe(pipes[0]) || close_pipe(pipes[1])) {
            //    perror("close_pipe");
            //    return 1;
            //}
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
