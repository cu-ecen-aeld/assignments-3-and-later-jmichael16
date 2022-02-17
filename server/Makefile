SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = 
TARGET = aesdsocket

default: $(TARGET)
all: $(TARGET)

$(TARGET): $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o : %.c 
	$(CC) $(CFLAGS) -c $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f valgrind.out.txt*

debug:
	make all
	rm -f valgrind.out.txt*
	valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./aesdsocket -d
