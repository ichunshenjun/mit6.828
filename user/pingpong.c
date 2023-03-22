#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv) {
  if (argc > 1) {
    fprintf(2, "usage: pingpong\n");
    exit(1);
  }
  int fd[2];
  char buf[1];
  char msg = 'A';
  if (pipe(fd) < 0) {
    fprintf(2, "pipe error\n");
    exit(1);
  }
  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork error\n");
    exit(1);
  }
  if (pid == 0) {
    // read(fd[0], buf, 1);
    read(fd[0], buf, 1);
    printf("%d: received ping\n", getpid());
    write(fd[1], &msg, 1);
  } else {
    write(fd[1], &msg, 1);
    // wait(NULL);
    read(fd[0], buf, 1); // read the byte from the child
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}