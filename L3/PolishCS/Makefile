# Compiler
CC=gcc

# Compiler flags
CFLAGS=-std=gnu99 -Wall

# Target executable name
TARGET=prog

# Default target
all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

# Clean target to remove executable and object files
clean:
	rm -f $(TARGET)

# Additional target to clean up resources such as shared memory and semaphore
clean-resources:
	ipcrm -m /shm_init_semaphore
	ipcrm -s /shared_memory
	echo "Resources cleaned up"

.PHONY: all clean clean-resources
