#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Pipe {
    int read_fd;
    int write_fd;
} pipe_t;

pipe_t* pipe_new(void) {
    pipe_t* new_pipe = malloc(sizeof(pipe_t));

    int fds[2];
    if (pipe(fds) < 0) {
        return NULL;
    }

    // set pipe fds non-blocking
    int i;
    for (i = 0; i < 2; i++) {
        int flags = fcntl(fds[i], F_GETFL);
        fcntl(fds[i], F_SETFL, flags|O_NONBLOCK);
    }

    new_pipe->read_fd = fds[0];
    new_pipe->write_fd = fds[1];
    return new_pipe;
}

int close_pipe(pipe_t* pipe_to_close) {
    int close_errs[2];
    close_errs[0] = close(pipe_to_close->read_fd);
    close_errs[1] = close(pipe_to_close->write_fd);
    if (close_errs[0] < 0 || close_errs[1] < 0) {
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    printf("Hi\n");
    pipe_t* myPipe = pipe_new();
    if (myPipe == NULL) {
        perror("pipe_new");
        return 1;
    }

    if (close_pipe(myPipe)) {
        perror("close_pipe");
        return 1;
    }
    free(myPipe);
    printf("Bye\n");
    return 0;
}
