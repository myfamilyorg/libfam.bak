#!/bin/bash

# Coverage script for libfam project using GCC/gcov
# Compiles C files with --coverage, links object files directly, runs tests, and generates gcov reports

set -e  # Exit on error

if [ "$NO_COLOR" = "" ]; then
   GREEN="\033[32m";
   CYAN="\033[36m";
   YELLOW="\033[33m";
   BRIGHT_RED="\033[91m";
   RESET="\033[0m";
   BLUE="\033[34m";
else
   GREEN="";
   CYAN="";
   YELLOW="";
   BRIGHT_RED="";
   RESET="";
   BLUE="";
fi

# Directories
SRCDIR="src"
INCLDIR="src/include"
TOBJDIR=".tobj"
TEST_OBJDIR=".testobj"
BINDIR="bin"
COVDIR=".cov"

# Compiler and flags
CC="gcc"
CFLAGS="-DPAGE_SIZE=16384 -I${INCLDIR} -O0 -Wno-attributes -DMEMSAN=0 -DSTATIC= -g"
COVFLAGS="--coverage -O1 -DTEST=1 -DCOVERAGE"
LDFLAGS="--coverage"
LIBCGCOV=""

# Source files
TEST_SRC="src/core/test.c src/store/test.c src/net/test.c src/crypto/test.c src/util/test.c"
# Include non-test .c files from core, store, and net
CORE_SRC=$(ls src/core/*.c src/store/*.c src/net/*.c src/crypto/*.c src/util/*.c 2>/dev/null | grep -v test.c)

# Object files
TEST_OBJS=$(echo ${TEST_SRC} | sed "s|${SRCDIR}/|${TOBJDIR}/|g" | sed "s|\.c|\.cov.o|g")
LIB_OBJS=$(echo ${CORE_SRC} | sed "s|${SRCDIR}/|${TEST_OBJDIR}/|g" | sed "s|\.c|\.cov.o|g")

# Output binary
COV_BIN="${BINDIR}/runtests_cov"

# Clean up
rm -rf ${TOBJDIR} ${TEST_OBJDIR} ${BINDIR}/runtests_cov ${COVDIR}/* *.gcov

# Create directories
mkdir -p ${TOBJDIR}/core ${TOBJDIR}/store ${TOBJDIR}/net ${TOBJDIR}/crypto ${TOBJDIR}/util
mkdir -p ${TEST_OBJDIR}/core ${TEST_OBJDIR}/store ${TEST_OBJDIR}/net ${TEST_OBJDIR}/crypto ${TEST_OBJDIR}/util

mkdir -p ${BINDIR}

# Compile test source files
for src in ${TEST_SRC}; do
    obj=${src/${SRCDIR}/${TOBJDIR}}
    obj=${obj/.c/.cov.o}
    ${CC} ${CFLAGS} ${COVFLAGS} -c ${src} -o ${obj}
done

# Compile library source files
for src in ${CORE_SRC}; do
    obj=${src/${SRCDIR}/${TEST_OBJDIR}}
    obj=${obj/.c/.cov.o}
    COMMAND="${CC} ${CFLAGS} ${COVFLAGS} -fPIC -c ${src} -o ${obj}"
    echo ${COMMAND}
    ${COMMAND}
done

# Link test binary
COMMAND="${CC} ${LDFLAGS} ${TEST_OBJS} ${LIB_OBJS} -I${INCLDIR} src/test/main.c ${LIBGCOV} -lc -lgcc -o ${COV_BIN} -DCOVERAGE"
echo ${COMMAND}
${COMMAND}

# Run tests
export TEST_PATTERN="*"; # ensure test pattern is set for tests
export SHARED_MEMORY_BYTES="67108864"; # use invalid size to trigger default
./${COV_BIN} || { echo "Tests failed; check errors above"; exit 1; }

# Copy object files for coverage
mkdir -p ${COVDIR}
cp -rp ${TEST_OBJDIR}/* ${COVDIR}

# Copy source files for coverage analysis
cd ${SRCDIR}
for dir in *; do
    if ls $dir/*.c > /dev/null 2>&1; then
        mkdir -p ../${COVDIR}/$dir
        cp $dir/*.c ../${COVDIR}/$dir
    fi
done
cd ..

echo "------------------------------------------------------------------------------------------";
linesum=0;
coveredsum=0;


cd ${COVDIR}
for dir in *; do
    cd $dir
    if ls *.o > /dev/null 2>&1; then
        for file in *.o; do
            mv $file ${file%.cov.o}.o
        done
    fi
    if ls *.gcda > /dev/null 2>&1; then
        for file in *.gcda; do
            mv $file ${file%.cov.gcda}.gcda
        done
    fi
    if ls *.gcno > /dev/null 2>&1; then
        for file in *.gcno; do
            mv $file ${file%.cov.gcno}.gcno
        done
    fi

    for file in *.c; do
        if [ -f "$file" ] && [ "test.c" != "$file" ] && [ "main.c" != "$file" ]; then
            cd ../..

            percent=`gcov ${COVDIR}/$dir/$file 2> /dev/null | grep "^Lines" | head -1 | cut -f2 -d ' ' | cut -f2 -d ':' | cut -f1 -d '%' | tr -d \\n`;
            if [ "$percent" == "" ]; then
                percent=0.00;
            fi;
            lines=`gcov ${COVDIR}/$dir/$file 2> /dev/null | grep "^Lines" | head -1 | cut -f4 -d ' ' | tr -d \\n`;
	    if [ "$lines" == "" ]; then
                lines=0;
	    fi

            BASENAME=$file
            ratio=`awk "BEGIN {print $percent / 100}"`;
            covered=`awk "BEGIN {print int($ratio * $lines)}"`;
            linesum=`awk "BEGIN {print $linesum+ $lines}"`;
            coveredsum=`awk "BEGIN {print $coveredsum + $covered}"`;
	    printf "${GREEN}%-20s${RESET} %6s%% -${YELLOW} [%3s/%3s]${RESET}\n" "${BASENAME}" "${percent}" "${covered}" "${lines}"

            if ls *.gcov > /dev/null 2>&1; then
                mv *.gcov ${COVDIR}/$dir
            fi
            cd ${COVDIR}/$dir
        fi
    done
    cd ..
done

echo "------------------------------------------------------------------------------------------";
codecov=`awk "BEGIN {printf \"%.2f\", 100 * $coveredsum / $linesum}"`
echo "Coverage: ${codecov}% [$coveredsum / $linesum]";

timestamp=`date +%s`
echo "$codecov" > /tmp/cc_final;
echo "$timestamp $codecov $coveredsum $linesum" > /tmp/cc.txt

