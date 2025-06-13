# Compiler and tools
CC = gcc
GCOV = gcov
UNAME_S := $(shell uname -s)

# Directories
SRCDIR	 = src
INCLDIR	= src/include
LIBDIR	 = lib
BINDIR	 = bin
OBJDIR	 = .obj
TEST_OBJDIR = .testobj
TOBJDIR	= .tobj
COV_OBJDIR = .covobj
COV_TOBJDIR = .covtobj
ASM_DIR	= $(SRCDIR)/asm
SRC_DIRS   = core store net crypto  # Add new source directories here

# Source and object files
C_SOURCES   = $(foreach dir,$(SRC_DIRS),$(filter-out $(SRCDIR)/$(dir)/test.c,$(wildcard $(SRCDIR)/$(dir)/*.c)))
ASM_SOURCES = $(wildcard $(ASM_DIR)/*.S)
OBJECTS	 = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SOURCES)) $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.o,$(ASM_SOURCES))
TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(TEST_OBJDIR)/%.o,$(C_SOURCES)) $(patsubst $(SRCDIR)/%.S,$(TEST_OBJDIR)/%.o,$(ASM_SOURCES))
COV_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(COV_OBJDIR)/%.o,$(C_SOURCES))
COV_TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(COV_TOBJDIR)/%.o,$(TEST_SRC))

# Test sources and objects
TEST_SRC	= $(foreach dir,$(SRC_DIRS),$(SRCDIR)/$(dir)/test.c)
TEST_OBJ	= $(patsubst $(SRCDIR)/%.c,$(TOBJDIR)/%.o,$(TEST_SRC))
COV_TEST_OBJ = $(patsubst $(SRCDIR)/%.c,$(COV_TOBJDIR)/%.o,$(TEST_SRC))
TEST_LIB	= $(LIBDIR)/libfam_test.so
COV_TEST_LIB = $(LIBDIR)/libfam_test_cov.a
TEST_BIN	= $(BINDIR)/runtests
COV_TEST_BIN = $(BINDIR)/runtests_cov
COV_TEST_MAIN_OBJ = $(COV_TOBJDIR)/test/main.o

# Common configuration
PAGE_SIZE   = 16384
MEMSAN	 ?= 0
FILTER	 ?= "*"

# Common flags
COMMON_FLAGS = -pedantic \
			   -Wall \
			   -Wextra \
			   -std=c89 \
			   -Werror \
			   -I$(INCLDIR) \
			   -D_FORTIFY_SOURCE=0 \
			   -fno-builtin \
			   -fno-stack-protector \
			   -Wno-attributes \
			   -DPAGE_SIZE=$(PAGE_SIZE) \
			   -DMEMSAN=$(MEMSAN)

# Coverage flags
COV_FLAGS = -fprofile-arcs -ftest-coverage -O0 -fprofile-dir=$(COV_OBJDIR)

# Specific flags
LIB_CFLAGS		= $(COMMON_FLAGS) -fPIC -O3 -DSTATIC=static
TEST_CFLAGS	   = $(COMMON_FLAGS) -fPIC -O1 -DSTATIC= -DTEST=1
TEST_BINARY_CFLAGS = $(COMMON_FLAGS) -ffreestanding -nostdlib -O1 -DSTATIC= -DTEST=1
COV_LIB_CFLAGS	= $(COMMON_FLAGS) $(COV_FLAGS) -DSTATIC=static
COV_TEST_CFLAGS   = $(COMMON_FLAGS) $(COV_FLAGS) -DSTATIC= -DTEST=1
COV_TEST_BINARY_CFLAGS = $(COMMON_FLAGS) $(COV_FLAGS) -DSTATIC= -DTEST=1 -DCOVERAGE
LDFLAGS		   = -shared -nostdlib -ffreestanding
COV_LDFLAGS	   = -static-libgcc

# Default target
all: $(LIBDIR)/libfam.so

# Create directories
$(OBJDIR) $(TEST_OBJDIR) $(TOBJDIR) $(COV_OBJDIR) $(COV_TOBJDIR) $(LIBDIR) $(BINDIR):
	@mkdir -p $@

# Rules for main library objects
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.S | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

# Rules for test library objects
$(TEST_OBJDIR)/%.o: $(SRCDIR)/%.c | $(TEST_OBJDIR)
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_OBJDIR)/%.o: $(SRCDIR)/%.S | $(TEST_OBJDIR)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

# Rules for test binary objects
$(TOBJDIR)/%.o: $(SRCDIR)/%.c | $(TOBJDIR)
	@mkdir -p $(@D)
	$(CC) $(TEST_BINARY_CFLAGS) -c $< -o $@

# Rules for coverage library objects
$(COV_OBJDIR)/%.o: $(SRCDIR)/%.c | $(COV_OBJDIR)
	@mkdir -p $(@D)
	$(CC) $(COV_LIB_CFLAGS) -c $< -o $@

$(COV_OBJDIR)/%.o: $(SRCDIR)/%.S | $(COV_OBJDIR)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

# Rules for coverage test objects
$(COV_TOBJDIR)/%.o: $(SRCDIR)/%.c | $(COV_TOBJDIR)
	@mkdir -p $(@D)
	$(CC) $(COV_TEST_CFLAGS) -c $< -o $@

# Rule for coverage test main object
$(COV_TEST_MAIN_OBJ): $(SRCDIR)/test/main.c | $(COV_TOBJDIR)
	@mkdir -p $(@D)
	$(CC) $(COV_TEST_BINARY_CFLAGS) -c $< -o $@

# Build main library
$(LIBDIR)/libfam.so: $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# Build test library
$(TEST_LIB): $(TEST_OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(TEST_LIB) | $(BINDIR)
	$(CC) $(TEST_OBJ) -I$(INCLDIR) -L$(LIBDIR) $(SRCDIR)/test/main.c $(TEST_BINARY_CFLAGS) -lfam_test -o $@

# Build coverage test library (static)
$(COV_TEST_LIB): $(COV_OBJECTS) | $(LIBDIR)
	ar rcs $@ $(COV_OBJECTS)

# Build coverage test binary
$(COV_TEST_BIN): $(COV_TEST_OBJ) $(COV_TEST_MAIN_OBJ) $(COV_TEST_LIB) | $(BINDIR)
	$(CC) -I$(INCLDIR) $(COV_TEST_BINARY_CFLAGS) $(COV_TEST_OBJ) $(COV_TEST_MAIN_OBJ) $(COV_TEST_LIB) -lgcov $(COV_LDFLAGS) -o $@

# Run tests
test: $(TEST_BIN)
	export TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Run coverage analysis
cov: $(COV_TEST_BIN)
	@rm -f $(COV_OBJDIR)/*.gcda $(COV_TOBJDIR)/*.gcda *.gcov
	@echo "Running runtests_cov to generate .gcda files..."

# Clean up
clean:
	rm -fr $(OBJDIR) $(TEST_OBJDIR) $(TOBJDIR) $(COV_OBJDIR) $(COV_TOBJDIR) $(LIBDIR)/*.so $(LIBDIR)/*.a $(BINDIR)/* *.gcno *.gcda *.gcov

# Phony targets
.PHONY: all test cov clean
