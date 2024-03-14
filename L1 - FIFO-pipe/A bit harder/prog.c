#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEMP_FAILURE_RETRY(expression)        \
  ({                                          \
    long int _result;                         \
    do                                        \
      _result = (long int)(expression);       \
    while (_result == -1L && errno == EINTR); \
    _result;                                  \
  })

#define ERR(source)                                                \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

// MAX_BUFF must be in one byte range
#define MAX_BUFF 200

volatile sig_atomic_t last_signal = 0;

int sethandler(void (*f)(int), int sigNo)
{
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(sigNo, &act, NULL))
    return -1;
  return 0;
}

void sig_handler(int sig) { last_signal = sig; }

void sig_killme(int sig)
{
  if (rand() % 5 == 0)
    exit(EXIT_SUCCESS);
}

void sigchld_handler(int sig)
{
  pid_t pid;
  for (;;)
  {
    pid = waitpid(0, NULL, WNOHANG);
    if (0 == pid)
      return;
    if (0 >= pid)
    {
      if (ECHILD == errno)
        return;
      ERR("waitpid:");
    }
  }
}

void close_unused_pipes(int pipe_id, int n, int **pipes)
{
  for (int i = 0; i < n * 2; i++)
  {
    if (i != pipe_id && i - 1 != pipe_id)
    {
      if (TEMP_FAILURE_RETRY(close(pipes[i][0])))
        ERR("close");
      if (TEMP_FAILURE_RETRY(close(pipes[i][1])))
        ERR("close");
    }
  }
}

void child_work(int write_fd, int read_fd, int m)
{
  srand(getpid());
  char msg[16] = {0};
  int used_cards[m];
  for (int i = 0; i < m; i++)
  {
    used_cards[i] = 0;
  }
  for (int i = 0; i < m; i++)
  {
    if (TEMP_FAILURE_RETRY(read(read_fd, msg, sizeof(msg))) < 0)
    {
      if (errno == EPIPE)
      {
        if (TEMP_FAILURE_RETRY(close(write_fd)))
          ERR("close");
        if (TEMP_FAILURE_RETRY(close(read_fd)))
          ERR("close");
        exit(EXIT_SUCCESS);
      }
      else
      {
        ERR("read");
      }
    }
    if (strncmp(msg, "new_round", 9) == 0)
    {
      int r_number;
      while (used_cards[r_number = rand() % m] != 0)
        ;
      used_cards[r_number] = 1;
      int failure = rand() % 100;
      if (failure < 5)
      {
        r_number = -1;
      }
      memset(msg, 0, sizeof(msg));
      memcpy(msg, &r_number, sizeof(int));
      if (TEMP_FAILURE_RETRY(write(write_fd, msg, sizeof(msg)) !=
                             sizeof(msg)))
      {
        if (errno == EPIPE)
        {
          if (TEMP_FAILURE_RETRY(close(write_fd)))
            ERR("close");
          if (TEMP_FAILURE_RETRY(close(read_fd)))
            ERR("close");
          exit(EXIT_SUCCESS);
        }
        else
        {
          ERR("write");
        }
      }
      if (r_number == -1)
      {
        if (TEMP_FAILURE_RETRY(close(write_fd)))
          ERR("close");
        if (TEMP_FAILURE_RETRY(close(read_fd)))
          ERR("close");
        exit(EXIT_SUCCESS);
      }
    }
    else
    {
      ERR("wrong message");
    }
  }
}

void print_results(int n, int *points)
{
  printf("\nRESULTS\n\n");
  for (int i = 0; i < n; i++)
  {
    printf("Player %d: %d\n", i, points[i]);
  }
}

