#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MSG_SIZE (PIPE_BUF - sizeof(pid_t))
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int fifo, file;
    if (argc != 3)
        usage(argv[0]);

    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0)
        if (errno != EEXIST)
            ERR("create fifo");
    if ((fifo = open(argv[1], O_WRONLY)) < 0)
        ERR("open");
    if ((file = open(argv[2], O_RDONLY)) < 0)
        ERR("file open");
    write_to_fifo(fifo, file);
    if (close(file) < 0)
        perror("Close fifo:");
    if (close(fifo) < 0)
        perror("Close fifo:");
    return EXIT_SUCCESS;
}
