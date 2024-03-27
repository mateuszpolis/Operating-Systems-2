# Compiler
CC=gcc

# Compiler flags
CFLAGS=-std=gnu99 -Wall

# Target executable names
SERVER_TARGET=server
CLIENT_TARGET=client

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Server target
$(SERVER_TARGET): $(SERVER_TARGET).c
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_TARGET).c

# Client target
$(CLIENT_TARGET): $(CLIENT_TARGET).c
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_TARGET).c

# Clean target to remove executables and object files
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
