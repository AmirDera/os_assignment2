#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define LCG_A 1664525u
#define LCG_B 1013904223u

// Global state of the generator. Protected by rand_lock.
static uint lcg_state;
static struct spinlock rand_lock;

// Initialize the LCG state and its protecting spinlock.
// Called once from main() during kernel boot.
void
rand_init(void)
{
  initlock(&rand_lock, "rand");
  lcg_state = 0;
}

// Reseed the generator. Safe to call concurrently from multiple CPUs.
void
lcg_srand(uint seed)
{
  acquire(&rand_lock);
  lcg_state = seed;
  release(&rand_lock);
}

// Advance the generator by one step and return the new value.
// Read-modify-write of the global state is protected by a spinlock.
uint
lcg_rand(void)
{
  uint val;
  acquire(&rand_lock);

  // The modulus m = 2^32 is obtained from the natural overflow
  // of the 32-bit unsigned integer type in C.
  lcg_state = LCG_A * lcg_state + LCG_B;
  val = lcg_state;
  
  release(&rand_lock);
  return val;
}
