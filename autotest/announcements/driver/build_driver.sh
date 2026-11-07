#!/bin/sh
# Builds the announcements bench's driver and its clock (autotest/announcements).
#
# The driver is the application's objects, linked as in the Google bench's build (autotest/googlesync/driver/build_driver.sh),
# with two differences.  The email and SMS objects are left out, and the bench's stubs are linked in their place
# (stubs/email_stub.cpp and stubs/sms_stub.cpp), so nothing is sent to a server or a text service.  The clock is built as
# a separate shared library (clock/fakeclock.c), which the runner preloads.
#
# The application must already be built in BUILD (qmake6 and make, documents/headless-test.md section 1).  Nothing in the
# application's sources is changed: the stubs are test substitutes compiled from autotest/announcements only.
#
# usage: build_driver.sh <build folder> [output folder]
#   writes <output folder>/driver and <output folder>/fakeclock.so (default output folder: the build folder)
set -e
BUILD="${1:?usage: build_driver.sh <build folder> [output folder]}"
OUTDIR="${2:-$BUILD}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BENCH="$(cd "$HERE/.." && pwd)"
SRC="$(cd "$BENCH/../.." && pwd)"          # the repository (the application's sources)
LIB="$(cd "$BENCH/../../../Lib" && pwd)"    # the Lib folder beside the repository
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

# The driver and the two stubs are compiled with the application's defines and include paths
eval g++ -std=c++17 -pipe -O2 -fPIC -D_REENTRANT $DEFINES $INCPATH -I"$QTEST_INC" -I"$SRC" -I"$LIB" \
     -c "$BENCH/driver/driver.cpp" -o "$WORK/driver.o"
eval g++ -std=c++17 -pipe -O2 -fPIC -D_REENTRANT $DEFINES $INCPATH -I"$SRC" -I"$LIB" \
     -c "$BENCH/stubs/email_stub.cpp" -o "$WORK/email_stub.o"
eval g++ -std=c++17 -pipe -O2 -fPIC -D_REENTRANT $DEFINES $INCPATH -I"$SRC" -I"$LIB" \
     -c "$BENCH/stubs/sms_stub.cpp" -o "$WORK/sms_stub.o"

# Every object of the application except main.o (the driver has its own main), email.o and sms.o (the stubs replace them)
OBJS=$(ls *.o | grep -v -e '^main\.o$' -e '^email\.o$' -e '^sms\.o$')
eval g++ $LFLAGS -o "$OUTDIR/driver" "$WORK/driver.o" "$WORK/email_stub.o" "$WORK/sms_stub.o" $OBJS $LIBS "$QTEST_LIB"
echo "built $OUTDIR/driver"

# The clock: a shared library, preloaded by the runner
gcc -shared -fPIC -O2 -o "$OUTDIR/fakeclock.so" "$BENCH/clock/fakeclock.c" -ldl
echo "built $OUTDIR/fakeclock.so"
