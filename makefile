# Nombre del ejecutable
TARGET = server

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LIBS = -lpthread

CFLAGS += -I. -Iestructuras

SRCS = $(wildcard *.c) $(wildcard estructuras/*.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean