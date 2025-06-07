CC      = gcc
CSTYLE  = -pedantic \
          -Wall \
          -Wextra \
	  -std=c89 \
	  -Werror
CFLAGS  = -fPIC \
	  $(CSTYLE) \
          -O3 \
          -fno-stack-protector \
          -fno-builtin \
          -Wno-attributes
TFLAGS  = $(CSTYLE) -fno-builtin -O1 -Isrc/include -Wno-attributes
TCFLAGS = -O1 -Isrc/include -Wno-attributes
UNAME_S := $(shell uname -s)
LDFLAGS = -shared -nostdlib -ffreestanding
FILTER  = "*"
PAGE_SIZE = 16384
MEMSAN ?= 0
CFLAGS += -DMEMSAN=$(MEMSAN)
TFLAGS += -DMEMSAN=$(MEMSAN)
TCFLAGS += -DMEMSAN=$(MEMSAN)

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
ASM_SRCDIR = $(SRCDIR)/asm

# Source files, excluding test.c
CORE_SOURCES = $(filter-out $(CORE_SRCDIR)/test.c, $(wildcard $(CORE_SRCDIR)/*.c))
STORE_SOURCES = $(filter-out $(STORE_SRCDIR)/test.c, $(wildcard $(STORE_SRCDIR)/*.c))
NET_SOURCES = $(filter-out $(NET_SRCDIR)/test.c, $(wildcard $(NET_SRCDIR)/*.c))
SOURCES = $(CORE_SOURCES) $(STORE_SOURCES) $(NET_SOURCES)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(TEST_OBJDIR)/%.o,$(SOURCES))

# Assembly source files
ASM_SOURCES = $(wildcard $(ASM_SRCDIR)/*.S)
ASM_OBJECTS = $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.o,$(ASM_SOURCES))
TEST_ASM_OBJECTS = $(patsubst $(SRCDIR)/%.S,$(TEST_OBJDIR)/%.o,$(ASM_SOURCES))

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

# Rule for library objects (C files for 'all')
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(CFLAGS) -DSTATIC=static -c $< -o $@

# Rule for assembly objects (for 'all')
$(OBJDIR)/%.o: $(SRCDIR)/%.S | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

# Rule for test library objects (C files for 'test')
$(TEST_OBJDIR)/%.o: $(SRCDIR)/%.c | $(TEST_OBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(TFLAGS) -DSTATIC= -fPIC -c $< -o $@

# Rule for test assembly objects (for 'test')
$(TEST_OBJDIR)/%.o: $(SRCDIR)/%.S | $(TEST_OBJDIR)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

# Rule for test objects (C files for runtests)
$(TOBJDIR)/%.o: $(SRCDIR)/%.c | $(TOBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(TCFLAGS) -DSTATIC= -c $< -o $@

# Build shared library (for 'all')
$(LIBDIR)/libfam.so: $(OBJECTS) $(ASM_OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(ASM_OBJECTS)

# Build test shared library (for 'test')
$(TEST_LIB): $(TEST_OBJECTS) $(TEST_ASM_OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJECTS) $(TEST_ASM_OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(TEST_LIB) | $(BINDIR)
	$(CC) $(TEST_OBJ) -I$(INCLDIR) -L$(LIBDIR) ./src/test/main.c -lfam_test -o $@

# Create directories if they don't exist
$(OBJDIR) $(TOBJDIR) $(TEST_OBJDIR) $(LIBDIR) $(BINDIR):
	@mkdir -p $@

# Run tests
test: $(TEST_BIN)
	export TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Clean up
clean:
	rm -fr $(OBJDIR) $(TOBJDIR) $(TEST_OBJDIR) $(LIBDIR)/*.so $(BINDIR)/*

# Phony targets
.PHONY: all test clean
