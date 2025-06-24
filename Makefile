# Compiler and tools
CC = gcc

# Directories
SRCDIR	 = src
INCLDIR	= src/include
LIBDIR	 = lib
BINDIR	 = bin
OBJDIR	 = .obj
TEST_OBJDIR = .testobj
TOBJDIR	= .tobj
ASM_DIR	= $(SRCDIR)/asm
SRC_DIRS   = core store net crypto util

# Source and object files
C_SOURCES   = $(foreach dir,$(SRC_DIRS),$(filter-out $(SRCDIR)/$(dir)/test.c,$(wildcard $(SRCDIR)/$(dir)/*.c)))
ASM_SOURCES = $(wildcard $(ASM_DIR)/*.S)
OBJECTS	 = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SOURCES)) $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.o,$(ASM_SOURCES))
TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(TEST_OBJDIR)/%.o,$(C_SOURCES)) $(patsubst $(SRCDIR)/%.S,$(TEST_OBJDIR)/%.o,$(ASM_SOURCES))

# Test sources and objects
TEST_SRC	= $(foreach dir,$(SRC_DIRS),$(SRCDIR)/$(dir)/test.c)
TEST_OBJ	= $(patsubst $(SRCDIR)/%.c,$(TOBJDIR)/%.o,$(TEST_SRC))
TEST_LIB	= $(LIBDIR)/libfam_test.so
TEST_BIN	= $(BINDIR)/runtests

# Common configuration
PAGE_SIZE   = 16384
MEMSAN	 ?= 0
FILTER	 ?= "*"

# Common flags
COMMON_FLAGS =             -Wall \
			   -Wextra \
			   -std=c89 \
			   -I$(INCLDIR) \
			   -D_FORTIFY_SOURCE=0 \
			   -fno-builtin \
			   -fno-stack-protector \
			   -Wno-attributes \
			   -Wno-error=pointer-sign \
			   -Wno-pointer-to-int-cast \
			   -Wno-int-to-pointer-cast \
			   -Wno-incompatible-pointer-types-discards-qualifiers \
			   -Wno-discarded-qualifiers \
                           -Wno-int-conversion \
			   -Wno-pointer-sign \
			   -DPAGE_SIZE=$(PAGE_SIZE) \
			   -DMEMSAN=$(MEMSAN)

# Specific flags
LIB_CFLAGS		= $(COMMON_FLAGS) -fPIC -O3 -DSTATIC=static
TEST_CFLAGS	   = $(COMMON_FLAGS) -fPIC -O1 -DSTATIC= -DTEST=1
TEST_BINARY_CFLAGS = $(COMMON_FLAGS) -ffreestanding -nostdlib -O1 -DSTATIC= -DTEST=1
LDFLAGS		   = -shared -nostdlib -ffreestanding

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

# Build main library
$(LIBDIR)/libfam.so: $(OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# Build test library
$(TEST_LIB): $(TEST_OBJECTS) | $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJECTS)

# Build test binary
$(TEST_BIN): $(TEST_OBJ) $(TEST_LIB) | $(BINDIR)
	$(CC) $(TEST_OBJ) -I$(INCLDIR) -L$(LIBDIR) $(SRCDIR)/test/main.c $(TEST_BINARY_CFLAGS) -lfam_test -o $@

# Run tests
test: $(TEST_BIN)
	export TEST_PATTERN=$(FILTER); LD_LIBRARY_PATH=$(LIBDIR) $(TEST_BIN)

# Clean up
clean:
	rm -fr $(OBJDIR) \
	$(TEST_OBJDIR) \
	$(TOBJDIR) \
	$(COV_OBJDIR) \
	$(COV_TOBJDIR) \
	$(LIBDIR)/*.so \
	$(LIBDIR)/*.a \
	$(BINDIR)/* \
	*.gcno *.gcda *.gcov

# Phony targets
.PHONY: all test clean
