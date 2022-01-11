TARGET = writer
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CC = gcc
CFLAGS = -g -Wall -Werror 
LDFLAGS = 

$(TARGET): $(OBJS) 
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o : %.c 
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
