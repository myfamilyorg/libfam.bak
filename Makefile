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
SRC_DIRS   = core store net crypto util lmdb

PREFIX ?= /usr
INSTALL_LIBDIR = $(PREFIX)/lib64
INCDIR = $(PREFIX)/include
INSTALL = install
INSTALL_LIB = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_DIR = $(INSTALL) -d


# Source and object files
C_SOURCES   = $(foreach dir,$(SRC_DIRS), \
	      $(filter-out $(SRCDIR)/$(dir)/test.c, \
	      $(wildcard $(SRCDIR)/$(dir)/*.c)))
OBJECTS	 = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SOURCES))
TEST_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(TEST_OBJDIR)/%.o,$(C_SOURCES))

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
COMMON_FLAGS =  -Wall \
		-Wextra \
		-std=c89 \
		-Werror \
		-I$(INCLDIR) \
		-DPAGE_SIZE=$(PAGE_SIZE) \
		-DMEMSAN=$(MEMSAN) \
		-Wno-pointer-sign \
		-Wno-builtin-declaration-mismatch \
		-Wno-nonnull-compare \
		-Wno-unknown-warning-option \
		-Wno-incompatible-library-redeclaration

# Arch specific flags
ARCH_FLAGS_x86_64 = -msse4.2
ARCH_FLAGS_aarch64 = -march=armv8-a+crc

# Detect architecture (default to x86_64)
ARCH = $(shell uname -m)
ifeq ($(ARCH),x86_64)
    ARCH_FLAGS = $(ARCH_FLAGS_x86_64)
else
    ARCH_FLAGS = $(ARCH_FLAGS_aarch64)
endif

# Specific flags
LIB_CFLAGS		= $(COMMON_FLAGS) $(ARCH_FLAGS) -fPIC -O3 -DSTATIC=static
TEST_CFLAGS	   = $(COMMON_FLAGS) $(ARCH_FLAGS) -fPIC -O1 -DSTATIC= -DTEST=1
TEST_BINARY_CFLAGS = $(COMMON_FLAGS) $(ARCH_FLAGS) -ffreestanding -nostdlib -O1 -DSTATIC= -DTEST=1
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
	$(CC) \
		$(TEST_OBJ) \
		-I$(INCLDIR) \
		-L$(LIBDIR) \
		-DTEST=1 \
		$(SRCDIR)/test/main.c \
		$(TEST_BINARY_CFLAGS) \
		-lfam_test -o $@

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

# Install rule
install: lib/libfam.so
	$(INSTALL_DIR) $(DESTDIR)$(INSTALL_LIBDIR)
	$(INSTALL_DIR) $(DESTDIR)$(INCDIR)
	$(INSTALL_LIB) lib/libfam.so $(INSTALL_LIBDIR)/libfam.so
	mkdir -p $(DESTDIR)$(INCDIR)/libfam
	$(INSTALL_DATA) src/include/libfam/*.H $(DESTDIR)$(INCDIR)/libfam

# Uninstall rule
uninstall:
	rm -f $(INSTALL_LIBDIR)/libfam.so
	rm -f $(DESTDIR)$(INCDIR)/libfam/*.H

# Phony targets
.PHONY: all test clean install uninstall
