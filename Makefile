# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c99 -D_POSIX_C_SOURCE=200112L
LDFLAGS = -lcurl -lpthread

# Target executable
TARGET = downloader

# Source files
SRCS = main.c downloader.c
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = downloader.h

# Default target
all: $(TARGET)

# Build executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile source files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET) part_*.tmp *.bin *.zip

# Clean and rebuild
rebuild: clean all

# Run with test URL
test: $(TARGET)
	./$(TARGET) "http://212.183.159.230/5MB.zip" test.bin 4

.PHONY: all clean rebuild test