void parent_work(int n, int m, int **pipes)
{
  char msg[16];

  int *points;
  if (NULL == (points = (int *)malloc(n * sizeof(int))))
    ERR("malloc");
  for (int i = 0; i < n; i++)
  {
    points[i] = 0;
  }

  int alive[n];
  for (int i = 0; i < n; ++i)
  {
    alive[i] = 1;
  }

  while (m)
  {
    printf("\nNEW ROUND\n\n");
    strcpy(msg, "new_round");
    memset(msg + 9, 0, 7);
    for (int i = 0; i < n; i++)
    {
      if (alive[i] == 0)
      {
        continue;
      }
      if (TEMP_FAILURE_RETRY(write(pipes[i * 2 + 1][1], msg, sizeof(msg))) !=
          sizeof(msg))
      {
        if (errno == EPIPE)
        {
          if (TEMP_FAILURE_RETRY(close(pipes[i * 2][0])))
            ERR("close");
          if (TEMP_FAILURE_RETRY(close(pipes[i * 2 + 1][1])))
            ERR("close");
          exit(EXIT_SUCCESS);
        }
        else
        {
          ERR("write");
        }
      }
    }
    int cards[n];
    for (int i = 0; i < n; i++)
    {
      cards[i] = -1;
    }
    for (int i = 0; i < n; i++)
    {
      if (alive[i] == 0)
      {
        continue;
      }
      int number;
      memset(msg, 0, sizeof(msg));
      if (TEMP_FAILURE_RETRY(read(pipes[i * 2][0], msg, sizeof(msg))) !=
          sizeof(msg))
      {
        if (errno == EPIPE)
        {
          if (TEMP_FAILURE_RETRY(close(pipes[i * 2][0])))
            ERR("close");
          if (TEMP_FAILURE_RETRY(close(pipes[i * 2 + 1][1])))
            ERR("close");
          exit(EXIT_SUCCESS);
        }
        else
        {
          ERR("read");
        }
      }
      memcpy(&number, msg, sizeof(int));
      if (number == -1)
      {
        printf("Player %d is dead\n", i);
        alive[i] = 0;
        continue;
      }
      cards[i] = number;
      printf("Parent received: %d from player: %d\n", number, i);
    }
    int max = INT_MIN;
    int max_count = 1;
    for (int i = 0; i < n; i++)
    {
      if (cards[i] > max)
      {
        max = cards[i];
        max_count = 1;
      }
      else if (cards[i] == max)
      {
        max_count++;
      }
    }
    for (int i = 0; i < n; i++)
    {
      if (cards[i] == max)
      {
        points[i] += (int)floor(n / max_count);
      }
    }
    m--;
  }

  print_results(n, points);
  free(points);
}

void create_children(int n, int m, int **pipes)
{
  for (int i = 0; i < n; i++)
  {
    switch (fork())
    {
    case 0:
      close_unused_pipes(i * 2, n, pipes);
      child_work(pipes[i * 2][1], pipes[i * 2 + 1][0], m);
      exit(EXIT_SUCCESS);

    case -1:
      ERR("Fork:");
    }
  }
}

void usage(char *name)
{
  fprintf(stderr, "USAGE: %s n\n", name);
  fprintf(stderr, "2<=n<=5 - number of players\n");
  fprintf(stderr, "5<=m<=10 - number of cards\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  int N, M;
  if (3 != argc)
    usage(argv[0]);
  N = atoi(argv[1]);
  M = atoi(argv[2]);
  if (N < 2 || N > 5)
    usage(argv[0]);
  if (M < 5 || M > 10)
    usage(argv[0]);

  if (sethandler(SIG_IGN, SIGINT))
    ERR("Setting SIGINT handler");
  if (sethandler(SIG_IGN, SIGPIPE))
    ERR("Setting SIGINT handler");
  if (sethandler(sigchld_handler, SIGCHLD))
    ERR("Setting parent SIGCHLD:");

  int **pipes;
  if (NULL == (pipes = (int **)malloc(2 * N * sizeof(int *))))
    ERR("malloc");
  for (int i = 0; i < N * 2; i++)
    if (NULL == (pipes[i] = (int *)malloc(2 * sizeof(int))))
      ERR("malloc");

  for (int i = 0; i < N * 2; i++)
  {
    if (pipe(pipes[i]))
      ERR("pipe");
  }

  create_children(N, M, pipes);
  parent_work(N, M, pipes);

  for (int i = 0; i < N * 2; i++)
  {
    if (TEMP_FAILURE_RETRY(close(pipes[i][0])))
      ERR("close");
    if (TEMP_FAILURE_RETRY(close(pipes[i][1])))
      ERR("close");
  }

  for (int i = 0; i < N; i++)
  {
    free(pipes[i]);
  }
  free(pipes);

  return EXIT_SUCCESS;
}
