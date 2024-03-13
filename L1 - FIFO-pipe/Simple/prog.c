#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
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

void process_work(int read_p, int write_p, int is_parent)
{
  srand(time(NULL) ^ (getpid() << 16));

  char buf[MAX_BUFF + 1];
  int num, bytesRead;

  if (is_parent == 1)
  {
    printf("PID: %d, Sending: %d\n", getpid(), 1);
    snprintf(buf, sizeof(buf), "%d", 1);
    TEMP_FAILURE_RETRY(write(write_p, buf, strlen(buf) + 1));
  }

  while ((bytesRead = TEMP_FAILURE_RETRY(read(read_p, buf, MAX_BUFF))) > 0)
  {
    buf[bytesRead] = '\0';
    num = atoi(buf);
    if (num == 0 || errno == EPIPE)
    {
      printf("PID: %d, Terminating due to 0 value\n", getpid());
      if (close(read_p))
            ERR("close");
        exit(EXIT_SUCCESS);
    }
    printf("PID: %d, Received: %d\n", getpid(), num);

    num += (rand() % 21) - 10;

    printf("PID: %d, Sending: %d\n", getpid(), num);
    snprintf(buf, sizeof(buf), "%d", num);
    TEMP_FAILURE_RETRY(write(write_p, buf, strlen(buf) + 1));
  }
}

int main()
{
  int pipe1[2], pipe2[2], pipe3[2]; // Pipes for communication
  if (pipe(pipe1) == -1 || pipe(pipe2) == -1 || pipe(pipe3) == -1)
    ERR("pipe");

  pid_t pid1, pid2;

  // Creating first child process
  if ((pid1 = fork()) == -1)
    ERR("fork");

  if (pid1 == 0)
  { // Child 1
    if (close(pipe1[1]) || close(pipe2[0]) || close(pipe3[0]) ||
        close(pipe3[1]))
      ERR("close pipe");

    process_work(pipe1[0], pipe2[1], 0);

    if (close(pipe1[0]) || close(pipe2[1]))
      ERR("close pipe");
    exit(EXIT_SUCCESS);
  }

  // Creating second child process
  if ((pid2 = fork()) == -1)
    ERR("fork");

  if (pid2 == 0)
  { // Child 2
    if (close(pipe1[0]) || close(pipe1[1]) || close(pipe2[1]) ||
        close(pipe3[0]))
      ERR("close pipe");

    process_work(pipe2[0], pipe3[1], 0);

    if (close(pipe2[0]) || close(pipe3[1]))
      ERR("close pipe");
    exit(EXIT_SUCCESS);
  }

  // Parent process
  if (close(pipe1[0]) || close(pipe3[1]) || close(pipe2[0]) || close(pipe2[1]))
    ERR("close pipe");

  process_work(pipe3[0], pipe1[1], 1);

  if (close(pipe1[1]) || close(pipe3[0]))
    ERR("close pipe");

  waitpid(pid1, NULL, 0);
  waitpid(pid2, NULL, 0);

  return EXIT_SUCCESS;
}
