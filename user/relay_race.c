#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TEAMSNUM    3
#define RUNNERSNUM  5
#define TARGET      30

int
main(int argc, char *argv[])
{
  int favoritism = 50;
  if (argc > 1)
    favoritism = atoi(argv[1]);

  teams_reset();

  int lock_id = israeli_create(favoritism);
  if (lock_id < 0) {
    printf("israeli_create failed\n");
    exit(1);
  }

  printf("Relay race: favoritism=%d, teams=%d, runners/team=%d, target=%d\n",
         favoritism, TEAMSNUM, RUNNERSNUM, TARGET);

  // Randomize team assignment so the queue isn't loaded team-by-team.
  // Each team still ends up with exactly RUNNERSNUM runners: we draw a
  // random team id, and if that team is already full we just try again.
  lcg_srand(uptime());
  int counts[TEAMSNUM];
  for (int t = 0; t < TEAMSNUM; t++)
    counts[t] = 0;

  int forked = 0;
  while (forked < TEAMSNUM * RUNNERSNUM) {
    int team = lcg_rand() % TEAMSNUM;
    if (counts[team] >= RUNNERSNUM)
      continue;
    counts[team]++;
    forked++;

    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      setgid(team);

      for (;;) {
        israeli_acquire(lock_id);

        // End-of-race check inside the lock: ensures no team is bumped
        // beyond TARGET by another runner that already saw the winner.
        int over = 0;
        for (int t = 0; t < TEAMSNUM; t++) {
          if (team_score_get(t) >= TARGET) {
            over = 1;
            break;
          }
        }
        if (over) {
          israeli_release(lock_id);
          exit(0);
        }

        int new_score = team_score_inc(team);
        printf("Runner %d (Team %d) acquired the baton\n", getpid(), team);
        printf("Team %d score = %d\n", team, new_score);

        israeli_release(lock_id);
        sleep(1);
      }
    }
  }

  // Parent reaps all children.
  for (int i = 0; i < TEAMSNUM * RUNNERSNUM; i++)
    wait(0);

  // Report final standings.
  int winner = -1, best = -1;
  printf("\n--- final scores (favoritism=%d) ---\n", favoritism);
  for (int t = 0; t < TEAMSNUM; t++) {
    int s = team_score_get(t);
    printf("  Team %d: %d\n", t, s);
    if (s > best) {
      best = s;
      winner = t;
    }
  }
  printf("Winner: Team %d (score %d)\n", winner, best);

  israeli_destroy(lock_id);
  exit(0);
}
