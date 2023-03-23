#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv) {
  if (argc > 1) {
    fprintf(2, "usage: primes\n");
    exit(1);
  }
  exit(0);
}