CC      = gcc
CFLAGS  = -fPIC \
          -std=c99 \
          -pedantic \
          -Wall \
          -Wextra \
          -O3 \
          -fno-stack-protector \
          -fno-builtin \
	  -ffreestanding \
	  -Wno-attributes \
	  -Wno-dollar-in-identifier-extension \
          -DSTATIC=static
TFLAGS  = -g -I/usr/local/include -Wno-attributes -Wno-dollar-in-identifier-extension
LDFLAGS = -shared
FILTER ="*"

# Directories
OBJDIR  = .obj
TOBJDIR = .tobj
LIBDIR  = lib
BINDIR  = bin
INCLDIR = src/include
SRCDIR  = src

# Source files, excluding test.c
SOURCES = $(filter-out $(SRCDIR)/core/test.c, $(wildcard $(SRCDIR)/core/*.c))
OBJECTS = $(patsubst $(SRCDIR)/core/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Test source file
TEST_SRC = $(SRCDIR)/core/test.c
TEST_OBJ = $(TOBJDIR)/test.o
TEST_BIN = $(BINDIR)/runtests

# Default target
all: $(LIBDIR)/libfam.so

# Rule for library objects
$(OBJDIR)/%.o: $(SRCDIR)/core/%.c $(INCLDIR)/%.h | $(OBJDIR)
	$(CC) -I$(INCLDIR) $(CFLAGS) -c $< -o $@

# Rule for test object
$(TOBJDIR)/%.o: $(SRCDIR)/core/test.c | $(TOBJDIR)
	$(CC) -I$(INCLDIR) $(TFLAGS) -c $< -o $@

# Build shared library (excludes test.o)
$(LIBDIR)/libfam.so: $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(LIBDIR)/libfam.so | $(BINDIR)
	$(CC) -L/usr/local/lib -Wno-overflow -lcriterion -I$(INCLDIR) $(TEST_OBJ) -L$(LIBDIR) -lfam -o $@

# Create directories if they don't exist
$(OBJDIR) $(TOBJDIR) $(LIBDIR) $(BINDIR):
	mkdir -p $@

# Run tests
test: $(TEST_BIN)
	export CRITERION_TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Clean up
clean:
	rm -fr $(OBJDIR)/*.o $(TOBJDIR)/*.o $(LIBDIR)/*.so $(TEST_BIN)

# Phony targets
.PHONY: all test clean
