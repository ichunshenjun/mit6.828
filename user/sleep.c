// #include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv) {
  int i;

  if (argc < 2) {
    fprintf(2, "usage: sleep ...\n");
    exit(1);
  }
  for (i = 1; i < argc; i++)
    sleep(atoi(argv[i]));
  exit(0);
}