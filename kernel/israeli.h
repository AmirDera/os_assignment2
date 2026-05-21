#ifndef ISRAELI_H
#define ISRAELI_H

#define ISRAELINUM 16  // number of Israeli lock slots (spec requires >= 15)
#define WAITERSNUM 16  // max processes that may wait on a single lock

struct israeli {
  struct spinlock lk;          // protects all fields below

  int active;                  // 1 if this slot is currently in use
  int held;                    // 1 if the lock is currently owned
  int owner_pid;               // pid of the current owner (when held)
  int favoritism;              // favoritism coefficient c in [0, 100]

  // Hand-off pid chosen by israeli_release() for the next owner.
  // Lets the wakeup recipient know it is the selected waiter.
  int next_owner_pid;

  // FIFO queue of waiters (front = index 0).
  int waitersnum;
  int waiter_pid[WAITERSNUM];
  int waiter_gid[WAITERSNUM];    // cached gid of each waiter at enqueue time
};

extern struct israeli israeli_locks[ISRAELINUM];

#endif
