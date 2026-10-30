#!/bin/sh
#
# Builds and runs the message and appointment tests (tst_messages.cpp).
#
#   autotest/messages/run-messages-tests.sh <build-folder>
#
# The build folder is the one ContactManager was built in (qmake6 ContactManager.pro, then make).  The tests link with every
# object file there except main.o, so they use the application's own code.  They run headless, in a new empty home folder,
# so no real settings or contacts are read or written.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${1:?usage: run-messages-tests.sh <build-folder>}
build=$(cd "$build" && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

moc="$(qmake6 -query QT_INSTALL_LIBEXECS)/moc"
qtcflags=$(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Multimedia Qt6Test Qt6Core5Compat)
qtlibs=$(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Multimedia Qt6Test Qt6Core5Compat)

# The test's moc output, which the test includes at its end, then the test itself
"$moc" "$here/tst_messages.cpp" -o "$work/tst_messages.moc"
g++ -std=gnu++17 -fPIC -DQT_NO_DEBUG \
    -I"$repo" -I"$repo/../Lib" -I"$build" -I"$here" -I"$work" $qtcflags \
    -c "$here/tst_messages.cpp" -o "$work/tst_messages.o"

# Every application object except its main
objects=""
for o in "$build"/*.o; do
    case "$(basename "$o")" in
        main.o) ;;
        *) objects="$objects $o" ;;
    esac
done

g++ -o "$work/tst_messages" "$work/tst_messages.o" $objects $qtlibs -lpthread -lGLX -lOpenGL

# Headless, in an empty home folder
mkdir -p "$work/home"
cd "$work"
env HOME="$work/home" QT_QPA_PLATFORM=offscreen "$work/tst_messages" -o -,txt
