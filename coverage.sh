#!/bin/bash

# Coverage script for libfam project using GCC/gcov
# Compiles C files with --coverage, links object files directly, runs tests, and generates gcov reports

set -e  # Exit on error

# Directories
SRCDIR="src"
INCLDIR="src/include"
TOBJDIR=".tobj"
TEST_OBJDIR=".testobj"
BINDIR="bin"
COVDIR="coverage"

# Compiler and flags
CC="gcc"
CFLAGS="-DPAGE_SIZE=16384 -I${INCLDIR} -O0 -Wno-attributes -DMEMSAN=0 -DSTATIC= -g"
COVFLAGS="--coverage -O1"
LDFLAGS="--coverage -v"
LIBGCOV="/usr/lib/gcc/x86_64-linux-gnu/13/libgcov.a"

# Source files
TEST_SRC="src/core/test.c src/store/test.c src/net/test.c"
CORE_SRC=$(ls src/core/*.c | grep -v test.c)
SRC="${TEST_SRC} ${CORE_SRC}"

# Object files
TEST_OBJS=$(echo ${TEST_SRC} | sed "s|${SRCDIR}/|${TOBJDIR}/|g" | sed "s|\.c|\.cov.o|g")
LIB_OBJS=$(echo ${CORE_SRC} | sed "s|${SRCDIR}/|${TEST_OBJDIR}/|g" | sed "s|\.c|\.cov.o|g")

# Output binary
COV_BIN="${BINDIR}/runtests_cov"

# Clean up
echo "Cleaning up..."
rm -rf ${TOBJDIR} ${TEST_OBJDIR} ${BINDIR}/runtests_cov ${COVDIR} *.gcov

# Create directories
mkdir -p ${TOBJDIR}/core ${TOBJDIR}/store ${TOBJDIR}/net
mkdir -p ${TEST_OBJDIR}/core
mkdir -p ${BINDIR} ${COVDIR}

# Compile test source files
for src in ${TEST_SRC}; do
    obj=${src/${SRCDIR}/${TOBJDIR}}
    obj=${obj/.c/.cov.o}
    echo "Compiling ${src}..."
    ${CC} ${CFLAGS} ${COVFLAGS} -c ${src} -o ${obj}
done

# Compile library source files
for src in ${CORE_SRC}; do
    obj=${src/${SRCDIR}/${TEST_OBJDIR}}
    obj=${obj/.c/.cov.o}
    echo "Compiling ${src}..."
    ${CC} ${CFLAGS} ${COVFLAGS} -fPIC -c ${src} -o ${obj}
done

# Link test binary
echo "Linking ${COV_BIN}..."
${CC} ${LDFLAGS} ${TEST_OBJS} ${LIB_OBJS} -I${INCLDIR} src/test/main.c ${LIBGCOV} -lc -lgcc -o ${COV_BIN}

# Ensure write permissions
echo "Setting permissions..."
chmod -R u+rw ${TOBJDIR} ${TEST_OBJDIR} 2>/dev/null || true

# Run tests
echo "Running tests for coverage..."
./${COV_BIN} || { echo "Tests failed; check errors above"; exit 1; }

