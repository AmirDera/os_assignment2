#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define MAX_TEAMS 32  // upper bound on team ids

static int team_scores[MAX_TEAMS];
static struct spinlock teams_lock;

void
teams_init(void)
{
  initlock(&teams_lock, "teams");
  for (int i = 0; i < MAX_TEAMS; i++)
    team_scores[i] = 0;
}

void
teams_reset(void)
{
  acquire(&teams_lock);
  for (int i = 0; i < MAX_TEAMS; i++)
    team_scores[i] = 0;
  release(&teams_lock);
}

int
team_score_inc(int team)
{
  if (team < 0 || team >= MAX_TEAMS)
    return -1;
  acquire(&teams_lock);
  int val = ++team_scores[team];
  release(&teams_lock);
  return val;
}

int
team_score_get(int team)
{
  if (team < 0 || team >= MAX_TEAMS)
    return -1;
  acquire(&teams_lock);
  int val = team_scores[team];
  release(&teams_lock);
  return val;
}
