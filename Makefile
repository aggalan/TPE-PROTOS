# Makefile

CC           := gcc
PKG_CONFIG   := pkg-config

# Try to get Check flags; if pkg-config fails, fall back to -lcheck -pthread
CHECK_CFLAGS := $(shell $(PKG_CONFIG) --cflags check 2>/dev/null)
CHECK_LIBS   := $(shell $(PKG_CONFIG) --libs   check 2>/dev/null) -pthread

CFLAGS       := -Wall -Wextra -std=gnu99 -g -I src -pthread $(CHECK_CFLAGS)
LDFLAGS      := -pthread $(CHECK_LIBS)

SRCDIR       := src
ALL_SRCS     := $(wildcard $(SRCDIR)/*.c)
TEST_SRCS    := $(wildcard $(SRCDIR)/*_test.c)
PROD_SRCS    := $(filter-out $(TEST_SRCS),$(ALL_SRCS))

PROD_OBJS    := $(PROD_SRCS:.c=.o)
TEST_OBJS    := $(TEST_SRCS:.c=.o)

TARGET       := main
TEST_TARGET  := test_runner

.PHONY: all test clean

all: $(TARGET)

# Link the main proxy
$(TARGET): $(PROD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build & link the test runner
$(TEST_TARGET): $(PROD_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile any .c → .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests (will rebuild test_runner if needed)
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean up
clean:
	rm -f $(SRCDIR)/*.o $(TARGET) $(TEST_TARGET)
