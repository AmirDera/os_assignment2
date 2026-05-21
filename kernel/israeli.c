#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "israeli.h"

// Global table. Slot index is the user-visible lock id.
struct israeli israeli_locks[ISRAELINUM];

// Initialize every slot. Called once from main() during boot,
// mirroring procinit().
void
israeli_init(void)
{
  for (int i = 0; i < ISRAELINUM; i++) {
    struct israeli *l = &israeli_locks[i];
    initlock(&l->lk, "israeli");
    l->active = 0;
    l->held = 0;
    l->owner_pid = 0;
    l->next_owner_pid = 0;
    l->favoritism = 0;
    l->waitersnum = 0;
    for (int j = 0; j < WAITERSNUM; j++) {
      l->waiter_pid[j] = 0;
      l->waiter_gid[j] = 0;
    }
  }
}

// Remove the waiter at position `idx` from the FIFO queue (shifting
// later entries down by one). Caller must hold l->lk.
static void
israeli_queue_remove(struct israeli *l, int idx)
{
  for (int i = idx; i < l->waitersnum - 1; i++) {
    l->waiter_pid[i] = l->waiter_pid[i + 1];
    l->waiter_gid[i] = l->waiter_gid[i + 1];
  }
  l->waitersnum--;
  l->waiter_pid[l->waitersnum] = 0;
  l->waiter_gid[l->waitersnum] = 0;
}

// Allocate a new lock with the given favoritism coefficient.
// Returns the lock id (>= 0) or -1 on error.
int
israeli_create(int favoritism)
{
  if (favoritism < 0 || favoritism > 100)
    return -1;

  for (int i = 0; i < ISRAELINUM; i++) {
    struct israeli *l = &israeli_locks[i];
    acquire(&l->lk);
    if (!l->active) {
      l->active = 1;
      l->held = 0;
      l->owner_pid = 0;
      l->next_owner_pid = 0;
      l->favoritism = favoritism;
      l->waitersnum = 0;
      release(&l->lk);
      return i;
    }
    release(&l->lk);
  }
  return -1; // no free slot
}

// Acquire the lock. Blocks (via sleep) until the calling process is
// selected as the owner. Returns 0 on success, -1 on error.
int
israeli_acquire(int lock_id)
{
  if (lock_id < 0 || lock_id >= ISRAELINUM)
    return -1;

  struct israeli *l = &israeli_locks[lock_id];
  int mypid = myproc()->pid;
  int mygid = myproc()->gid;

  acquire(&l->lk);
  if (!l->active) {
    release(&l->lk);
    return -1;
  }

  // Fast path: lock is free, take it directly.
  if (!l->held) {
    l->held = 1;
    l->owner_pid = mypid;
    release(&l->lk);
    return 0;
  }

  // Slow path: enqueue in FIFO order and sleep until release chooses us.
  if (l->waitersnum >= WAITERSNUM) {
    release(&l->lk);
    return -1; // queue full
  }
  l->waiter_pid[l->waitersnum] = mypid;
  l->waiter_gid[l->waitersnum] = mygid;
  l->waitersnum++;

  // Wait until israeli_release sets next_owner_pid == our pid, or the
  // lock is destroyed under us.
  while (l->active && l->next_owner_pid != mypid) {
    sleep(l, &l->lk);
  }

  if (!l->active) {
    release(&l->lk);
    return -1;
  }

  // We were selected. release() already set owner_pid = mypid and
  // removed us from the queue; just consume next_owner_pid.
  l->next_owner_pid = 0;
  release(&l->lk);
  return 0;
}

// Release the lock and hand it off to the next owner according to the
// favoritism policy. Returns 0 on success, -1 on error.
int
israeli_release(int lock_id)
{
  if (lock_id < 0 || lock_id >= ISRAELINUM)
    return -1;

  struct israeli *l = &israeli_locks[lock_id];
  int mypid = myproc()->pid;
  int mygid = myproc()->gid;

  acquire(&l->lk);
  if (!l->active || !l->held || l->owner_pid != mypid) {
    release(&l->lk);
    return -1;
  }

  // No waiters: simply release.
  if (l->waitersnum == 0) {
    l->held = 0;
    l->owner_pid = 0;
    release(&l->lk);
    return 0;
  }

  // Default: FIFO head.
  int chosen = 0;

  // Look for the earliest waiter that shares our gid.
  int same_gid_idx = -1;
  for (int i = 0; i < l->waitersnum; i++) {
    if (l->waiter_gid[i] == mygid) {
      same_gid_idx = i;
      break;
    }
  }

  // With probability favoritism, pick that same-group waiter instead.
  if (same_gid_idx >= 0 && (lcg_rand() % 100) < (uint)l->favoritism) {
    chosen = same_gid_idx;
  }

  int chosen_pid = l->waiter_pid[chosen];
  israeli_queue_remove(l, chosen);

  // Hand the lock off to the chosen waiter; the lock stays "held".
  l->owner_pid = chosen_pid;
  l->next_owner_pid = chosen_pid;

  release(&l->lk);
  wakeup(l); // wake everyone sleeping on this lock; only chosen_pid proceeds
  return 0;
}

// Destroy the lock. Future use of this id returns -1. Any current
// waiters wake up and return -1 from israeli_acquire.
int
israeli_destroy(int lock_id)
{
  if (lock_id < 0 || lock_id >= ISRAELINUM)
    return -1;

  struct israeli *l = &israeli_locks[lock_id];

  acquire(&l->lk);
  if (!l->active) {
    release(&l->lk);
    return -1;
  }
  l->active = 0;
  l->held = 0;
  l->owner_pid = 0;
  l->next_owner_pid = 0;
  l->waitersnum = 0;
  for (int j = 0; j < WAITERSNUM; j++) {
    l->waiter_pid[j] = 0;
    l->waiter_gid[j] = 0;
  }
  release(&l->lk);
  wakeup(l); // let any sleeping acquirers bail out
  return 0;
}
