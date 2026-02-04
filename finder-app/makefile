
CROSS_COMPILE=aarch64-none-linux-gnu

CC ?= $(CROSS_COMPILE)-gcc
CFLAGS ?= -Wall -Wextra -g

all: writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

# Clean target - removes executable and object files
clean:
	rm -f writer
	rm -f *.o

.PHONY: all clean