CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(TARGET)