CC      = gcc
CFLAGS  = -fPIC \
          -pedantic \
          -Wall \
          -Wextra \
          -std=c89 \
          -O3 \
          -fno-stack-protector \
          -fno-builtin \
          -ffreestanding \
	  -nostdlib \
          -Wno-attributes \
          -DSTATIC=static
TFLAGS  = -O1 -Isrc/include -Wno-attributes -DSTATIC=A
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS = -shared -nostdlib
else ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -shared
else
    $(error Unsupported platform: $(UNAME_S))
endif
FILTER  ="*"
PAGE_SIZE=16384
MEMSAN ?= 0
CFLAGS += -DMEMSAN=$(MEMSAN)
TFLAGS += -DMEMSAN=$(MEMSAN)

# Directories
OBJDIR  = .obj
TOBJDIR = .tobj
TEST_OBJDIR = .testobj
LIBDIR  = lib
BINDIR  = bin
INCLDIR = src/include
SRCDIR  = src
CORE_SRCDIR = $(SRCDIR)/core
STORE_SRCDIR = $(SRCDIR)/store
NET_SRCDIR = $(SRCDIR)/net

# Source files, excluding test.c
CORE_SOURCES = $(filter-out $(CORE_SRCDIR)/test.c, $(wildcard $(CORE_SRCDIR)/*.c))
STORE_SOURCES = $(filter-out $(STORE_SRCDIR)/test.c, $(wildcard $(STORE_SRCDIR)/*.c))
NET_SOURCES = $(filter-out $(NET_SRCDIR)/test.c, $(wildcard $(NET_SRCDIR)/*.c))
SOURCES = $(CORE_SOURCES) $(STORE_SOURCES) $(NET_SOURCES)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(TEST_OBJDIR)/%.o,$(SOURCES))

# Test source files
CORE_TEST_SRC = $(CORE_SRCDIR)/test.c
STORE_TEST_SRC = $(STORE_SRCDIR)/test.c
NET_TEST_SRC = $(NET_SRCDIR)/test.c
TEST_SRC = $(CORE_TEST_SRC) $(STORE_TEST_SRC) $(NET_TEST_SRC)
TEST_OBJ = $(patsubst $(SRCDIR)/%.c,$(TOBJDIR)/%.o,$(TEST_SRC))
TEST_BIN = $(BINDIR)/runtests
TEST_LIB = $(LIBDIR)/libfam_test.so

# Default target
all: $(LIBDIR)/libfam.so

# Rule for library objects (for 'all')
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(CFLAGS) -c $< -o $@

# Rule for test library objects (for 'test')
$(TEST_OBJDIR)/%.o: $(SRCDIR)/%.c | $(TEST_OBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(TFLAGS) -fPIC -c $< -o $@

# Rule for test objects
$(TOBJDIR)/%.o: $(SRCDIR)/%.c | $(TOBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(TFLAGS) -c $< -o $@

# Build shared library (for 'all')
$(LIBDIR)/libfam.so: $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# Build test shared library (for 'test')
$(TEST_LIB): $(TEST_OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(TEST_LIB) | $(BINDIR)
	$(CC) $(TEST_OBJ) -I$(INCLDIR) -L$(LIBDIR) -lfam_test -L/usr/local/lib -lcriterion -o $@

# Create directories if they don't exist
$(OBJDIR) $(TOBJDIR) $(TEST_OBJDIR) $(LIBDIR) $(BINDIR):
	@mkdir -p $@

# Run tests
test: $(TEST_BIN)
	export CRITERION_TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Clean up
clean:
	rm -fr $(OBJDIR) $(TOBJDIR) $(TEST_OBJDIR) $(LIBDIR)/*.so $(TEST_BIN)

# Phony targets
.PHONY: all test clean
