#include "kernel/types.h"
#include "user/user.h"
void filter(int left_pipe[]) {
  int right_pipe[2];
  int n;
  int p;
  close(left_pipe[1]);
  if (read(left_pipe[0], &p, sizeof(int)) == 0) {
    close(left_pipe[0]);
    exit(0);
  }
  fprintf(1, "prime %d\n", p);
  pipe(right_pipe);
  int pid = fork();
  if (pid == 0) {
    filter(right_pipe);
  } else {
    while (read(left_pipe[0], &n, sizeof(int)) != 0) {
      if (n % p) {
        write(right_pipe[1], &n, sizeof(int));
      }
    }
    close(left_pipe[0]);
    close(right_pipe[1]);
    wait((int *)0);
  }
  exit(0);
}
int main(int argc, char **argv) {
  if (argc > 1) {
    fprintf(2, "usage: primes\n");
    exit(1);
  }
  int left_pipe[2];
  int n;
  pipe(left_pipe);
  int pid = fork();
  if (pid == 0) {
    filter(left_pipe);
  } else {
    for (n = 2; n <= 35; n++) {
      write(left_pipe[1], &n, sizeof(int));
    }
    close(left_pipe[1]);
    wait((int *)0);
  }
  exit(0);
}
