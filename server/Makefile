CC ?= $(CROSS_COMPILE)gcc
TARGET = aesdsocket

all: $(TARGET)
default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)