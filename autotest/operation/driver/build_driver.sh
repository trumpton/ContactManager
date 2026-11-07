#!/bin/sh
# Builds the operation tests' driver (autotest/operation/driver/driver.cpp) against the application's objects.
#
# The application must already be built in BUILD (qmake6 and make, documents/headless-test.md section 1).  The driver is
# compiled with the same defines and include paths as the Makefile, and linked with every object of the build except
# main.o, so it always uses the current application code.  The application's own email and SMS are linked as they are:
# the operation tests send no messages (their expectations say so), and the announcement bench has its own stubs.
#
# usage: build_driver.sh <build folder> [output file]
set -e
BUILD="${1:?usage: build_driver.sh <build folder> [output file]}"
OUT="${2:-$BUILD/driver}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$HERE/../../.." && pwd)"          # the repository (the application's sources)
LIB="$(cd "$HERE/../../../../Lib" && pwd)"  # the Lib folder beside the repository
MK="$BUILD/Makefile"
[ -f "$MK" ] || { echo "no Makefile in $BUILD: build the application first" >&2; exit 1; }

var() { sed -n "s/^$1[[:space:]]*=[[:space:]]*//p" "$MK" | head -1; }
DEFINES="$(var DEFINES)"
INCPATH="$(var INCPATH)"
LIBS="$(var LIBS | sed "s/\$(SUBLIBS)//g")"   # SUBLIBS is empty for this application
LFLAGS="$(var LFLAGS)"

cd "$BUILD"
# QTest sends the key presses the driver uses.  The application does not use it, so its path is added here only.
QTEST_INC=/usr/include/x86_64-linux-gnu/qt6/QtTest
QTEST_LIB=/usr/lib/x86_64-linux-gnu/libQt6Test.so
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
eval g++ -std=c++17 -pipe -O2 -fPIC -D_REENTRANT $DEFINES $INCPATH -I"$QTEST_INC" -I"$SRC" -I"$LIB" \
     -c "$HERE/driver.cpp" -o "$WORK/driver.o"
OBJS=$(ls *.o | grep -v '^main\.o$')
eval g++ $LFLAGS -o "$OUT" "$WORK/driver.o" $OBJS $LIBS "$QTEST_LIB"
echo "built $OUT"
