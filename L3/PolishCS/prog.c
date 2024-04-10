#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), \
   exit(EXIT_FAILURE))

#define SHM_SIZE 1024
#define SHM_NAME "/shared_memory"
#define SHMEM_SEMAPHORE_NAME "/shm_init_semaphore"

typedef struct {
  pthread_mutex_t mutex;
  int process_counter;
} SharedMemory;

int main(int argc, char *argv[]) {
  int shm_fd;
  sem_t *sem;

  printf("Process started\n");

  // Create or open the named semaphore
  if ((sem = sem_open(SHMEM_SEMAPHORE_NAME, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) {
    if(errno == EEXIST) {
      // If the semaphore already exists, open it
      if ((sem = sem_open(SHMEM_SEMAPHORE_NAME, 0)) == SEM_FAILED)
        ERR("sem_open");
    } else {
      ERR("sem_open");
    }
  }

  printf("Semaphore opened\n");

  if(sem_wait(sem) < 0)
    ERR("sem_wait");

  printf("Semaphore acquired\n");

  // Create or open the shared memory
  if ((shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666)) == -1)
    ERR("shm_open");

  printf ("Shared memory opened\n");

printf("shm_fd: %d\n", shm_fd); // Print the value of shm_fd

if (ftruncate(shm_fd, SHM_SIZE == -1)) {
    perror("ftruncate");
    ERR("ftruncate");
}


  SharedMemory *shm_ptr;
  if ((shm_ptr = (SharedMemory *)mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE,
                                      MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    ERR("mmap");

  // Initialize mutex and process counter in shared memory
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&(shm_ptr->mutex), &mutex_attr);
  shm_ptr->process_counter = 1; // First process initializing shared memory

  pthread_mutexattr_destroy(&mutex_attr);

  sem_post(sem); // Release the semaphore for other processes

  printf("Number of cooperating processes: %d\n", shm_ptr->process_counter);

  // Sleep for 2 seconds
  sleep(2);

  // Decrement the process counter and destroy shared memory if this is the last process
  pthread_mutex_lock(&(shm_ptr->mutex));
  shm_ptr->process_counter--;
  if (shm_ptr->process_counter == 0) {
    pthread_mutex_destroy(&(shm_ptr->mutex));
    shm_unlink(SHM_NAME);
  }
  pthread_mutex_unlock(&(shm_ptr->mutex));

  munmap(shm_ptr, sizeof(SharedMemory));
  sem_close(sem);
  sem_unlink(SHMEM_SEMAPHORE_NAME);

  return EXIT_SUCCESS;
}
