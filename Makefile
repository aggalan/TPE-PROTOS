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

# admin client source (contains its own main)
ADMIN_CLIENT_SRC := $(SRCDIR)/admin/admin_client.c

# Split test vs production sources by “*_test.c” naming convention
TEST_SRCS    := $(filter %_test.c,$(ALL_SRCS))
# Production sources exclude unit tests and the admin client main
PROD_SRCS    := $(filter-out $(TEST_SRCS) $(ADMIN_CLIENT_SRC),$(ALL_SRCS))

# Map *.c → *.o (object files live next to their sources)
PROD_OBJS    := $(PROD_SRCS:.c=.o)
TEST_OBJS    := $(TEST_SRCS:.c=.o)

# Objects used to build the main proxy executable
MAIN_OBJS    := $(PROD_OBJS)

# Admin client specific objects -----------------------------------------
#   – COMMON_OBJS: everything the proxy uses **except** src/main.o
COMMON_OBJS  := $(filter-out $(SRCDIR)/main.o $(ADMIN_CLIENT_SRC:.c=.o),$(PROD_OBJS))
ADMIN_CLIENT_OBJS := $(COMMON_OBJS) $(ADMIN_CLIENT_SRC:.c=.o)

# Collect every directory under src and turn each into a -I flag
SUBDIRS      := $(shell find $(SRCDIR) -type d)
INCLUDE_DIRS := $(addprefix -I,$(SUBDIRS))

# ----------- Flags -----------------------------------------------------
# Optional DEBUG build: pass DEBUG=1 (or any non-empty value) to enable
ifeq ($(strip $(DEBUG)),)
  DEBUG_FLAGS :=
else
  DEBUG_FLAGS := -DDEBUG -g
endif

CFLAGS       := -Wall -Wextra -std=gnu99 $(DEBUG_FLAGS) $(INCLUDE_DIRS) -pthread $(CHECK_CFLAGS)
LDFLAGS      := -pthread $(CHECK_LIBS)

# ----------- Targets ---------------------------------------------------
TARGET       := main
TEST_TARGET  := test_runner
ADMIN_TARGET := admin_client

.PHONY: all test clean $(ADMIN_TARGET)

all: $(TARGET)

# Main proxy ------------------------------------------------------------
$(TARGET): $(MAIN_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test runner -----------------------------------------------------------
$(TEST_TARGET): $(MAIN_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Admin management client ----------------------------------------------
$(ADMIN_TARGET): $(ADMIN_CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generic compile rule --------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests -------------------------------------------------------------
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# House-keeping ---------------------------------------------------------
clean:
	find $(SRCDIR) -name '*.o' -delete
	rm -f $(TARGET) $(TEST_TARGET) $(ADMIN_TARGET)
