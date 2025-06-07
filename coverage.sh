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
rm -rf ${TOBJDIR} ${TEST_OBJDIR} ${BINDIR}/runtests_cov ${COVDIR}/* *.gcov

# Create directories
mkdir -p ${TOBJDIR}/core ${TOBJDIR}/store ${TOBJDIR}/net
mkdir -p ${TEST_OBJDIR}/core
mkdir -p ${BINDIR}

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

cp -rp ${TEST_OBJDIR}/* ${COVDIR}

cd ${SRCDIR};
for dir in *; do
    if ls $dir/*.c > /dev/null 2>&1; then
        mkdir -p ../${COVDIR}/$dir
        cp $dir/*.c ../${COVDIR}/$dir
    fi
done
cd ..

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

    linesum=0;
    coveredsum=0;
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
            linessum=`awk "BEGIN {print $linessum + $lines}"`;
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

