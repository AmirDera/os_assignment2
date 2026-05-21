// Simple smoke test for Task 0: LCG PRNG syscalls.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // Deterministic sequence from seed 0.
  lcg_srand(0);
  printf("seed=0 sequence:\n");
  for(int i = 0; i < 5; i++)
    printf("  %d\n", lcg_rand());

  // Reseed with the same value and verify reproducibility.
  lcg_srand(0);
  printf("seed=0 again (should match):\n");
  for(int i = 0; i < 5; i++)
    printf("  %d\n", lcg_rand());

  // Different seed -> different sequence.
  lcg_srand(42);
  printf("seed=42:\n");
  for(int i = 0; i < 3; i++)
    printf("  %d\n", lcg_rand());

  exit(0);
}
