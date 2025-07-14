# ----------- Toolchain -------------------------------------------------
CC           := gcc
PKG_CONFIG   := pkg-config

# Try to get Check flags; if pkg-config fails, fall back to -lcheck -pthread
CHECK_CFLAGS := $(shell $(PKG_CONFIG) --cflags check 2>/dev/null)
CHECK_LIBS   := $(shell $(PKG_CONFIG) --libs   check 2>/dev/null) -pthread

# ----------- Project layout -------------------------------------------
SRCDIR       := src

# Fast file discovery using wildcards (much faster than find)
PROD_SRCS := $(wildcard $(SRCDIR)/*.c) \
             $(wildcard $(SRCDIR)/*/*.c)

# Exclude test files and admin client
TEST_SRCS    := $(filter %_test.c,$(PROD_SRCS))
ADMIN_CLIENT_SRC := $(SRCDIR)/admin/admin_client.c
PROD_SRCS    := $(filter-out $(TEST_SRCS) $(ADMIN_CLIENT_SRC),$(PROD_SRCS))

# Map *.c → *.o (object files live next to their sources)
PROD_OBJS    := $(PROD_SRCS:.c=.o)
TEST_OBJS    := $(TEST_SRCS:.c=.o)

# Objects used to build the main proxy executable
MAIN_OBJS    := $(PROD_OBJS)

# Admin client specific objects
COMMON_OBJS  := $(filter-out $(SRCDIR)/main.o $(ADMIN_CLIENT_SRC:.c=.o),$(PROD_OBJS))
ADMIN_CLIENT_OBJS := $(COMMON_OBJS) $(ADMIN_CLIENT_SRC:.c=.o)

# Include directories - use wildcard for speed
INCLUDE_DIRS := -I$(SRCDIR) $(addprefix -I,$(wildcard $(SRCDIR)/*/))

# ----------- Flags -----------------------------------------------------
# Optional DEBUG build: pass DEBUG=1 (or any non-empty value) to enable
ifeq ($(strip $(DEBUG)),)
  DEBUG_FLAGS :=
else
  DEBUG_FLAGS := -DDEBUG -g
endif

# Add ASan flags when SANITIZE=1
ifeq ($(SANITIZE),1)
  CFLAGS  += -fsanitize=address -fno-omit-frame-pointer -g
  LDFLAGS += -fsanitize=address
endif


CFLAGS       := -Wall -Wextra -std=gnu99 $(DEBUG_FLAGS) $(INCLUDE_DIRS) -pthread $(CHECK_CFLAGS)
LDFLAGS      := -pthread $(CHECK_LIBS)

# Enable parallel builds
MAKEFLAGS += -j$(shell nproc 2>/dev/null || echo 4)

# ----------- Targets ---------------------------------------------------
TARGET       := main
TEST_TARGET  := test_runner
ADMIN_TARGET := admin_client

.PHONY: all test clean $(ADMIN_TARGET)

all: $(TARGET)

# Main proxy ------------------------------------------------------------
$(TARGET): $(MAIN_OBJS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test runner -----------------------------------------------------------
$(TEST_TARGET): $(MAIN_OBJS) $(TEST_OBJS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Admin management client ----------------------------------------------
$(ADMIN_TARGET): $(ADMIN_CLIENT_OBJS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generic compile rule with dependency generation
%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files
-include $(PROD_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

# Run tests -------------------------------------------------------------
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# House-keeping ---------------------------------------------------------
clean:
	@echo "Cleaning build artifacts..."
	@find $(SRCDIR) -name '*.o' -delete
	@find $(SRCDIR) -name '*.d' -delete
	@rm -f $(TARGET) $(TEST_TARGET) $(ADMIN_TARGET)
	@echo "Clean completed!"

check-leaks: clean
	$(MAKE) SANITIZE=1
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./main