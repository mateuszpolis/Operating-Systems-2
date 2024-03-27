#define _GNU_SOURCE
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE (MAX_MSG_SIZE + 10)

typedef struct {
  pid_t client_pid;
  int num1;
  int num2;
} request_msg;

typedef struct {
  int result;
} response_msg;

#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   exit(EXIT_FAILURE))

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <server_queue_name>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char client_queue_name[64];
  snprintf(client_queue_name, sizeof(client_queue_name), "/%d", getpid());

  mqd_t mq_server, mq_client;
  struct mq_attr attr = {.mq_flags = 0,
                         .mq_maxmsg = MAX_MESSAGES,
                         .mq_msgsize = MAX_MSG_SIZE,
                         .mq_curmsgs = 0};

  // Create client queue for receiving responses
  mq_client = mq_open(client_queue_name, O_CREAT | O_RDONLY | O_NONBLOCK,
                      QUEUE_PERMISSIONS, &attr);
  if (mq_client == (mqd_t)-1)
    ERR("mq_open client");

  // Open server queue for sending requests
  mq_server = mq_open(argv[1], O_WRONLY);
  if (mq_server == (mqd_t)-1)
    ERR("mq_open server");

  ssize_t bytes_read;

  char msg_buffer[MSG_BUFFER_SIZE];
    int num1, num2;
    int resp;

  struct timespec ts;
  while (scanf("%d %d", &num1, &num2) == 2) {
    snprintf(msg_buffer, sizeof(msg_buffer), "%d %d %d", num1, num2, getpid());

        if (mq_send(mq_server, msg_buffer, strlen(msg_buffer) + 1, 0) == -1)
            ERR("mq_send to server");

    // Set timeout for mq_timedreceive
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 0; // Additional seconds not needed due to short wait
    ts.tv_nsec += 100 * 1000000; // 100 milliseconds converted to nanoseconds

    bytes_read =
        mq_timedreceive(mq_client, msg_buffer, strlen(msg_buffer), NULL, &ts);
    sscanf(msg_buffer, "%d", &resp);
    if (bytes_read == -1) {
      if (errno == ETIMEDOUT) {
        printf("Response timed out\n");
        break; // Explicitly terminate if no response within timeout
      } else {
        ERR("mq_timedreceive from server");
      }
    } else {
      printf("Result: %d\n", resp.result);
    }
  }

  // Cleanup
  mq_close(mq_client);
  mq_unlink(client_queue_name);
  mq_close(mq_server);

  return EXIT_SUCCESS;
}
