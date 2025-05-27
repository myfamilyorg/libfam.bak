CC      = gcc
CFLAGS  = -fPIC \
          -pedantic \
          -Wall \
          -Wextra \
	  -std=c99 \
          -O3 \
          -fno-stack-protector \
          -fno-builtin \
          -ffreestanding \
	  -Wno-attributes \
          -DSTATIC=static
TFLAGS  = -g -Isrc/include -Wno-attributes
LDFLAGS = -shared
FILTER ="*"
PAGE_SIZE=16384
MEMSAN ?= 0
CFLAGS += -DMEMSAN=$(MEMSAN)
TFLAGS += -DMEMSAN=$(MEMSAN)

# Directories
OBJDIR  = .obj
TOBJDIR = .tobj
LIBDIR  = lib
BINDIR  = bin
INCLDIR = src/include
SRCDIR  = src
CORE_SRCDIR = $(SRCDIR)/core
STORE_SRCDIR = $(SRCDIR)/store

# Source files, excluding test.c
CORE_SOURCES = $(filter-out $(CORE_SRCDIR)/test.c, $(wildcard $(CORE_SRCDIR)/*.c))
STORE_SOURCES = $(filter-out $(STORE_SRCDIR)/test.c, $(wildcard $(STORE_SRCDIR)/*.c))
SOURCES = $(CORE_SOURCES) $(STORE_SOURCES)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Test source files
CORE_TEST_SRC = $(CORE_SRCDIR)/test.c
STORE_TEST_SRC = $(STORE_SRCDIR)/test.c
TEST_SRC = $(CORE_TEST_SRC) $(STORE_TEST_SRC)
TEST_OBJ = $(patsubst $(SRCDIR)/%.c,$(TOBJDIR)/%.o,$(TEST_SRC))
TEST_BIN = $(BINDIR)/runtests

# Default target
all: $(LIBDIR)/libfam.so

# Rule for library objects
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(CFLAGS) -c $< -o $@

# Rule for test objects
$(TOBJDIR)/%.o: $(SRCDIR)/%.c | $(TOBJDIR)
	@mkdir -p $(@D)
	$(CC) -DPAGE_SIZE=$(PAGE_SIZE) -I$(INCLDIR) $(TFLAGS) -c $< -o $@

# Build shared library (excludes test.o)
$(LIBDIR)/libfam.so: $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(LIBDIR)/libfam.so | $(BINDIR)
	$(CC) $(TEST_OBJ) -I$(INCLDIR) -L$(LIBDIR) -lfam -L/usr/local/lib -lcriterion -Wno-overflow -o $@

# Create directories if they don't exist
$(OBJDIR) $(TOBJDIR) $(LIBDIR) $(BINDIR):
	mkdir -p $@

# Run tests
test: $(TEST_BIN)
	export CRITERION_TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Clean up
clean:
	rm -fr $(OBJDIR) $(TOBJDIR) $(LIBDIR)/*.so $(TEST_BIN)

# Phony targets
.PHONY: all test clean
