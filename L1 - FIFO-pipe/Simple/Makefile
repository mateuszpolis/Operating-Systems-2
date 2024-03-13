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

.PHONY: all clean
