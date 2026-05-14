TARGET_EXEC = chip8

SRCS := $(wildcard *.c)

CFLAGS := -Wall -Wextra -std=c11
LIBS := -lSDL3

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET_EXEC) $(LIBS)

clean:
	rm $(TARGET_EXEC)
