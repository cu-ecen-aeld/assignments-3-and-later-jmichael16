TARGET = writer
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror 
LDFLAGS = 

all: $(TARGET)

$(TARGET): $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o : %.c 
	$(CC) $(CFLAGS) -c $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
