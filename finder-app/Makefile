TARGET = writer
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = 

$(TARGET): $(SRCS) 
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
