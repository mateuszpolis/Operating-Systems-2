#define _GNU_SOURCE
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)

#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   exit(EXIT_FAILURE))

volatile sig_atomic_t terminate = 0;

typedef struct {
  pid_t client_pid;
  int num1;
  int num2;
} request_msg;

typedef struct {
  int result;
} response_msg;

void sigint_handler(int sig) { terminate = 1; }

void setup_sigint_handling() {
  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = sigint_handler;
  if (sigaction(SIGINT, &action, NULL) != 0)
    ERR("sigaction");
}

void process_request(mqd_t mq, char operation) {
  request_msg req;
  response_msg resp;
  mqd_t client_mq;
  char client_queue_name[64];
  ssize_t bytes_read;
  int result = 0;

  bytes_read = mq_receive(mq, (char *)&req, sizeof(req), NULL);
  if (bytes_read < 0) {
    perror("Error receiving message");
    return;
  }

  switch (operation) {
  case 's':
    result = req.num1 + req.num2;
    break;
  case 'd':
    if (req.num2 == 0) {
      fprintf(stderr, "Division by zero\n");
      return;
    }
    result = req.num1 / req.num2;
    break;
  case 'm':
    if (req.num2 == 0) {
      fprintf(stderr, "Modulo by zero\n");
      return;
    }
    result = req.num1 % req.num2;
    break;
  default:
    fprintf(stderr, "Unknown operation\n");
    return;
  }

  snprintf(client_queue_name, sizeof(client_queue_name), "/%d", req.client_pid);
  client_mq = mq_open(client_queue_name, O_WRONLY);
  if (client_mq == (mqd_t)-1) {
    perror("Opening client queue");
    return;
  }

  resp.result = result;
  if (mq_send(client_mq, (const char *)&resp, sizeof(resp), 0) < 0) {
    perror("Sending response to client");
  }

  mq_close(client_mq);
}

int main() {
  pid_t pid = getpid();
  char queue_name_s[64], queue_name_d[64], queue_name_m[64];
  snprintf(queue_name_s, sizeof(queue_name_s), "/%d_s", pid);
  snprintf(queue_name_d, sizeof(queue_name_d), "/%d_d", pid);
  snprintf(queue_name_m, sizeof(queue_name_m), "/%d_m", pid);

  mqd_t mq_s, mq_d, mq_m;
  struct mq_attr attr = {.mq_flags = 0,
                         .mq_maxmsg = MAX_MESSAGES,
                         .mq_msgsize = MAX_MSG_SIZE,
                         .mq_curmsgs = 0};

  // Creating message queues
  if ((mq_s = mq_open(queue_name_s, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS,
                      &attr)) == (mqd_t)-1)
    ERR("mq_open s");
  if ((mq_d = mq_open(queue_name_d, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS,
                      &attr)) == (mqd_t)-1)
    ERR("mq_open d");
  if ((mq_m = mq_open(queue_name_m, O_CREAT | O_RDONLY, QUEUE_PERMISSIONS,
                      &attr)) == (mqd_t)-1)

    ERR("mq_open m");

  printf("Server Queues:\n%s\n%s\n%s\n", queue_name_s, queue_name_d,
         queue_name_m);

  setup_sigint_handling();

  while (!terminate) {
    struct mq_attr attr;
    request_msg req;
    ssize_t bytes_read;
    mq_getattr(mq_s, &attr);
    if (attr.mq_curmsgs > 0) {
      bytes_read = mq_receive(mq_s, (char *)&req, sizeof(req), NULL);
      if (bytes_read >= 0) {
        process_request(mq_s, 's');
      } else if (errno != EAGAIN) {
        perror("mq_receive s");
      }
    }

    mq_getattr(mq_d, &attr);
    if (attr.mq_curmsgs > 0) {
      bytes_read = mq_receive(mq_d, (char *)&req, sizeof(req), NULL);
      if (bytes_read >= 0) {
        process_request(mq_d, 'd');
      } else if (errno != EAGAIN) {
        perror("mq_receive d");
      }
    }

    mq_getattr(mq_m, &attr);
    if (attr.mq_curmsgs > 0) {
      bytes_read = mq_receive(mq_m, (char *)&req, sizeof(req), NULL);
      if (bytes_read >= 0) {
        process_request(mq_m, 'm');
      } else if (errno != EAGAIN) {
        perror("mq_receive m");
      }
    }

    usleep(100000); 
  }

  if (mq_close(mq_s) == -1)
    ERR("mq_close s");
  if (mq_unlink(queue_name_s) == -1)
    ERR("mq_unlink s");

  if (mq_close(mq_d) == -1)
    ERR("mq_close d");
  if (mq_unlink(queue_name_d) == -1)
    ERR("mq_unlink d");

  if (mq_close(mq_m) == -1)
    ERR("mq_close m");
  if (mq_unlink(queue_name_m) == -1)
    ERR("mq_unlink m");

  printf("Server terminated. Queues removed.\n");

  return EXIT_SUCCESS;
}
