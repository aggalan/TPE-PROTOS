# ----------- Toolchain -------------------------------------------------
CC           := gcc
PKG_CONFIG   := pkg-config

# Try to get Check flags; if pkg-config fails, fall back to -lcheck -pthread
CHECK_CFLAGS := $(shell $(PKG_CONFIG) --cflags check 2>/dev/null)
CHECK_LIBS   := $(shell $(PKG_CONFIG) --libs   check 2>/dev/null) -pthread

# ----------- Project layout -------------------------------------------
SRCDIR       := src

# All source files, found recursively (works for src/greeting/*.c, etc.)
ALL_SRCS     := $(shell find $(SRCDIR) -name '*.c')

# Split test vs production sources by “*_test.c” naming convention
TEST_SRCS    := $(filter %_test.c,$(ALL_SRCS))
PROD_SRCS    := $(filter-out $(TEST_SRCS),$(ALL_SRCS))

# Map *.c → *.o (object files live next to their sources)
PROD_OBJS    := $(PROD_SRCS:.c=.o)
TEST_OBJS    := $(TEST_SRCS:.c=.o)

# Collect every directory under src and turn each into a -I flag
SUBDIRS      := $(shell find $(SRCDIR) -type d)
INCLUDE_DIRS := $(addprefix -I,$(SUBDIRS))

# ----------- Flags -----------------------------------------------------
CFLAGS       := -Wall -Wextra -std=gnu99 -g $(INCLUDE_DIRS) -pthread $(CHECK_CFLAGS)
LDFLAGS      := -pthread $(CHECK_LIBS)

# ----------- Targets ---------------------------------------------------
TARGET       := main
TEST_TARGET  := test_runner

.PHONY: all test clean

all: $(TARGET)

# Main proxy ------------------------------------------------------------
$(TARGET): $(PROD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test runner -----------------------------------------------------------
$(TEST_TARGET): $(PROD_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generic compile rule --------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests (rebuilds test_runner if needed) ----------------------------
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# House-keeping ---------------------------------------------------------
clean:
	find $(SRCDIR) -name '*.o' -delete
	rm -f $(TARGET) $(TEST_TARGET)
